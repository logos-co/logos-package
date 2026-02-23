# Builds lgx binary, shared library, and tests all together
{ pkgs, common, src }:

pkgs.stdenv.mkDerivation {
  pname = "${common.pname}-all";
  version = common.version;
  
  inherit src;
  inherit (common) nativeBuildInputs buildInputs;
  
  configurePhase = ''
    runHook preConfigure
    
    cmake -S . -B build \
      -GNinja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
      -DLGX_BUILD_TESTS=ON \
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
    
    # Use CMake's install command for the binary and core library
    cd build
    cmake --install . --prefix $out
    cd ..
    
    # Install shared library (not covered by cmake install)
    mkdir -p $out/lib
    if [ -f build/liblgx.dylib ]; then
      cp build/liblgx.dylib $out/lib/
    elif [ -f build/liblgx.so ]; then
      cp build/liblgx.so $out/lib/
    elif [ -f build/lgx.dll ]; then
      cp build/lgx.dll $out/lib/
      cp build/lgx.lib $out/lib/ 2>/dev/null || true
    fi
    
    # Install headers
    mkdir -p $out/include
    cp ${src}/src/lgx.h $out/include/
    
    runHook postInstall
  '';
  
  # Run tests during build to ensure they pass
  doCheck = true;
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
