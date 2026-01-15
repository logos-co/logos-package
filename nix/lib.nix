# Builds the lgx shared library
{ pkgs, common, src }:

pkgs.stdenv.mkDerivation {
  pname = "${common.pname}-lib";
  version = common.version;
  
  inherit src;
  inherit (common) nativeBuildInputs buildInputs meta;
  
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
      cp build/liblgx.0.dylib $out/lib/ 2>/dev/null || true
    elif [ -f build/liblgx.so ]; then
      cp build/liblgx.so* $out/lib/
    elif [ -f build/lgx.dll ]; then
      cp build/lgx.dll $out/lib/
      cp build/lgx.lib $out/lib/ 2>/dev/null || true
    fi
    
    # Copy headers
    cp ${src}/src/lgx.h $out/include/
    
    runHook postInstall
  '';
  
  meta = common.meta // {
    description = "lgx - Logos Package Manager shared library";
  };
}
