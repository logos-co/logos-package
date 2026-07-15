# Builds the lgx shared library
{ pkgs, common, src }:

pkgs.stdenv.mkDerivation {
  pname = "${common.pname}-lib";
  version = common.version;
  
  inherit src;
  inherit (common) nativeBuildInputs buildInputs;
  
  configurePhase = ''
    runHook preConfigure
    
    cmake -S . -B build \
      -GNinja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
      -DLGX_BUILD_TESTS=OFF \
      -DLGX_BUILD_SHARED=ON
    
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
    if [ -f build/liblgx.dylib ]; then
      cp build/liblgx.dylib $out/lib/
    elif [ -f build/liblgx.so ]; then
      cp build/liblgx.so $out/lib/
    elif [ -f build/lgx.dll ]; then
      cp build/lgx.dll $out/lib/
      cp build/lgx.lib $out/lib/ 2>/dev/null || true
    fi
    
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
