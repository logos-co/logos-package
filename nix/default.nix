# Common build configuration shared across all packages
{ pkgs }:

let
  cpp-semver = import ./cpp-semver.nix { inherit pkgs; };

  # gtest is dropped and the test targets switched off when cross-compiling:
  # gtest_discover_tests RUNS the freshly linked test binary at build time to
  # enumerate cases, and a cross build cannot execute on the build host.
  isWindows = pkgs.stdenv.hostPlatform.isWindows;
  canRunTests = pkgs.stdenv.buildPlatform.canExecute pkgs.stdenv.hostPlatform;

  # Android: ICU's C API from the platform's libicu.so and the NDK's zlib, so
  # an app ships neither.
  isAndroid = pkgs.stdenv.hostPlatform.isAndroid;
  # zlib's headers alone: the whole sysroot include dir on the include path
  # would shadow libc++'s own <stdint.h> and friends.
  ndkZlibHeaders = pkgs.runCommand "ndk-zlib-headers" { } ''
    mkdir -p $out/include
    ln -s ${pkgs.androidPkgs.ndkSysroot}/usr/include/zlib.h ${pkgs.androidPkgs.ndkSysroot}/usr/include/zconf.h $out/include/
  '';
in
{
  pname = "lgx";
  version = "0.1.0";

  inherit cpp-semver isWindows canRunTests;

  # Common native build inputs
  nativeBuildInputs = [
    pkgs.cmake
    pkgs.ninja
    pkgs.pkg-config
  ];

  # Common runtime dependencies
  buildInputs = [
    pkgs.nlohmann_json
    pkgs.libsodium
    cpp-semver
  ]
  ++ pkgs.lib.optionals (!isAndroid) [ pkgs.zlib pkgs.icu ]
  ++ pkgs.lib.optional canRunTests pkgs.gtest;
  
  # Common CMake flags
  cmakeFlags = [ 
    "-GNinja"
  ] ++ pkgs.lib.optionals isAndroid [
    "-DLGX_PLATFORM_ICU=ON"
    "-DZLIB_LIBRARY=${pkgs.androidPkgs.ndkStubLibDir}/libz.so"
    "-DZLIB_INCLUDE_DIR=${ndkZlibHeaders}/include"
  ];
  
  # Metadata
  meta = with pkgs.lib; {
    description = "lgx - Logos Package Manager CLI";
    platforms = platforms.unix ++ platforms.windows;
  };
}
