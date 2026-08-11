# Common build configuration shared across all packages
{ pkgs }:

let
  cpp-semver = import ./cpp-semver.nix { inherit pkgs; };

  # gtest is dropped and the test targets switched off when cross-compiling:
  # gtest_discover_tests RUNS the freshly linked test binary at build time to
  # enumerate cases, and a PE cannot execute on the Linux build host.
  isWindows = pkgs.stdenv.hostPlatform.isWindows;
in
{
  pname = "lgx";
  version = "0.1.0";

  inherit cpp-semver isWindows;

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
    cpp-semver
  ]
  ++ pkgs.lib.optional (!isWindows) pkgs.gtest;
  
  # Common CMake flags
  cmakeFlags = [ 
    "-GNinja"
  ];
  
  # Metadata
  meta = with pkgs.lib; {
    description = "lgx - Logos Package Manager CLI";
    platforms = platforms.unix ++ platforms.windows;
  };
}
