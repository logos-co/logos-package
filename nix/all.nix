# Builds lgx binary, shared library, and tests all together
{ pkgs, common, src }:

pkgs.stdenv.mkDerivation {
  pname = "${common.pname}-all";
  version = common.version;
  
  inherit src;
  
  nativeBuildInputs = common.nativeBuildInputs
    ++ pkgs.lib.optionals pkgs.stdenv.isLinux [ pkgs.patchelf ];
  
  inherit (common) buildInputs;
  
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
    
    # Install test executables
    mkdir -p $out/bin
    cp build/tests/lgx_tests $out/bin/
    cp build/tests/lgx_lib_tests $out/bin/
    
    # Fix rpath for lgx_lib_tests to find the shared library
    if [ -f $out/bin/lgx_lib_tests ]; then
      if [[ "$OSTYPE" == "darwin"* ]]; then
        install_name_tool -add_rpath $out/lib $out/bin/lgx_lib_tests
      else
        patchelf --set-rpath $out/lib:$(patchelf --print-rpath $out/bin/lgx_lib_tests) $out/bin/lgx_lib_tests
      fi
    fi
    
    runHook postInstall
  '';
  
  # ---------------------------------------------------------------------------
  # Portability fixup: rewrite /nix/store/ references so the library works on
  # machines without Nix installed.
  # ---------------------------------------------------------------------------
  postFixup = ''
    if [[ "$(uname)" == "Darwin" ]]; then
      echo "=== Making library portable (macOS) ==="

      strip_nix_rpaths() {
        local binary="$1"
        local bname
        bname=$(basename "$binary")

        otool -l "$binary" 2>/dev/null | awk '/LC_RPATH/{found=1} found && /path /{print $2; found=0}' | while IFS= read -r rpath; do
          case "$rpath" in
            /nix/store/*)
              echo "  $bname: removing rpath $rpath"
              install_name_tool -delete_rpath "$rpath" "$binary" 2>/dev/null || true
              ;;
          esac
        done
      }

      fixup_macos_binary() {
        local binary="$1"
        local bname
        bname=$(basename "$binary")

        otool -L "$binary" | tail -n +2 | awk '{print $1}' | while IFS= read -r dep_path; do
          case "$dep_path" in
            /nix/store/*)
              local dep_name
              dep_name=$(basename "$dep_path")

              case "$dep_name" in
                libz.*)
                  echo "  $bname: $dep_name -> /usr/lib/libz.1.dylib"
                  install_name_tool -change "$dep_path" /usr/lib/libz.1.dylib "$binary"
                  ;;
                libc++.*)
                  echo "  $bname: $dep_name -> /usr/lib/libc++.1.dylib"
                  install_name_tool -change "$dep_path" /usr/lib/libc++.1.dylib "$binary"
                  ;;
                libc++abi.*)
                  echo "  $bname: $dep_name -> /usr/lib/libc++abi.dylib"
                  install_name_tool -change "$dep_path" /usr/lib/libc++abi.dylib "$binary"
                  ;;
                *)
                  if [ ! -f "$out/lib/$dep_name" ]; then
                    echo "  $bname: bundling $dep_name"
                    cp "$dep_path" "$out/lib/$dep_name"
                    chmod u+w "$out/lib/$dep_name"
                    install_name_tool -id "@loader_path/$dep_name" "$out/lib/$dep_name"
                  fi
                  echo "  $bname: $dep_name -> @loader_path/$dep_name"
                  install_name_tool -change "$dep_path" "@loader_path/$dep_name" "$binary"
                  ;;
              esac
              ;;
          esac
        done

        strip_nix_rpaths "$binary"
      }

      # Fix the shared library if present
      if [ -f "$out/lib/liblgx.dylib" ]; then
        install_name_tool -id @rpath/liblgx.dylib "$out/lib/liblgx.dylib"
        fixup_macos_binary "$out/lib/liblgx.dylib"

        # Fix bundled dependencies (multiple passes for transitive deps)
        for pass in 1 2 3; do
          for bundled in "$out/lib/"*.dylib; do
            [ "$(basename "$bundled")" = "liblgx.dylib" ] && continue
            fixup_macos_binary "$bundled"
          done
        done
      fi

      echo "=== macOS portability fixup complete ==="

    else
      echo "=== Making library portable (Linux) ==="

      local main_lib=""
      if [ -f "$out/lib/liblgx.so" ]; then
        main_lib="$out/lib/liblgx.so"
      fi

      if [ -n "$main_lib" ]; then
        ldd "$main_lib" 2>/dev/null | grep '/nix/store/' | awk '{print $3}' | while IFS= read -r dep_path; do
          [ -z "$dep_path" ] && continue
          local dep_name
          dep_name=$(basename "$dep_path")
          if [ ! -f "$out/lib/$dep_name" ]; then
            echo "  bundling $dep_name"
            cp "$dep_path" "$out/lib/$dep_name"
            chmod u+w "$out/lib/$dep_name"
          fi
        done

        for pass in 1 2 3; do
          for bundled in "$out/lib/"*.so*; do
            ldd "$bundled" 2>/dev/null | grep '/nix/store/' | awk '{print $3}' | while IFS= read -r dep_path; do
              [ -z "$dep_path" ] && continue
              local dep_name
              dep_name=$(basename "$dep_path")
              if [ ! -f "$out/lib/$dep_name" ]; then
                echo "  bundling transitive dep $dep_name"
                cp "$dep_path" "$out/lib/$dep_name"
                chmod u+w "$out/lib/$dep_name"
              fi
            done
          done
        done

        for lib in "$out/lib/"*.so*; do
          echo "  setting RPATH on $(basename "$lib")"
          patchelf --set-rpath '$ORIGIN' "$lib" 2>/dev/null || true
        done
      fi

      echo "=== Linux portability fixup complete ==="
    fi
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
