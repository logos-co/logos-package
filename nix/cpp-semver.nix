# z4kn4fein/cpp-semver — the SemVer 2.0.0 precedence engine behind
# include/logos/semver.hpp.
#
# Not in nixpkgs (the only packaged C++ semver lib, neargye-semver, models
# the pre-release as an enum {alpha,beta,rc} + uint16 and so cannot represent
# `1.0.0-alpha.beta` or build metadata — it is not actually 2.0.0 compliant).
# Nix builds are sandboxed, so CMakeLists' FetchContent fallback can't reach
# the network here; this derivation is what makes `find_package(semver)` hit.
{ pkgs }:

pkgs.stdenv.mkDerivation rec {
  pname = "cpp-semver";
  version = "0.4.0";

  src = pkgs.fetchFromGitHub {
    owner = "z4kn4fein";
    repo = "cpp-semver";
    rev = "b2f4696a2dfac6a0b8d221529471a982e561f539"; # v0.4.0
    sha256 = "0gfvchvh4qngjvdmd7vlfdch73br4hnp69sq0gkyszf0lpx2qp27";
  };

  nativeBuildInputs = [ pkgs.cmake pkgs.ninja ];

  # Header-only: SEMVER_INSTALL installs the header plus the semver::semver
  # CMake config package. Upstream's own test suite needs Catch2 from the
  # network, so it stays off — our spec coverage lives in tests/test_semver.cpp.
  cmakeFlags = [
    "-GNinja"
    "-DSEMVER_BUILD_TESTS=OFF"
    "-DSEMVER_INSTALL=ON"
  ];

  meta = with pkgs.lib; {
    description = "C++17 header-only SemVer 2.0.0 library";
    homepage = "https://github.com/z4kn4fein/cpp-semver";
    license = licenses.mit;
    platforms = platforms.unix;
  };
}
