{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
      in {
        packages.default = pkgs.stdenv.mkDerivation {
          pname = "mux";
          version = "0.1.0";
          src = ./.;
          nativeBuildInputs = with pkgs; [ meson ninja pkg-config ];
          buildInputs = with pkgs; [ libyaml ];
        };

        devShells.default = pkgs.mkShell {
          buildInputs = with pkgs; [
            meson ninja pkg-config clang-tools
            libyaml valgrind
          ];
        };
      });
}
