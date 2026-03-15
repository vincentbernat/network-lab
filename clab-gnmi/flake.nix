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
            findutils
            gawk
            getent
            gnugrep
            gnused
            iproute2
            less
            locale
            nix
            openssh
            podman
            procps
            tmux
            zsh
          ];
        };
        setup = pkgs.replaceVarsWith {
          src = ./setup.sh;
          replacements = {
            inherit sandbox;
            inherit (pkgs) ncurses bubblewrap gum;
            setup = placeholder "out";
          };
          isExecutable = true;
        };
      in
      {
        devShells.default = pkgs.mkShell {
          name = "containerlab";
          shellHook = builtins.readFile setup;
        };
      });
}
