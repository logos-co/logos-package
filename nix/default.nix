# Common build configuration shared across all packages
# libiconv, ICU, and GTest are linked statically for portability.
{ pkgs }:

{
  pname = "lgx";
  version = "0.1.0";
  
  # Common native build inputs
  nativeBuildInputs = [ 
    pkgs.cmake 
    pkgs.ninja 
    pkgs.pkg-config
  ];
  
  # Common runtime dependencies (static iconv/ICU, static gtest where possible)
  buildInputs = [ 
    pkgs.zlib
    pkgs.pkgsStatic.libiconv
    pkgs.pkgsStatic.icu
    pkgs.nlohmann_json
    # GTest built static so test binaries don't depend on libgtest at runtime
    (pkgs.gtest.overrideAttrs (old: {
      cmakeFlags = (old.cmakeFlags or [ ]) ++ [ "-DBUILD_SHARED_LIBS=OFF" ];
    }))
  ];
  
  # Common CMake flags
  cmakeFlags = [ 
    "-GNinja"
  ];
  
  # Ensure static libs from buildInputs are not stripped from the closure
  dontDisableStatic = true;
  
  # Metadata
  meta = with pkgs.lib; {
    description = "lgx - Logos Package Manager CLI";
    platforms = platforms.unix;
  };
}
