# Common build configuration shared across all packages
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
  
  # Common runtime dependencies
  buildInputs = [ 
    pkgs.zlib
    pkgs.icu
    pkgs.nlohmann_json
  ];
  
  # Common CMake flags
  cmakeFlags = [ 
    "-GNinja"
  ];
  
  # Metadata
  meta = with pkgs.lib; {
    description = "lgx - Logos Package Manager CLI";
    platforms = platforms.unix;
  };
}
