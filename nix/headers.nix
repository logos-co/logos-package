# The header-only surface of lgx: include/ and nothing else.
#
# For consumers that need the shared semver implementation
# (include/logos/semver.hpp) but must NOT link liblgx — the package-manager UI
# is a Qt plugin, and the module builder copies every *.so/*.dylib an external
# library ships into the plugin's output lib/. ui-host then scans that directory
# and tries to load liblgx as a Qt plugin, which fails and takes the whole UI
# down with it.
#
# Shipping no library at all is what makes this safe: there is nothing to copy.
{ pkgs, common, src }:

pkgs.runCommand "${common.pname}-headers-${common.version}" { } ''
  mkdir -p $out/include/semver

  cp ${src}/src/lgx.h $out/include/
  cp -r ${src}/include/logos $out/include/

  # logos/semver.hpp includes <semver/semver.hpp>, so vendor it alongside —
  # one include dir is then enough for any consumer.
  cp ${common.cpp-semver}/include/semver/semver.hpp $out/include/semver/
''
