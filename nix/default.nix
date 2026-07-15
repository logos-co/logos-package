# Common build configuration shared across all packages
{ pkgs }:

let
  cpp-semver = import ./cpp-semver.nix { inherit pkgs; };
in
{
  pname = "lgx";
  version = "0.1.0";

  inherit cpp-semver;

  # Common native build inputs
  nativeBuildInputs = [
    pkgs.cmake
    pkgs.ninja
    pkgs.pkg-config
  ];

  # Common runtime dependencies
  buildInputs = [
    pkgs.zlib
    pkgs.icu
    pkgs.nlohmann_json
    pkgs.libsodium
    pkgs.gtest
    cpp-semver
  ];
  
  # Common CMake flags
  cmakeFlags = [ 
    "-GNinja"
  ];
  
  # Metadata
  meta = with pkgs.lib; {
    description = "lgx - Logos Package Manager CLI";
    platforms = platforms.unix;
  };
}
