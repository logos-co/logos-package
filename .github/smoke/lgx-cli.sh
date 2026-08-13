#!/usr/bin/env bash
# The EXERCISE half of doctests/lgx-cli.test.yaml, as a Windows smoke script.
#
# The nix half of that spec -- its single `nix build '...#lgx' -o lgx` step --
# is not reproduced here: it IS `targets: lgx` in the caller. What remains is
# the 14 steps that only ever drove the built binary, in the same order, with
# the same assertions.
#
# LGX is a variable rather than a literal so this same file runs against a
# native build during development. In CI it is always the .exe.
#
# `set -e` here is redundant -- the smoke action already runs this file with
# `bash -euo pipefail` -- and is kept anyway so the script behaves identically
# when run by hand against a native build, which is how it was developed.
set -euo pipefail
LGX="${LGX:-lgx/bin/lgx.exe}"

# tar is the one thing this script needs that neither the repo nor the harness
# ships. Assert it up front: absent, `tar -tzf` fails midway with a message
# about the archive, which reads as a packaging bug rather than a missing tool.
command -v tar >/dev/null || {
  echo "::error::tar is not on PATH, so the archive-layout assertion cannot run."
  echo "::error::This is a runner-image fact, not a defect in the package."
  exit 1
}

# --- Confirm it runs ---------------------------------------------------------
run "$LGX" --version | tee version.txt
grep -q "lgx version" version.txt

# --- Detect the platform variant ---------------------------------------------
# The spec's `uname` case has no Windows arm and exits 1 there. On this leg the
# variant is known from the target we were built for, so it is stated, not
# probed. `windows-x86_64` is the name lgpm computes and nix-bundle-lgx emits.
echo windows-x86_64 > variant
echo dll            > ext

# --- Create a package --------------------------------------------------------
run "$LGX" create greeter | tee create.txt
grep -q "Created package: greeter.lgx" create.txt
test -f greeter.lgx

run "$LGX" manifest greeter.lgx | tee m1.txt
grep -q "greeter"                m1.txt
grep -q "0.0.1"                  m1.txt
grep -q "Manifest ver.:"         m1.txt
grep -q "0.4.0"                  m1.txt
grep -q "Variants:       (none)" m1.txt
grep -q "Signed:         no"     m1.txt

# --- Add a platform variant --------------------------------------------------
V="$(cat variant)"; E="$(cat ext)"
echo 'stub library' > "libgreeter.$E"
run "$LGX" add greeter.lgx --variant "$V" --files "libgreeter.$E" | tee add.txt
grep -q "Added variant"  add.txt
grep -q "to greeter.lgx" add.txt

tar -tzf greeter.lgx > layout.txt
grep -q "manifest.json"                     layout.txt
grep -q "variants/$V/libgreeter.$E"         layout.txt

run "$LGX" manifest greeter.lgx | tee m2.txt
grep -q "Root hash:"   m2.txt
grep -q "Variants:"    m2.txt
grep -q "libgreeter."  m2.txt

# --- Verify the package ------------------------------------------------------
run "$LGX" verify greeter.lgx | tee verify.txt
grep -q "Package structure is valid: greeter.lgx" verify.txt
grep -q "Package is unsigned"                     verify.txt

# --- Extract a variant -------------------------------------------------------
run "$LGX" extract greeter.lgx --variant "$V" --output ./extracted | tee ex.txt
grep -q "Extracted variant" ex.txt
grep -q "extracted"         ex.txt
test -f "./extracted/$V/libgreeter.$E"

# --- Merge per-platform packages ---------------------------------------------
# These two variant names are labels in a merge scenario, not this machine's
# platform, so they stay exactly as the spec has them.
run "$LGX" create widget >/dev/null && mv widget.lgx widget-linux.lgx
echo 'linux stub' > libwidget.so
run "$LGX" add widget-linux.lgx --variant linux-x86_64 --files libwidget.so | tee a1.txt
grep -q "Added variant" a1.txt

run "$LGX" create widget >/dev/null && mv widget.lgx widget-darwin.lgx
echo 'darwin stub' > libwidget.dylib
run "$LGX" add widget-darwin.lgx --variant darwin-arm64 --files libwidget.dylib | tee a2.txt
grep -q "Added variant" a2.txt

run "$LGX" merge widget-linux.lgx widget-darwin.lgx -o widget.lgx | tee merge.txt
grep -q "Merged 2 packages into widget.lgx (darwin-arm64, linux-x86_64)" merge.txt

run "$LGX" manifest widget.lgx | tee m3.txt
grep -q "darwin-arm64" m3.txt
grep -q "linux-x86_64" m3.txt

run "$LGX" verify widget.lgx | tee v2.txt
grep -q "Package structure is valid: widget.lgx" v2.txt

echo "OK: lgx-cli exercise half passed"
