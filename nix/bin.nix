# Builds the lgx binary
{ pkgs, common, src }:

pkgs.stdenv.mkDerivation {
  pname = common.pname;
  version = common.version;
  
  inherit src;
  
  nativeBuildInputs = common.nativeBuildInputs
    ++ pkgs.lib.optionals pkgs.stdenv.isLinux [ pkgs.patchelf ];
  
  inherit (common) buildInputs cmakeFlags meta;
  
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
  
  # ---------------------------------------------------------------------------
  # Portability fixup: rewrite /nix/store/ references so the binary works on
  # machines without Nix installed.
  #
  # macOS: system libs -> /usr/lib/, non-system libs (ICU) -> bundled into
  #        $out/lib/ and referenced via @rpath with search paths:
  #        ./, ./lib/, ../lib/ (relative to executable).
  # Linux: bundle deps into $out/lib/ and set RPATH.
  # ---------------------------------------------------------------------------
  postFixup = ''
    if [[ "$(uname)" == "Darwin" ]]; then
      echo "=== Making binary portable (macOS) ==="

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
                  # Bundle non-system dep into $out/lib/
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

      # fixup_macos_binary for bundled dylibs (uses @loader_path)
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
                  install_name_tool -change "$dep_path" /usr/lib/libz.1.dylib "$binary"
                  ;;
                libc++.*)
                  install_name_tool -change "$dep_path" /usr/lib/libc++.1.dylib "$binary"
                  ;;
                libc++abi.*)
                  install_name_tool -change "$dep_path" /usr/lib/libc++abi.dylib "$binary"
                  ;;
                *)
                  if [ ! -f "$out/lib/$dep_name" ]; then
                    cp "$dep_path" "$out/lib/$dep_name"
                    chmod u+w "$out/lib/$dep_name"
                    install_name_tool -id "@loader_path/$dep_name" "$out/lib/$dep_name"
                  fi
                  install_name_tool -change "$dep_path" "@loader_path/$dep_name" "$binary"
                  ;;
              esac
              ;;
          esac
        done

        strip_nix_rpaths "$binary"
      }

      # Fix the binary
      fixup_macos_executable "$out/bin/lgx"

      # Fix bundled dependencies (multiple passes for transitive deps)
      if [ -d "$out/lib" ]; then
        for pass in 1 2 3; do
          for bundled in "$out/lib/"*.dylib; do
            [ -f "$bundled" ] || continue
            fixup_macos_binary "$bundled"
          done
        done
      fi

      echo "=== macOS portability fixup complete ==="

    else
      echo "=== Making binary portable (Linux) ==="

      # Bundle /nix/store/ deps
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

      bundle_nix_deps "$out/bin/lgx"

      # Bundle transitive deps
      for pass in 1 2 3; do
        for bundled in "$out/lib/"*.so*; do
          [ -f "$bundled" ] || continue
          bundle_nix_deps "$bundled"
        done
      done

      # Set RPATH on bundled libraries
      for lib in "$out/lib/"*.so*; do
        [ -f "$lib" ] || continue
        patchelf --set-rpath '$ORIGIN' "$lib" 2>/dev/null || true
      done

      # Set RPATH on binary: search ./, ./lib/, ../lib/
      patchelf --set-rpath '$ORIGIN:$ORIGIN/lib:$ORIGIN/../lib' "$out/bin/lgx" 2>/dev/null || true

      echo "=== Linux portability fixup complete ==="
    fi
  '';
}
