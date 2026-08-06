# Builds the lgx binary
{ pkgs, common, src }:

pkgs.stdenv.mkDerivation {
  pname = common.pname;
  version = common.version;
  
  inherit src;
  inherit (common) nativeBuildInputs buildInputs cmakeFlags meta;
  
  configurePhase = ''
    runHook preConfigure
    
    # $cmakeFlags / cmakeFlagsArray carry what nixpkgs' own cmakeConfigurePhase
    # would have passed -- most importantly -DCMAKE_SYSTEM_NAME=Windows and the
    # cross toolchain file. This phase is hand-rolled, so dropping them is
    # invisible natively and fatal under cross: CMake silently configures for
    # the BUILD platform and the failure surfaces far from the cause.
    cmake -S . -B build \
      -GNinja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
      -DLGX_BUILD_TESTS=OFF \
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
    
    mkdir -p $out/bin
    # .exe when cross-compiling to Windows.
    cp build/lgx${pkgs.stdenv.hostPlatform.extensions.executable} $out/bin/

    runHook postInstall
  '';
}
