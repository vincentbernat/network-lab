#!/usr/bin/env bash

# This script configures the environment to run Containerlab. The environment is
# reproducible (through Nix) and rootless (through bwrap). This is done in
# several stages:
#
# - Stage 1. Setup the mount namespace to ensure we use Nix for all dependencies.
# - Stage 2. Setup the network namespace with pasta (as well as user, ipc, pid, uts).
# - Stage 3. Setup the remaining namespaces (cgroup) to complete isolation.
# - Stage 4. Run tmux.

set -euo pipefail

PATH=@sandbox@/bin

gum=@gum@/bin/gum
progress() {
  local title="$1"
  shift
  $gum spin --spinner points --title "$title" --show-error -- "$@"
}
log() {
  local level="$1"
  shift
  $gum log --time=datetime --structured --level "$level" "$@"
}

run_stage1() {
  # Stage 1 configures the mount namespace (and uname).
  log info "Setup stage 1..."
  log debug "Setup is @setup@"
  labname=${PWD##*/}

  # Create the temporary workspace
  workspace=$(mktemp -d)
  trap "rm -rf $workspace" EXIT

  # Set the SHELL variable matching the current user.
  case $(getent passwd $USER) in
    */zsh) SHELL=@sandbox@/bin/zsh ;;
    *) SHELL=@sandbox@/bin/bash ;;
  esac
  log debug "Shell is $SHELL"

  # Create some system files
  log debug "Creating workspace..." directory "$workspace"
  cat > "$workspace/passwd" <<EOF
root:x:0:0:root:/home/lab:$SHELL
EOF
  cat > "$workspace/group" <<EOF
root:x:0:
EOF
  cat > "$workspace/hosts" <<EOF
127.0.0.1 localhost.localdomain localhost
127.0.1.1 $labname.home.arpa $labname
::1       ip6-localhost ip6-loopback
EOF
  cat > "$workspace/nsswitch.conf" <<EOF
passwd: compat
group:  compat
shadow: compat

hosts:    files dns
networks: files

protocols: db files
services:  db files
ethers:    db files
rpc:       db files
EOF
  cat > "$workspace/resolv.conf" <<EOF
nameserver 8.8.8.8
search clab
EOF
  cat > "$workspace/containers-storage.conf" <<EOF
[storage]
driver = "overlay"
graphroot = "$HOME/.local/share/containers/storage"

[storage.options]
# additionalimagestores = ["$HOME/.local/share/containers/storage"]

[storage.options.overlay]
mount_program = "@fuse-overlayfs@/bin/fuse-overlayfs"
mountopt = "uidmapping=0:$(id -u):1,gidmapping=0:$(id -g):1"
skip_mount_home = "true"
EOF


  # Create an SSH key
  install -m 0700 -d "$workspace/home/.ssh"
  progress "Creating an SSH key..." ssh-keygen -q -t rsa -f "$workspace/home/.ssh/id_rsa" -N ""
  log debug "SSH key created"

  # Prepare bubblewrap
  declare -a args
  args=(
    # UTS
    --unshare-uts
    --hostname $labname
    # Home and lab
    --bind "$workspace/home" /home/lab
    --ro-bind-try "$HOME/.tmux.conf" /home/lab/.tmux.conf
    --bind "$PWD" /lab
    # System
    --ro-bind /nix /nix
    --dev-bind /dev /dev
    --proc /proc
    --tmpfs /tmp
    --tmpfs /run
    --tmpfs /var/log
    --ro-bind "$workspace/passwd" /etc/passwd
    --ro-bind "$workspace/group" /etc/group
    --ro-bind "$workspace/hosts" /etc/hosts
    --ro-bind "$workspace/nsswitch.conf" /etc/nsswitch.conf
    --ro-bind "$workspace/resolv.conf" /etc/resolv.conf
    --ro-bind "$workspace/containers-storage.conf" /etc/containers/storage.conf
    --tmpfs /etc/ssh/ssh_config.d
    # Environment
    --clearenv
    --setenv PATH "@sandbox@/bin"
    --setenv HOME "/home/lab"
    --setenv SHELL "$SHELL"
    --setenv TERM "$TERM"
    --setenv TERMINFO "@ncurses@/share/terminfo"
    --setenv SSL_CERT_FILE "@cacert@/etc/ssl/certs/ca-bundle.crt"
    --setenv LANG "C.utf8"
    --setenv STAGE 2
    --chdir "/lab"
    # Podman: we need to bind at the same location
    --bind "$HOME/.local/share/containers" "$HOME/.local/share/containers"
    --die-with-parent
  )
  case $SHELL in
    */zsh)
      args+=(
        --ro-bind-try "$HOME/.zsh" /home/lab/.zsh
        --ro-bind-try "$HOME/.zshrc" /home/lab/.zshrc
        --ro-bind-try "$HOME/.zshenv" /home/lab/.zshenv
        # This is specific to my Zsh setup
        --tmpfs "$HOME/.zsh/run"
        --bind-try "$HOME/.zsh/run/u" /home/lab/.zsh/run/u
        --setenv VBE_SHELL_ISOLATED true
      )
      ;;
    *)
      args+=(
        --ro-bind-try "$HOME/.bashrc" /home/lab/.bashrc
        --ro-bind-try "$HOME/.bash_profile" /home/lab/.bash_profile
      )
      ;;
  esac

  @bubblewrap@/bin/bwrap ''${args[@]} @setup@
}

run_stage2() {
  # Stage 2 runs pasta.
  log info "Prepare stage 2..."

  log debug "Run pasta..."
  mkdir -p $HOME/tmp
  env STAGE=3 @passt@/bin/pasta \
    -l /var/log/pasta.log \
    -t none \
    --hostname $(uname -n) \
    --config-net \
    @setup@
}

run_stage3() {
  # Stage 3 setup the other namespaces
  log info "Prepare stage 3..."

  declare -a args
  args=(
    # Filesystem
    --bind / /
    --proc /proc
    --dev /dev
    --dev-bind /dev/fuse /dev/fuse
    --chdir "/lab"
    # Other namespaces
    --unshare-cgroup
    # ...
    --setenv STAGE 4
    --die-with-parent
  )
  @bubblewrap@/bin/bwrap ''${args[@]} @setup@
}

run_stage4() {
  # Stage 4 spawns tmux.
  log info "Prepare stage 4..."

  exec env STAGE=5 tmux -L "$(uname -n)" new-session -s "$(uname -n)" -n main @setup@
}

run_stage5() {
  # Stage 5 configures tmux
  log info "Prepare stage 4..."

  log debug "Configure tmux"
  tmux set-option prefix C-b
  tmux set-option -w remain-on-exit on
  tmux set-hook window-linked "set-option -w remain-on-exit on"
  tmux bind-key r respawn-window

  log debug "Configure podman"
  export CLAB_RUNTIME=podman
  mkdir -p /sys/fs/cgroup
  mount -t cgroup2 cgroup2 /sys/fs/cgroup
  ln -s ../proc/self/mounts /etc/mtab

  log debug "Add pane for logs"
  tmux split-window -v -l 3 -t '{bottom}' tail -F /var/log/pasta.log \; select-pane -T 'pasta logs' \; last-pane

  log debug "Spawn shell $SHELL..."
  SHLVL=0 $SHELL -i
}

case $STAGE in
  1)
    run_stage1
    # We have to explicitely exit as stage 1 is sourced from Nix shell hook.
    exit
    ;;
  *)
    run_stage$STAGE || {
      log error "Failure of stage $STAGE. Spawning a shell..."
      $SHELL -i
    }
    ;;
esac
