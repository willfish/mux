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


          doInstallCheck = true;
          installCheckPhase = ''
            runHook preInstallCheck

            if strings "$out/bin/mux" | grep -Fxq '/bin/bash'; then
              echo "hardcoded /bin/bash found in installed mux binary" >&2
              exit 1
            fi

            runHook postInstallCheck
          '';
        };

        devShells.default = pkgs.mkShell {
          buildInputs = with pkgs; [
            meson ninja pkg-config clang-tools
            libyaml valgrind
          ];
        };
      });
}
