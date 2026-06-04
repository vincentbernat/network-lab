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
        setup = pkgs.replaceVarsWith {
          src = ./setup.sh;
          replacements = {
            inherit sandbox;
            inherit (pkgs) ncurses bubblewrap gum passt cacert;
            setup = placeholder "out";
          };
          isExecutable = true;
        };
      in
      {
        devShells.default = pkgs.mkShell {
          name = "containerlab";
          shellHook = ''
            STAGE=1
            source ${setup}
          '';
        };
      });
}
