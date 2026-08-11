# Builds lgx binary, shared library, and tests all together
{ pkgs, common, src }:

pkgs.stdenv.mkDerivation {
  pname = "${common.pname}-all";
  version = common.version;
  
  inherit src;
  inherit (common) nativeBuildInputs buildInputs cmakeFlags;
  
  configurePhase = ''
    runHook preConfigure
    
    cmake -S . -B build \
      -GNinja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
      -DLGX_BUILD_TESTS=${if common.isWindows then "OFF" else "ON"} \
      -DLGX_BUILD_SHARED=ON \
      $cmakeFlags "''${cmakeFlagsArray[@]}"

    runHook postConfigure
  '';
  
  buildPhase = ''
    runHook preBuild
    
    cmake --build build
    
    runHook postBuild
  '';
  
  installPhase = ''
    runHook preInstall
    
    # Use CMake's install command for the binary and core library
    cd build
    cmake --install . --prefix $out
    cd ..
    
    # Install shared library (not covered by cmake install)
    mkdir -p $out/lib
    # CMake names a MinGW shared library liblgx.dll with an import library
    # liblgx.dll.a -- NOT the MSVC lgx.dll/lgx.lib pair this used to look for.
    # That mattered more than it looks: the old if/elif chain had no else, so an
    # unmatched name installed an EMPTY $out/lib and the build still succeeded.
    # Glob every spelling, and fail loudly rather than shipping nothing.
    shopt -s nullglob
    # EVERY entry must contain a wildcard: nullglob only removes patterns that
    # fail to match, and a literal path with no wildcard is left in the array
    # verbatim -- which is what made the first attempt at this die in `cp`
    # rather than in the intended error branch below.
    libs=(build/liblgx*.dylib build/liblgx*.so build/liblgx*.dll build/lgx*.dll \
          build/liblgx*.dll.a build/lgx*.lib)
    if [ ''${#libs[@]} -eq 0 ]; then
      echo "ERROR: no lgx shared library found in build/" >&2
      ls -la build >&2
      exit 1
    fi
    cp "''${libs[@]}" $out/lib/
    
    # Install headers. See nix/lib.nix — logos/semver.hpp is the shared semver
    # implementation and needs the cpp-semver header vendored beside it so a
    # single include dir is enough for every consumer.
    mkdir -p $out/include
    cp ${src}/src/lgx.h $out/include/
    cp -r ${src}/include/logos $out/include/
    mkdir -p $out/include/semver
    cp ${common.cpp-semver}/include/semver/semver.hpp $out/include/semver/

    runHook postInstall
  '';
  
  # Run tests during build to ensure they pass.
  #
  # Not under cross: ctest would have to execute PE binaries on the Linux build
  # host. The Windows test story is a native run on a real machine (or wine),
  # not a build-time check -- see the verification ladder.
  doCheck = !common.isWindows;
  checkPhase = ''
    runHook preCheck
    
    cd build
    # Set LGX_BINARY environment variable for CLI tests
    export LGX_BINARY="$(pwd)/lgx"
    ctest --output-on-failure
    cd ..
    
    runHook postCheck
  '';
  
  meta = common.meta // {
    description = "lgx - Logos Package Manager (binary, library, and tests)";
  };
}
