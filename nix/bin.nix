# Builds the lgx binary
{ pkgs, common, src }:

pkgs.stdenv.mkDerivation {
  pname = common.pname;
  version = common.version;
  
  inherit src;
  inherit (common) nativeBuildInputs buildInputs cmakeFlags meta;
  
  configurePhase = ''
    runHook preConfigure
    
    cmake -S . -B build \
      -GNinja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
      -DLGX_BUILD_TESTS=OFF
    
    runHook postConfigure
  '';
  
  buildPhase = ''
    runHook preBuild
    
    cmake --build build
    
    runHook postBuild
  '';
  
  installPhase = ''
    runHook preInstall
    
    mkdir -p $out/bin
    cp build/lgx $out/bin/
    
    runHook postInstall
  '';
}
