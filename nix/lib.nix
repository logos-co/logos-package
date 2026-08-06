# Builds the lgx shared library
{ pkgs, common, src }:

pkgs.stdenv.mkDerivation {
  pname = "${common.pname}-lib";
  version = common.version;
  
  inherit src;
  inherit (common) nativeBuildInputs buildInputs cmakeFlags;
  
  configurePhase = ''
    runHook preConfigure
    
    cmake -S . -B build \
      -GNinja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
      -DLGX_BUILD_TESTS=OFF \
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
    
    mkdir -p $out/lib
    mkdir -p $out/include
    
    # Copy the shared library
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
    
    # Copy headers.
    #
    # Downstream repos (lgpm, lgpd, the package-manager UI) get the shared
    # semver implementation from here. logos/semver.hpp includes
    # <semver/semver.hpp>, so vendor that header in alongside it — otherwise
    # every consumer would need its own cpp-semver input just to see it.
    cp ${src}/src/lgx.h $out/include/
    cp -r ${src}/include/logos $out/include/
    mkdir -p $out/include/semver
    cp ${common.cpp-semver}/include/semver/semver.hpp $out/include/semver/

    runHook postInstall
  '';
  
  meta = common.meta // {
    description = "lgx - Logos Package Manager shared library";
  };
}
