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
        containerlab = pkgs.containerlab.overrideAttrs (old: {
          # Patch it for podman support.
          tags = (old.tags or []) ++ [
            "podman"
            "exclude_graphdriver_btrfs"
            "btrfs_noversion"
            "exclude_graphdriver_devicemapper"
            "exclude_graphdriver_overlay"
            "containers_image_openpgp"
          ];
          doCheck = false;
          doInstallCheck = false;
        });
        sandbox = pkgs.buildEnv {
          name = "sandbox";
          paths = with pkgs; [
            # Basic environment
            coreutils
            findutils
            gawk
            getent
            gnugrep
            gnused
            less
            locale
            nix
            procps
            util-linux
            # network tools
            curl
            iproute2
            iputils
            mtr
            openssh
            # tmux
            tmux
            # Shells
            bashInteractive
            zsh
            # containerlab
            containerlab
            podman
          ];
        };
        python = pkgs.python3.withPackages (ps: with ps; [ rich ]);
        setup = pkgs.replaceVarsWith {
          src = ./setup.py;
          replacements = {
            inherit sandbox python;
            inherit (pkgs) ncurses passt cacert;
          };
          isExecutable = true;
        };
      in {
        apps.default = {
          type = "app";
          program = "${setup}";
        };
      });
}
