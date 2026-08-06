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
    
    # Install via CMake's own install(TARGETS lgx_shared) rule rather than
    # copying files out of build/ by hand.
    #
    # On Windows a shared library is TWO artifacts that must match each other:
    # liblgx.dll (the code) and liblgx.dll.a (the import library consumers
    # actually link against, supplying the __imp_* symbols that __declspec
    # (dllimport) generates). Hand-globbing build/ picked up a stale, mismatched
    # liblgx.dll.a containing 2 symbols while the DLL exported 437 -- so lgpd
    # configured and compiled cleanly, then failed at link with
    # "undefined reference to `__imp_lgx_verify'". CMake knows the correct
    # pairing; we do not.
    cmake --install build --prefix $out

    # ...but this output is the LIBRARY, so drop the CLI the install rules also
    # emit. Only the executable: on Windows `install(TARGETS lgx_shared RUNTIME
    # DESTINATION bin)` puts liblgx.dll in bin/ too, and deleting bin/ wholesale
    # would take the DLL with it.
    rm -f $out/bin/lgx $out/bin/lgx.exe
    rmdir $out/bin 2>/dev/null || true

    # CMake's install rules do not cover the shared library on Unix, so keep
    # copying it there. Windows is fully handled by the install above.
    shopt -s nullglob
    # EVERY entry must contain a wildcard: nullglob only removes patterns that
    # fail to match, and a literal path with no wildcard is left in the array
    # verbatim -- which made an earlier attempt at this die in `cp` rather than
    # in the intended error branch.
    libs=(build/liblgx*.dylib build/liblgx*.so)
    if [ ''${#libs[@]} -gt 0 ]; then
      cp "''${libs[@]}" $out/lib/
    fi

    # Whatever route got us here, refuse to ship an empty library output --
    # the failure this guards against is otherwise completely silent.
    if ! ls $out/lib/*lgx* >/dev/null 2>&1; then
      echo "ERROR: no lgx library artifact landed in $out/lib" >&2
      ls -la build $out/lib >&2
      exit 1
    fi

    # On Windows, also prove the import library actually describes the DLL.
    # A wrong-but-present liblgx.dll.a is invisible here and only surfaces as
    # "undefined reference to `__imp_lgx_*'" in an unrelated downstream repo.
    # stdenv exports NM for the target toolchain on a cross build; fall back to
    # plain nm so this check never itself becomes the reason a build fails.
    nmbin="''${NM:-nm}"
    if [ -f $out/lib/liblgx.dll.a ] && command -v "$nmbin" >/dev/null 2>&1; then
      if ! "$nmbin" $out/lib/liblgx.dll.a 2>/dev/null | grep -q "__imp_lgx_"; then
        echo "ERROR: $out/lib/liblgx.dll.a exports no __imp_lgx_* symbols;" >&2
        echo "       it is probably lgx.exe's import library, not liblgx.dll's." >&2
        "$nmbin" $out/lib/liblgx.dll.a >&2 || true
        exit 1
      fi
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
