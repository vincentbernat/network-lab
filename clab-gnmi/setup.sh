#!/usr/bin/env bash

# This script configures the environment to run Containerlab. The environment is
# reproducible (through Nix) and rootless (through bwrap). This is done in
# several stages.

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
  # Stage 1 configures the isolated filesystem. The network is shared with the
  # current namespace.

  log info "Setup stage 1..."
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

  # Create an SSH key
  install -m 0700 -d "$workspace/home/.ssh"
  progress "Creating an SSH key..." ssh-keygen -q -t rsa -f "$workspace/home/.ssh/id_rsa" -N ""
  log debug "SSH key created"

  # Prepare bubblewrap
  declare -a args
  args=(
    # Home and lab
    --bind "$workspace/home" /home/lab
    --bind-try "$HOME/.tmux.conf" /home/lab/.tmux.conf
    --bind "$PWD" /lab
    # System
    --ro-bind /nix /nix
    --dev /dev
    --proc /proc
    --tmpfs /tmp
    --tmpfs /run
    --ro-bind "$workspace/passwd" /etc/passwd
    --ro-bind "$workspace/group" /etc/group
    --ro-bind "$workspace/hosts" /etc/hosts
    --ro-bind "$workspace/nsswitch.conf" /etc/nsswitch.conf
    --tmpfs /etc/ssh/ssh_config.d
    # Environment
    --clearenv
    --setenv PATH "@sandbox@/bin"
    --setenv HOME "/home/lab"
    --setenv TERM "$TERM"
    --setenv TERMINFO "@ncurses@/share/terminfo"
    --setenv LANG "C.utf8"
    --setenv STAGE 2
    --hostname $labname
    --chdir "/lab"
    --uid 0 --gid 0
    # Podman
    --bind "$HOME/.local/share/containers/storage" /var/lib/containers/storage
    --setenv CLAB_RUNTIME podman
    # Unshare everything except network
    --unshare-all --share-net
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

  # Execute tmux
  @bubblewrap@/bin/bwrap ''${args[@]} @setup@
}

run_stage2() {
  # Stage 2 switches to tmux to be able to spawn more shells.

  exec env STAGE=3 tmux -L "$(uname -n)" new-session -s "$(uname -n)" -n main @setup@
}

run_stage3() {
  # Stage 3 configures tmux, spawn network namespace, configure pasta

  log debug "Configure tmux"
  tmux set-option prefix C-b
  tmux set-option -w remain-on-exit on
  tmux set-hook window-linked "set-option -w remain-on-exit on"
  tmux bind-key r respawn-window

  log info "Prepare network namespace"
  mkfifo /run/sync-netns
  tmux split-window -v -l 1 -t '{top}' env STAGE=4 unshare --user --net @setup@
  read < /run/sync-netns
  rm /run/sync-netns

  echo @passt@

  $SHELL -i
}

run_stage4() {
  # Stage 4 keeps the network namespace alive (we prefer to use a PID than a
  # mountpoint to not leak namespaces)

  echo $$ > /run/netns.pid
  echo > /run/sync-netns
  progress "Network namespace" sleep infinity
}

case $STAGE in
  1)
    run_stage1
    # We have to explicitely exit as stage 1 is sourced from Nix shell hook.
    exit
    ;;
  2) run_stage2 ;;
  3) run_stage3 ;;
  4) run_stage4 ;;
  *)
    log error "Unknown stage" stage "$STAGE"
    exit 1
    ;;
esac
