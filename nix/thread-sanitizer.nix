# The concurrency tests -- names containing "ManyThreads" -- under
# ThreadSanitizer. liblgx is called from several threads at once (Basecamp
# checks installed packages concurrently), and a data race there is caught
# reliably only by the sanitizer, not by a plain test run.
{ pkgs, common, src }:

pkgs.stdenv.mkDerivation {
  pname = "${common.pname}-thread-sanitizer";
  version = common.version;

  inherit src;
  inherit (common) nativeBuildInputs buildInputs;

  configurePhase = ''
    runHook preConfigure
    cmake -S . -B build -GNinja \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DLGX_BUILD_TESTS=ON \
      -DCMAKE_CXX_FLAGS=-fsanitize=thread \
      -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=thread
    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    cmake --build build --target lgx_tests
    runHook postBuild
  '';

  # One process per test (ctest), so each starts with liblgx uninitialised.
  # --no-tests=error: a renamed test must not turn this into a silent pass.
  doCheck = true;
  checkPhase = ''
    runHook preCheck
    TSAN_OPTIONS=halt_on_error=1 ctest --test-dir build -R ManyThreads \
      --output-on-failure --no-tests=error
    runHook postCheck
  '';

  installPhase = ''
    touch $out
  '';
}
