# LGX

## Usage

### Create a Package

Create a new skeleton package:

```bash
lgx create mymodule
# Creates mymodule.lgx
```

### Add Variants

Add a single file to a variant:

```bash
lgx add mymodule.lgx --variant linux-amd64 --files ./build/libfoo.so
```

Add a directory to a variant (requires `--main`):

```bash
lgx add mymodule.lgx --variant web --files ./dist --main index.js
```

**Note:** If a variant already exists, it is **completely replaced** (no merging). Use `-y` to skip confirmation:

```bash
lgx add mymodule.lgx -v linux-amd64 -f ./new-build/libfoo.so -y
```

### Remove a Variant

```bash
lgx remove mymodule.lgx --variant linux-amd64
```

### Verify a Package

Validate a package against the LGX specification:

```bash
lgx verify mymodule.lgx
```

Always verifies content hashes (Merkle tree) match the actual package contents.
If the package is signed, also checks the Ed25519 signature and reports whether
the signer's DID is in the trusted keyring.

### Generate a Signing Key

```bash
lgx keygen --name my-key
# Creates ~/.config/logos/keys/my-key.jwk (secret, JWK format)
#         ~/.config/logos/keys/my-key.pub (SSH format)
#         ~/.config/logos/keys/my-key.did (DID string)
# Prints the did:jwk:... DID to stdout
```

### Sign a Package

```bash
lgx sign mymodule.lgx --key my-key
lgx sign mymodule.lgx --key my-key --name "My Organization" --url "https://example.com"
```

Signing recomputes the Merkle tree of content hashes in `manifest.json`
and creates `manifest.sig` with the signer's DID (`did:jwk:...`), an Ed25519 signature
over the manifest bytes, and optional signer metadata.

### Manage Trusted Keys

```bash
# Add a trusted key by DID
lgx keyring add publisher-name did:jwk:eyJjcnYi... --display-name "Publisher" --url "https://..."

# List trusted keys
lgx keyring list

# Remove a trusted key
lgx keyring remove publisher-name
```

Trusted keys are stored as `.json` files in `~/.config/logos/trusted-keys/`.

### Merge Packages

Merge multiple single-variant `.lgx` packages into one multi-variant package:

```bash
lgx merge linux.lgx darwin.lgx -o mymodule.lgx
```

All input packages must have identical manifests (except for the variant-specific `main` field). Fails on duplicate variants unless `--skip-duplicates` is used:

```bash
lgx merge pkg1.lgx pkg2.lgx pkg3.lgx --skip-duplicates -o mymodule.lgx -y
```

### Inspect Package Contents

Since `.lgx` files are just `tar.gz` archives:

```bash
tar -tzf mymodule.lgx
```

## Command Reference

| Command | Description |
|---------|-------------|
| `lgx create <name>` | Create a new skeleton package |
| `lgx add <pkg> --variant <v> --files <path> [--main <relpath>] [-y]` | Add files to a variant |
| `lgx remove <pkg> --variant <v> [-y]` | Remove a variant |
| `lgx extract <pkg> [--variant <v>] [--output <dir>]` | Extract variant contents |
| `lgx merge <pkg1> <pkg2> ... [-o <output>] [--skip-duplicates] [-y]` | Merge packages into one |
| `lgx verify <pkg>` | Validate package structure and signature |
| `lgx sign <pkg> --key <name> [--name "..."] [--url "..."]` | Sign package with Ed25519 key and DID identity |
| `lgx keygen --name <name>` | Generate an Ed25519 signing keypair (outputs DID) |
| `lgx keyring add\|remove\|list` | Manage trusted keys (by DID) |
| `lgx publish <pkg>` | Publish package (TODO) |

## Package Structure

```
mymodule.lgx (tar.gz)
├── manifest.json          # Package metadata
├── manifest.sig           # Optional - Ed25519 signature with DID identity
├── variants/
│   ├── linux-amd64/
│   │   └── libfoo.so
│   ├── darwin-arm64/
│   │   └── libfoo.dylib
│   └── web/
│       └── index.js
├── docs/                  # Optional
└── licenses/              # Optional
```

## Example Workflow

```bash
# Create package
lgx create mylib

# Add variants
lgx add mylib.lgx -v linux-amd64 -f ./out/linux/libmylib.so -y
lgx add mylib.lgx -v darwin-arm64 -f ./out/macos/libmylib.dylib -y

# Verify
lgx verify mylib.lgx

# Optional: sign the package
lgx keygen --name my-key
lgx sign mylib.lgx --key my-key

# Inspect
tar -tzf mylib.lgx

# Extract for use
lgx extract mylib.lgx --variant linux-amd64 --output ./extracted
```


## Building

### Nix (Recommended)

The recommended way to build `lgx` is using Nix, which automatically handles all dependencies:

#### Binary

```bash
nix build '.#lgx'
```

The binary will be available at `./result/bin/lgx`.

#### Library

To build the library (.so/.dylib/.dll):

```bash
nix build '.#lib'
```

The library will be available at:
- `./result/lib/liblgx.dylib` (macOS)
- `./result/lib/liblgx.so` (Linux)
- `./result/lib/lgx.dll` (Windows)

The C API header will be at `./result/include/lgx.h`.

#### Running Tests with Nix

Tests run automatically during `nix build`. The build will fail if any tests do not pass.

**Note:** If you haven't enabled flakes, you'll need to add the experimental features flag:

```bash
nix --extra-experimental-features "nix-command flakes" build '.#lgx'
```

Or enable flakes permanently in your Nix configuration by adding to `~/.config/nix/nix.conf`:

```
experimental-features = nix-command flakes
```

### CMake

If you prefer not to use Nix, you can build with CMake directly.

#### Prerequisites

- CMake 3.16+
- C++17 compiler (GCC 8+, Clang 7+, MSVC 2019+)
- zlib
- ICU

##### macOS (Homebrew)

```bash
brew install cmake icu4c
```

##### Ubuntu/Debian

```bash
sudo apt install cmake libicu-dev zlib1g-dev
```

#### Building

```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

The `lgx` executable will be created in the `build/` directory.

#### Building the Library

To build the library:

```bash
mkdir build
cd build
cmake .. -DLGX_BUILD_SHARED=ON
make -j$(nproc)
```

This will create:
- `build/liblgx.dylib` (macOS) or `build/liblgx.so` (Linux) or `build/lgx.dll` (Windows)
- The C API header is at `src/lgx.h`

#### Building with Tests

```bash
mkdir build
cd build
cmake .. -DLGX_BUILD_TESTS=ON -DLGX_BUILD_SHARED=ON
make -j$(nproc)
```

### Running Tests

```bash
cd build
ctest --output-on-failure
```
