#!@python@/bin/python3
"""Configure a rootless, reproducible Containerlab environment.

The environment is reproducible through Nix and rootless through user
namespaces. It is built in one shot:

  - A parent process stays in the *initial* user/network/pid namespaces. It
    owns the /etc/sub{u,g}id allocation and keeps a foot in the root network
    namespace, so it can map the child and run pasta from there.
  - A child unshares every namespace at once (user, mount, pid, uts, ipc, net,
    cgroup).
  - The parent maps the child's uid/gid ranges (newuidmap/newgidmap) and runs
    pasta from the root network namespace, targeting the child's network
    namespace by its procfs path, to wire up networking.
  - The child then forks (to land PID 1 in the new pid namespace), sets up the
    mount namespace, and execs tmux, which in turn re-runs this script's
    `tmux-init` step to spawn the shell.

"""

import argparse
import ctypes
import ctypes.util
import enum
import os
import platform
import pwd
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Literal, NamedTuple

from rich.console import Console

# Nix store paths, substituted at build time.
SANDBOX = "@sandbox@"
PASST = "@passt@"
NCURSES = "@ncurses@"
CACERT = "@cacert@"

NAMESPACES = (
    os.CLONE_NEWUSER
    | os.CLONE_NEWNS
    | os.CLONE_NEWPID
    | os.CLONE_NEWUTS
    | os.CLONE_NEWIPC
    | os.CLONE_NEWNET
    | os.CLONE_NEWCGROUP
)


class MS(enum.IntFlag):
    """mount(2) flags"""

    RDONLY = enum.auto()
    NOSUID = enum.auto()
    NODEV = enum.auto()
    NOEXEC = enum.auto()
    SYNCHRONOUS = enum.auto()
    REMOUNT = enum.auto()
    MANDLOCK = enum.auto()
    DIRSYNC = enum.auto()
    NOSYMFOLLOW = enum.auto()
    _ = enum.auto()
    NOATIME = enum.auto()
    NODIRATIME = enum.auto()
    BIND = enum.auto()
    MOVE = enum.auto()
    REC = enum.auto()
    SILENT = enum.auto()
    POSIXACL = enum.auto()
    UNBINDABLE = enum.auto()
    PRIVATE = enum.auto()
    SLAVE = enum.auto()
    SHARED = enum.auto()
    RELATIME = enum.auto()


class MNT(enum.IntFlag):
    """umount2(2) flags."""

    FORCE = enum.auto()
    DETACH = enum.auto()
    EXPIRE = enum.auto()


class SubId(NamedTuple):
    """A subordinate id range from /etc/subuid or /etc/subgid."""

    start: int
    size: int


# Flags the kernel locks on mounts inherited into a user namespace; a
# read-only remount must carry them over or it fails with EPERM.
LOCKED_FLAGS = (
    (os.ST_NOSUID, MS.NOSUID),
    (os.ST_NODEV, MS.NODEV),
    (os.ST_NOEXEC, MS.NOEXEC),
    (os.ST_NOATIME, MS.NOATIME),
    (os.ST_NODIRATIME, MS.NODIRATIME),
    (os.ST_RELATIME, MS.RELATIME),
)

# pivot_root(2) has no glibc wrapper, so go through syscall().
SYS_PIVOT_ROOT = {"x86_64": 155, "aarch64": 41}[platform.machine()]

console = Console(stderr=True)

_libc = ctypes.CDLL(ctypes.util.find_library("c"), use_errno=True)
_libc.mount.argtypes = [
    ctypes.c_char_p,
    ctypes.c_char_p,
    ctypes.c_char_p,
    ctypes.c_ulong,
    ctypes.c_char_p,
]
_libc.umount2.argtypes = [ctypes.c_char_p, ctypes.c_int]
_libc.sethostname.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
_libc.syscall.restype = ctypes.c_long


def log(msg: str, **fields: object) -> None:
    """Log a timestamped message with optional structured fields."""
    extra = "".join(f" [dim]{k}=[/]{v}" for k, v in fields.items())
    console.log(msg + extra)


def _enc(value: str | None) -> bytes | None:
    """Encode a string for libc, passing None through."""
    return value.encode() if value else None


def _check(ret: int, what: str) -> None:
    """Raise OSError from errno when a libc call reports failure (nonzero)."""
    if ret != 0:
        err = ctypes.get_errno()
        raise OSError(err, os.strerror(err), what)


def mount(
    source: str | None,
    target: str,
    fstype: str | None,
    flags: MS = MS(0),
    data: str | None = None,
) -> None:
    """Wrap the mount(2) syscall, raising OSError on failure."""
    _check(
        _libc.mount(_enc(source), _enc(target), _enc(fstype), flags, _enc(data)),
        f"{source} -> {target}",
    )


def sethostname(name: str) -> None:
    """Wrap the sethostname(2) syscall, raising OSError on failure."""
    raw = name.encode()
    _check(_libc.sethostname(raw, len(raw)), name)


def umount(target: str, flags: MNT = MNT(0)) -> None:
    """Wrap the umount2(2) syscall, raising OSError on failure."""
    _check(_libc.umount2(_enc(target), flags), target)


def pivot_root(new_root: str, put_old: str) -> None:
    """Wrap the pivot_root(2) syscall, raising OSError on failure."""
    _check(
        _libc.syscall(SYS_PIVOT_ROOT, _enc(new_root), _enc(put_old)),
        f"{new_root} -> {put_old}",
    )


def bind(source: Path, target: Path, ro: bool = False, optional: bool = False) -> None:
    """Recursively bind-mount source onto target, creating the mountpoint.

    With ro, remount read-only afterwards. With optional, silently skip a
    missing source.
    """
    if optional and not source.exists():
        return
    if not target.exists():  # the mountpoint may already exist under another bind
        if source.is_dir():
            target.mkdir(parents=True, exist_ok=True)
        else:
            target.parent.mkdir(parents=True, exist_ok=True)
            target.touch()
    mount(str(source), str(target), None, MS.BIND | MS.REC)
    if ro:
        flags = MS.BIND | MS.REMOUNT | MS.RDONLY
        st_flag = os.statvfs(target).f_flag
        for st_bit, ms_bit in LOCKED_FLAGS:
            if st_flag & st_bit:
                flags |= ms_bit
        mount(None, str(target), None, flags)


def write(path: Path, content: str, mode: int = 0o644) -> None:
    """Write content to path with the given mode."""
    path.write_text(content)
    path.chmod(mode)


def subid_range(path: str, user: str, uid: int) -> SubId:
    """Return the first subordinate range owned by the user.

    Parses an /etc/subuid or /etc/subgid file (both keyed by user).
    """
    keys = {user, str(uid)}
    for line in Path(path).read_text().splitlines():
        who, start, count = line.split(":")
        if who in keys:
            return SubId(int(start), int(count))
    raise RuntimeError(f"no subordinate id range for {user} in {path}")


def map_ids(kind: Literal["uid", "gid"], pid: int, host_id: int, sub: SubId) -> None:
    """Map id 0 to us plus the subordinate range, like `unshare --map-*=auto`.

    Mapping a range requires the setuid newuidmap/newgidmap helpers, so this
    runs from the parent (real uid, owner of the /etc/sub{u,g}id allocation)
    against the freshly unshared child. We need to use the helper from the host
    as the one from Nix is not setuid.

    """
    tool = f"new{kind}map"
    subprocess.run(
        [tool, str(pid), "0", str(host_id), "1", "1", str(sub.start), str(sub.size)],
        check=True,
    )


def build_workspace(
    home_src: Path,
    labname: str,
    shell: str,
    subuid: SubId,
    subgid: SubId,
) -> Path:
    """Create the throwaway /etc files, home, and SSH key, returning the dir."""
    ws = Path(tempfile.mkdtemp(prefix="clab-"))
    home = ws / "home"
    (home / ".ssh").mkdir(parents=True)
    (home / ".ssh").chmod(0o700)

    storage = home_src / ".local/share/containers/storage"
    storage.mkdir(parents=True, exist_ok=True)

    write(ws / "passwd", f"root:x:0:0:root:/home/lab:{shell}\n")
    write(ws / "group", "root:x:0:\n")
    write(ws / "subuid", f"root:1:{subuid.size}\n")
    write(ws / "subgid", f"root:1:{subgid.size}\n")
    write(
        ws / "hosts",
        f"""\
127.0.0.1 localhost.localdomain localhost
127.0.1.1 {labname}.home.arpa {labname}
::1       ip6-localhost ip6-loopback
""",
    )
    write(
        ws / "nsswitch.conf",
        """\
passwd: compat
group:  compat
shadow: compat

hosts:    files dns
networks: files

protocols: db files
services:  db files
ethers:    db files
rpc:       db files
""",
    )
    write(
        ws / "resolv.conf",
        """\
nameserver 8.8.8.8
search clab
""",
    )
    write(
        ws / "storage.conf",
        f"""\
[storage]
driver = "overlay"
graphroot = "{storage}"
""",
    )
    write(
        ws / "policy.json",
        """\
{
    "default": [
        {
            "type": "insecureAcceptAnything"
        }
    ]
}
""",
    )

    with console.status("Creating an SSH key...", spinner="dots"):
        subprocess.run(
            [
                f"{SANDBOX}/bin/ssh-keygen",
                "-q",
                "-t",
                "rsa",
                "-f",
                str(home / ".ssh/id_rsa"),
                "-N",
                "",
            ],
            check=True,
        )

    log("Workspace ready", dir=ws)
    return ws


def launch() -> None:
    """Fork into the parent (maps ids, runs pasta) and the child (unshares)."""
    uid, gid = os.getuid(), os.getgid()
    home_src = Path.home()
    project = Path.cwd()
    labname = project.name
    passwd = pwd.getpwuid(uid)
    shell = (
        f"{SANDBOX}/bin/zsh"
        if passwd.pw_shell.endswith("zsh")
        else f"{SANDBOX}/bin/bash"
    )
    subuid = subid_range("/etc/subuid", passwd.pw_name, uid)
    subgid = subid_range("/etc/subgid", passwd.pw_name, uid)
    log("Setting up environment", lab=labname, shell=shell)

    ws = build_workspace(home_src, labname, shell, subuid, subgid)
    pasta_log = ws / "pasta.log"

    ready_r, ready_w = os.pipe()  # child -> parent: namespaces created
    go_r, go_w = os.pipe()  # parent -> child: ids mapped and pasta configured

    child = os.fork()
    if child:
        # Parent: stays put in the initial namespaces. It owns the subordinate
        # id allocation, so it maps the child's user namespace, then runs pasta
        # from the root network namespace, reaching the child by procfs path.
        os.close(ready_w)
        os.close(go_r)
        os.read(ready_r, 1)
        log("Mapping ids", child=child, subuid=subuid, subgid=subgid)
        map_ids("uid", child, uid, subuid)
        map_ids("gid", child, gid, subgid)
        log("Configuring network with pasta", netns=child)
        subprocess.run(
            [
                f"{PASST}/bin/pasta",
                "--config-net",
                "--netns",
                f"/proc/{child}/ns/net",
                "--userns",
                f"/proc/{child}/ns/user",
                "--hostname",
                labname,
                "-l",
                str(pasta_log),
                "-t",
                "none",
                "-u",
                "none",
            ],
            check=True,
        )
        os.write(go_w, b"\0")
        os.close(go_w)
        _, status = os.waitpid(child, 0)
        sys.exit(os.waitstatus_to_exitcode(status))

    # Child: unshare everything at once; the parent maps our ids and wires up
    # the network, then releases us through the go pipe.
    os.close(ready_r)
    os.close(go_w)
    os.unshare(NAMESPACES)
    log("Unshared namespaces")
    os.write(ready_w, b"\0")
    os.close(ready_w)
    os.read(go_r, 1)  # wait for the parent to map ids and configure the network
    os.close(go_r)

    # CLONE_NEWPID only takes effect for children, so fork to land PID 1.
    grand = os.fork()
    if grand:
        _, status = os.waitpid(grand, 0)
        sys.exit(os.waitstatus_to_exitcode(status))

    enter_namespace(ws, home_src, project, shell, labname, pasta_log)


def enter_namespace(
    ws: Path,
    home_src: Path,
    project: Path,
    shell: str,
    labname: str,
    pasta_log: Path,
) -> None:
    """As PID 1 in the new namespaces, build a fresh root and exec tmux."""
    log("Setting up mount namespace", host=labname)
    new_root = ws / "root"
    new_root.mkdir()

    def at(path: str) -> Path:
        return new_root / path.lstrip("/")

    def tmpfs(path: str, mode: str | None = None) -> None:
        target = at(path)
        target.mkdir(parents=True, exist_ok=True)
        mount("tmpfs", str(target), "tmpfs", data=f"mode={mode}" if mode else None)

    # A private tree holding a fresh tmpfs root we can pivot into.
    mount(None, "/", None, MS.REC | MS.PRIVATE)
    mount("tmpfs", str(new_root), "tmpfs")

    # System: Nix store (read-only), devices, kernel filesystems. /dev/shm gets
    # a fresh tmpfs (the bound host one is owned by real root). The empty dirs
    # below need no tmpfs of their own: the root is already one.
    bind(Path("/nix"), at("/nix"), ro=True)
    bind(Path("/dev"), at("/dev"))
    tmpfs("/dev/shm", mode="1777")
    proc = at("/proc")
    proc.mkdir()
    mount("proc", str(proc), "proc", MS.NOSUID | MS.NODEV | MS.NOEXEC)
    for d in ("/tmp", "/run", "/var/tmp", "/var/log", "/etc/ssh/ssh_config.d"):
        at(d).mkdir(parents=True, exist_ok=True)
    bind(Path("/sys"), at("/sys"))

    # /etc: our throwaway files (copied in, as the root is a separate tmpfs),
    # plus an mtab.
    for name, target in (
        ("passwd", "/etc/passwd"),
        ("group", "/etc/group"),
        ("hosts", "/etc/hosts"),
        ("nsswitch.conf", "/etc/nsswitch.conf"),
        ("resolv.conf", "/etc/resolv.conf"),
        ("subuid", "/etc/subuid"),
        ("subgid", "/etc/subgid"),
        ("storage.conf", "/etc/containers/storage.conf"),
        ("policy.json", "/etc/containers/policy.json"),
    ):
        dst = at(target)
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy(ws / name, dst)
    os.symlink("../proc/self/mounts", at("/etc/mtab"))

    # Home, lab project directory, and persistent podman storage (at its real
    # path so it survives across runs).
    bind(ws / "home", at("/home/lab"))
    bind(home_src / ".tmux.conf", at("/home/lab/.tmux.conf"), ro=True, optional=True)
    bind(project, at("/lab"))
    containers = home_src / ".local/share/containers"
    bind(containers, at(str(containers)))

    # Shell dotfiles (read-only). The Zsh setup keeps a writable run dir with
    # the user's socket exposed through it.
    if shell.endswith("zsh"):
        bind(home_src / ".zsh", at("/home/lab/.zsh"), ro=True, optional=True)
        bind(home_src / ".zshrc", at("/home/lab/.zshrc"), ro=True, optional=True)
        bind(home_src / ".zshenv", at("/home/lab/.zshenv"), ro=True, optional=True)
        tmpfs(str(home_src / ".zsh/run"))
        bind(home_src / ".zsh/run/u", at("/home/lab/.zsh/run/u"), optional=True)
    else:
        bind(home_src / ".bashrc", at("/home/lab/.bashrc"), ro=True, optional=True)
        bind(
            home_src / ".bash_profile",
            at("/home/lab/.bash_profile"),
            ro=True,
            optional=True,
        )

    # pasta logs to a host path; expose it so the log pane can tail it.
    bind(pasta_log, at("/var/log/pasta.log"))

    # Pivot into the new root and detach the old one.
    at("/oldroot").mkdir()
    os.chdir(new_root)
    pivot_root(".", "oldroot")
    os.chroot(".")
    umount("/oldroot", MNT.DETACH)
    os.rmdir("/oldroot")

    sethostname(labname)
    os.chdir("/lab")

    # Start from an empty environment (clearenv) and keep only what we need,
    # passing TERM through from the host.
    env = {
        "PATH": f"{SANDBOX}/bin",
        "HOME": "/home/lab",
        "SHELL": shell,
        "TERM": os.environ.get("TERM", "xterm"),
        "TERMINFO": f"{NCURSES}/share/terminfo",
        "SSL_CERT_FILE": f"{CACERT}/etc/ssl/certs/ca-bundle.crt",
        "LANG": "C.utf8",
        "CLAB_RUNTIME": "podman",
        "CLAB_VERSION_CHECK": "disable",
    }
    if shell.endswith("zsh"):
        env["VBE_SHELL_ISOLATED"] = "true"

    log("Starting tmux", session=labname)
    script = str(Path(__file__).resolve())
    os.execve(
        f"{SANDBOX}/bin/tmux",
        [
            "tmux",
            "-L",
            labname,
            "new-session",
            "-s",
            labname,
            "-n",
            "main",
            sys.executable,
            script,
            "tmux-init",
            "--shell",
            shell,
            "--pasta-log",
            "/var/log/pasta.log",
        ],
        env,
    )


def tmux_init(shell: str, pasta_log: str) -> None:
    """Configure the tmux session, add the services window, and spawn the shell."""
    tmux = f"{SANDBOX}/bin/tmux"

    def run(*argv: str) -> None:
        subprocess.run([tmux, *argv], check=True)

    # podman
    run(
        "new-window",
        "-n",
        "services",
        f"{SANDBOX}/bin/podman --log-level info system service --time=0",
    )
    run("select-pane", "-T", "podman")

    # pasta logs
    run(
        "split-window",
        "-v",
        "-t",
        "services",
        f"{SANDBOX}/bin/tail -n+0 -F {pasta_log}",
    )
    run("select-pane", "-T", "pasta logs")

    # label the panes, only on the services window
    run("set-option", "-w", "-t", "services", "pane-border-status", "top")
    run("set-option", "-w", "-t", "services", "pane-border-format", "#T")

    log("Spawning shell", shell=shell)
    run("select-window", "-t", "main")
    run("set-option", "prefix", "C-b")
    os.execve(shell, [Path(shell).name, "-i"], {**os.environ, "SHLVL": "0"})


def main() -> None:
    """Dispatch to the launcher or the in-tmux setup step."""
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command")
    sub.add_parser("launch", help="set up the environment and spawn the shell")
    init = sub.add_parser("tmux-init", help="configure tmux and spawn the shell")
    init.add_argument("--shell", required=True)
    init.add_argument("--pasta-log", required=True)
    parser.set_defaults(command="launch")
    args = parser.parse_args()

    match args.command:
        case "launch":
            launch()
        case "tmux-init":
            tmux_init(args.shell, args.pasta_log)


if __name__ == "__main__":
    main()
