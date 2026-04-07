{
  description = "lgx - Logos Package Manager CLI";

  inputs = {
    logos-nix.url = "github:logos-co/logos-nix";
    nixpkgs.follows = "logos-nix/nixpkgs";
  };

  outputs = { self, nixpkgs, logos-nix }:
    let
      systems = [ "aarch64-darwin" "x86_64-darwin" "aarch64-linux" "x86_64-linux" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f {
        pkgs = import nixpkgs { inherit system; };
      });
    in
    {
      packages = forAllSystems ({ pkgs }: 
        let
          # Common configuration
          common = import ./nix/default.nix { inherit pkgs; };
          src = ./.;
          
          # Binary package
          bin = import ./nix/bin.nix { inherit pkgs common src; };
          
          # Library package
          libPkg = import ./nix/lib.nix { inherit pkgs common src; };
          
          # Combined package (binary + library + tests)
          allPkg = import ./nix/all.nix { inherit pkgs common src; };
        in
        {
          # lgx binary package
          lgx = bin;
          
          # lgx shared library
          lib = libPkg;
          
          # lgx all-in-one package (binary, library, and tests)
          all = allPkg;
          
          # Default package
          default = allPkg;
        }
      );

      checks = forAllSystems ({ pkgs }:
        let
          common = import ./nix/default.nix { inherit pkgs; };
          src = ./.;
        in {
          tests = import ./nix/all.nix { inherit pkgs common src; };
        }
      );

      devShells = forAllSystems ({ pkgs }: {
        default = pkgs.mkShell {
          nativeBuildInputs = [
            pkgs.cmake
            pkgs.ninja
            pkgs.pkg-config
          ];
          buildInputs = [
            pkgs.zlib
            pkgs.icu
            pkgs.nlohmann_json
            pkgs.libsodium
          ];
          
          shellHook = ''
            echo "lgx development environment"
            echo "Build with: nix build"
            echo "Or use cmake directly: cmake -B build -GNinja && cmake --build build"
          '';
        };
      });
    };
}
