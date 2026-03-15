#!/usr/bin/env bash

set -e

PATH=@sandbox@/bin
labname=${PWD##*/}
gum=@gum@/bin/gum

progress() {
  local title="$1"
  shift
  $gum spin --spinner points --title "$1" --show-error -- "$@"
}

log() {
  local level="$1"
  shift
  $gum log --time=datetime --structured --level "$level" "$@"
}

setup_workspace() {
  # Setup the temporary workspace
  workspace=$(mktemp -d)
  trap "rm -rf $workspace" EXIT

  # Set the SHELL variable
  case $(getent passwd $USER) in
    */zsh) SHELL=@sandbox@/bin/zsh ;;
    *) SHELL=@sandbox@/bin/bash ;;
  esac
  log info "Shell is $SHELL"

  # Setup system files
  log info "Creating workspace..." directory "$workspace"
  cat > "$workspace/passwd" <<EOF
root:x:0:0:root:/home/lab:$SHELL
EOF
  cat > "$workspace/group" <<EOF
root:x:0:
EOF
  cat > "$workspace/hosts" <<EOF
127.0.0.1 localhost.localdomain localhost
127.0.1.1 $labname
::1       ip6-localhost ip6-loopback
EOF
  cp /etc/resolv.conf "$workspace/resolv.conf"

  # Setup an SSH key
  install -m 0700 -d "$workspace/home/.ssh"
  progress "Creating an SSH key..." ssh-keygen -q -t rsa -f "$workspace/home/.ssh/id_rsa" -N ""
  log debug "SSH key created"
}

setup_isolation() {
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
    --ro-bind "$workspace/resolv.conf" /etc/resolv.conf
    --tmpfs /etc/ssh/ssh_config.d
    # Environment
    --clearenv
    --setenv PATH "@sandbox@/bin"
    --setenv HOME "/home/lab"
    --setenv TERM "$TERM"
    --setenv TERMINFO "@ncurses@/share/terminfo"
    --setenv STEP 2
    --hostname $labname
    --chdir "/lab"
    --uid 0 --gid 0
    # Podman
    --bind "$HOME/.local/share/containers/storage" /var/lib/containers/storage
    --setenv CLAB_RUNTIME podman
    # Unshare everything
    --unshare-all
    --die-with-parent
  )
  case $SHELL in
    */zsh)
      args+=(
        --ro-bind-try "$HOME/.zsh" /home/lab/.zsh
        --ro-bind-try "$HOME/.zshrc" /home/lab/.zshrc
        --ro-bind-try "$HOME/.zshenv" /home/lab/.zshenv
        # This is specific to my Zsh setup
        --tmpfs "/home/lab/.zsh/run"
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

case ${STEP:-1} in
  1)
    setup_workspace
    setup_isolation
    exit
    ;;
  2)
    exec tmux
esac
