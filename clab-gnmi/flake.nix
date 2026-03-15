{
  inputs = {
    nixpkgs.url = "nixpkgs";
    flake-utils.url = "github:numtide/flake-utils";
  };
  outputs = { self, flake-utils, ... }@inputs:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import inputs.nixpkgs {
          inherit system;
        };
        sandbox = pkgs.buildEnv {
          name = "sandbox";
          paths = with pkgs; [
            bashInteractive
            containerlab
            coreutils
            diffutils
            podman
            findutils
            gawk
            gnugrep
            gnused
            iproute2
            less
            locale
            openssh
            procps
            tmux
            which
            zsh
          ];
        };
      in
      {
        devShells.default = pkgs.mkShell {
          name = "containerlab";
          shellHook = ''
            labname=''${PWD##*/}
            workspace=$(mktemp -d)
            trap "rm -rf $workspace" EXIT

            # Setup system files
            cat > "$workspace/group" <<EOF
            root:x:0:
            EOF
            cat > "$workspace/hosts" <<EOF
            127.0.0.1 localhost.localdomain localhost
            127.0.1.1 $labname
            ::1       ip6-localhost ip6-loopback
            EOF
            cp /etc/resolv.conf "$workspace/resolv.conf"

            # Setup home files
            install -m 0700 -d "$workspace/home/.ssh"
            ssh-keygen -q -t rsa -f "$workspace/home/.ssh/id_rsa" -N ""

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
              --setenv PATH "${sandbox}/bin"
              --setenv HOME "/home/lab"
              --setenv TERM "$TERM"
              --setenv TERMINFO "${pkgs.ncurses}/share/terminfo"
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
            case $(getent passwd $USER) in
              */zsh)
                SHELL=${sandbox}/bin/zsh
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
                  SHELL=${sandbox}/bin/bash
                  --ro-bind-try "$HOME/.bashrc" /home/lab/.bashrc
                  --ro-bind-try "$HOME/.bash_profile" /home/lab/.bash_profile
                )
                ;;
            esac
            cat > "$workspace/passwd" <<EOF
            root:x:0:0:root:/home/lab:$SHELL
            EOF

            # Execute tmux
            exec ${pkgs.bubblewrap}/bin/bwrap ''${args[@]} $SHELL -ic tmux
          '';
        };
      });
}
