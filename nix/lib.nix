# Builds the lgx shared library
{ pkgs, common, src }:

pkgs.stdenv.mkDerivation {
  pname = "${common.pname}-lib";
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
    
    # Copy headers
    cp ${src}/src/lgx.h $out/include/
    
    runHook postInstall
  '';
  
  # ---------------------------------------------------------------------------
  # Portability fixup: rewrite /nix/store/ references so the library works on
  # machines without Nix installed.
  #
  # macOS: use install_name_tool to point system-available libs (zlib, libc++)
  #        at their standard paths, and bundle non-system libs (ICU) alongside
  #        the library with @loader_path references.
  # Linux: bundle all /nix/store/ deps and set RPATH to $ORIGIN.
  # ---------------------------------------------------------------------------
  postFixup = ''
    if [[ "$(uname)" == "Darwin" ]]; then
      echo "=== Making library portable (macOS) ==="

      # strip_nix_rpaths <binary>
      #   Remove any LC_RPATH entries pointing to /nix/store/
      strip_nix_rpaths() {
        local binary="$1"
        local bname
        bname=$(basename "$binary")

        otool -l "$binary" 2>/dev/null | awk '/LC_RPATH/{found=1} found && /path /{print $2; found=0}' | while IFS= read -r rpath; do
          case "$rpath" in
            /nix/*)
              echo "  $bname: removing rpath $rpath"
              install_name_tool -delete_rpath "$rpath" "$binary" 2>/dev/null || true
              ;;
          esac
        done
      }

      # fixup_macos_binary <binary>
      #   For each /nix/store/ linked library:
      #     - System libs (libz, libc++, libc++abi) -> rewrite to /usr/lib/ path
      #     - Other libs -> copy into $out/lib/ and rewrite to @loader_path/
      #   Also strips any /nix/store/ RPATHs.
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
                  # Bundle non-system dependency
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

        # Remove any /nix/store/ RPATHs from this binary
        strip_nix_rpaths "$binary"
      }

      # 1. Fix install name of our library
      install_name_tool -id @rpath/liblgx.dylib "$out/lib/liblgx.dylib"

      # 2. Fix direct dependencies of liblgx.dylib
      fixup_macos_binary "$out/lib/liblgx.dylib"

      # 3. Fix bundled dependencies (and their transitive deps).
      #    Run multiple passes so transitive deps that get copied in one pass
      #    are themselves fixed in the next pass.
      for pass in 1 2 3; do
        for bundled in "$out/lib/"*.dylib; do
          [ "$(basename "$bundled")" = "liblgx.dylib" ] && continue
          fixup_macos_binary "$bundled"
        done
      done

      echo "=== macOS portability fixup complete ==="

    else
      echo "=== Making library portable (Linux) ==="

      local main_lib=""
      if [ -f "$out/lib/liblgx.so" ]; then
        main_lib="$out/lib/liblgx.so"
      fi

      if [ -n "$main_lib" ]; then
        # Bundle all /nix/store/ dependencies
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

        # Also bundle transitive deps from any copied libraries
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

        # Set RPATH to $ORIGIN on all libraries
        for lib in "$out/lib/"*.so*; do
          echo "  setting RPATH on $(basename "$lib")"
          patchelf --set-rpath '$ORIGIN' "$lib" 2>/dev/null || true
        done
      fi

      echo "=== Linux portability fixup complete ==="
    fi
  '';
  
  meta = common.meta // {
    description = "lgx - Logos Package Manager shared library";
  };
}
