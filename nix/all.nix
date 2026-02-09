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
      echo "=== Making portable (macOS) ==="

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

      # fixup_macos_executable <binary>
      #   Like fixup_macos_binary but for executables: non-system deps use
      #   @rpath/<name> and three RPATHs are added so the binary searches
      #   ./, ./lib/, and ../lib/ relative to itself.
      fixup_macos_executable() {
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
                  # Bundle non-system dep into $out/lib/ if not already there
                  if [ ! -f "$out/lib/$dep_name" ]; then
                    echo "  $bname: bundling $dep_name"
                    mkdir -p "$out/lib"
                    cp "$dep_path" "$out/lib/$dep_name"
                    chmod u+w "$out/lib/$dep_name"
                    install_name_tool -id "@rpath/$dep_name" "$out/lib/$dep_name"
                  fi
                  echo "  $bname: $dep_name -> @rpath/$dep_name"
                  install_name_tool -change "$dep_path" "@rpath/$dep_name" "$binary"
                  ;;
              esac
              ;;
          esac
        done

        strip_nix_rpaths "$binary"

        # Add search paths: ./, ./lib/, ../lib/ relative to executable
        install_name_tool -add_rpath @executable_path/. "$binary" 2>/dev/null || true
        install_name_tool -add_rpath @executable_path/lib "$binary" 2>/dev/null || true
        install_name_tool -add_rpath @executable_path/../lib "$binary" 2>/dev/null || true
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

      # Fix all executables in $out/bin/
      if [ -d "$out/bin" ]; then
        for exe in "$out/bin/"*; do
          [ -f "$exe" ] || continue
          # Only process Mach-O executables
          if file "$exe" 2>/dev/null | grep -q 'Mach-O'; then
            echo "  fixing executable: $(basename "$exe")"
            fixup_macos_executable "$exe"
          fi
        done

        # Fix any newly bundled deps from executable fixup (transitive)
        if [ -d "$out/lib" ]; then
          for pass in 1 2 3; do
            for bundled in "$out/lib/"*.dylib; do
              [ "$(basename "$bundled")" = "liblgx.dylib" ] && continue
              fixup_macos_binary "$bundled"
            done
          done
        fi
      fi

      echo "=== macOS portability fixup complete ==="

    else
      echo "=== Making portable (Linux) ==="

      # Bundle /nix/store/ deps from all binaries and libraries
      bundle_nix_deps() {
        local target="$1"
        ldd "$target" 2>/dev/null | grep '/nix/store/' | awk '{print $3}' | while IFS= read -r dep_path; do
          [ -z "$dep_path" ] && continue
          local dep_name
          dep_name=$(basename "$dep_path")
          if [ ! -f "$out/lib/$dep_name" ]; then
            echo "  bundling $dep_name"
            mkdir -p "$out/lib"
            cp "$dep_path" "$out/lib/$dep_name"
            chmod u+w "$out/lib/$dep_name"
          fi
        done
      }

      # Bundle deps from the shared library
      if [ -f "$out/lib/liblgx.so" ]; then
        bundle_nix_deps "$out/lib/liblgx.so"
      fi

      # Bundle deps from binaries
      if [ -d "$out/bin" ]; then
        for exe in "$out/bin/"*; do
          [ -f "$exe" ] || continue
          bundle_nix_deps "$exe"
        done
      fi

      # Bundle transitive deps
      for pass in 1 2 3; do
        for bundled in "$out/lib/"*.so*; do
          [ -f "$bundled" ] || continue
          bundle_nix_deps "$bundled"
        done
      done

      # Set RPATH on libraries
      for lib in "$out/lib/"*.so*; do
        [ -f "$lib" ] || continue
        echo "  setting RPATH on $(basename "$lib")"
        patchelf --set-rpath '$ORIGIN' "$lib" 2>/dev/null || true
      done

      # Set RPATH on binaries: search ./, ./lib/, ../lib/
      if [ -d "$out/bin" ]; then
        for exe in "$out/bin/"*; do
          [ -f "$exe" ] || continue
          if file "$exe" 2>/dev/null | grep -q 'ELF'; then
            echo "  setting RPATH on $(basename "$exe")"
            patchelf --set-rpath '$ORIGIN:$ORIGIN/lib:$ORIGIN/../lib' "$exe" 2>/dev/null || true
          fi
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
