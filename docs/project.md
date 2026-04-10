# Project Description

## Project Structure

```
logos-package/
├── CMakeLists.txt              # Build configuration
├── README.md                   # Project documentation
├── flake.nix                   # Nix flake configuration
├── flake.lock                  # Nix flake lock file
├── docs/
│   ├── project.md              # This specification
│   ├── specs.md                # High level specification
├── src/
│   ├── lgx.h                   # C API header
│   ├── lib.cpp                 # C API implementation
│   ├── main.cpp                # CLI entry point
│   ├── commands/               # CLI command implementations
│   │   ├── command.cpp/h       # Base command class
│   │   ├── create_command.cpp/h
│   │   ├── add_command.cpp/h
│   │   ├── remove_command.cpp/h
│   │   ├── extract_command.cpp/h
│   │   ├── verify_command.cpp/h
│   │   ├── merge_command.cpp/h
│   │   ├── sign_command.cpp/h
│   │   ├── keygen_command.cpp/h
│   │   ├── keyring_command.cpp/h
│   │   └── publish_command.cpp/h
│   ├── crypto/                 # Cryptographic operations
│   │   ├── signing.cpp/h       # Ed25519 sign/verify, SHA-256, Merkle tree, DID utilities
│   │   ├── keyring.cpp/h       # Directory-based trust store (JSON format, DID-based)
│   │   └── manifest_sig.cpp/h  # manifest.sig JSON format (DID, signer metadata, linkedDids)
│   └── core/                   # Core library
│       ├── package.cpp/h       # High-level package operations
│       ├── manifest.cpp/h      # Manifest JSON handling
│       ├── tar_writer.cpp/h    # Deterministic tar creation
│       ├── tar_reader.cpp/h    # Tar extraction/reading
│       ├── gzip_handler.cpp/h  # Deterministic gzip
│       └── path_normalizer.cpp/h # Unicode NFC + path security
├── tests/                      # Test suite
│   ├── CMakeLists.txt          # Test build configuration
│   ├── test_cli.cpp            # CLI command tests
│   ├── test_lib.cpp            # C API library tests
│   ├── test_package.cpp        # Package operation tests
│   ├── test_crypto.cpp         # Crypto tests (base64url, DID, ManifestSig, Keyring, signing)
│   ├── test_manifest.cpp       # Manifest handling tests
│   ├── test_tar_reader.cpp     # Tar reader tests
│   ├── test_tar_writer.cpp     # Tar writer tests
│   ├── test_gzip_handler.cpp   # Gzip handler tests
│   └── test_path_normalizer.cpp # Path normalizer tests
├── examples/
│   └── example_c_usage.c       # C API usage example
├── nix/
│   ├── default.nix             # Nix package definition
│   ├── bin.nix                 # Nix binary configuration
│   ├── lib.nix                 # Nix library configuration
│   └── all.nix                 # Nix all-in-one package (binary + library + tests)
├── .github/
│   └── workflows/
│       └── ci.yml              # GitHub Actions CI workflow
└── build/                      # Build output (generated)
```

## Stack, Frameworks & Dependencies

| Component | Purpose |
|-----------|---------|
| **C++17** | Implementation language |
| **CMake 3.16+** | Build system |
| **zlib** | Gzip compression/decompression |
| **ICU** | Unicode NFC normalization |
| **nlohmann/json** | JSON parsing and serialization |
| **libsodium** | Ed25519 signing/verification, SHA-256 hashing |
| **Google Test** | Unit testing framework (optional, for tests) |
| **Nix** | Package management and reproducible builds |

## Core Modules

### PathNormalizer

**Files:** `src/core/path_normalizer.cpp`, `src/core/path_normalizer.h`

**Purpose:** Unicode NFC normalization and path security validation.

**API:**

| Method | Description |
|--------|-------------|
| `toNFC(path) → optional<string>` | Normalize path to Unicode NFC form |
| `isNFC(str) → bool` | Check if string is already NFC-normalized |
| `validateArchivePath(path) → ValidationResult` | Validate path against security rules |
| `toLowercase(str) → string` | Convert string to lowercase (Unicode-aware) |
| `normalizeSeparators(path) → string` | Normalize path separators to forward slash |
| `joinPath(base, relative) → string` | Join path components |
| `basename(path) → string` | Extract filename from path |
| `dirname(path) → string` | Extract directory from path |
| `isAbsolute(path) → bool` | Check if path is absolute |
| `splitPath(path) → vector<string>` | Split path into components |

### GzipHandler

**Files:** `src/core/gzip_handler.cpp`, `src/core/gzip_handler.h`

**Purpose:** Deterministic gzip compression and decompression.

**Determinism Settings:**
- Header mtime = 0
- No original filename
- OS byte = 0xFF (unknown)

**API:**

| Method | Description |
|--------|-------------|
| `compress(data) → vector<uint8_t>` | Compress data with deterministic settings |
| `decompress(data) → vector<uint8_t>` | Decompress gzip data |
| `isGzipData(data) → bool` | Check if data has gzip magic bytes |
| `getLastError() → string` | Get last error message |

### DeterministicTarWriter

**Files:** `src/core/tar_writer.cpp`, `src/core/tar_writer.h`

**Purpose:** Create USTAR format tar archives with deterministic output.

**Fixed Metadata:**
- `uid=0`, `gid=0`
- `uname=""`, `gname=""`
- `mtime=0`
- Directories: mode `0755`
- Files: mode `0644`

**API:**

| Method | Description |
|--------|-------------|
| `addFile(path, data)` | Add file entry |
| `addDirectory(path)` | Add directory entry |
| `addEntry(TarEntry)` | Add generic entry |
| `finalize() → vector<uint8_t>` | Sort entries and generate tar data |
| `clear()` | Clear all entries |
| `entryCount() → size_t` | Get number of entries |

### TarReader

**Files:** `src/core/tar_reader.cpp`, `src/core/tar_reader.h`

**Purpose:** Read and extract entries from tar archives.

**API:**

| Method | Description |
|--------|-------------|
| `read(tarData) → ReadResult` | Read all entries from tar |
| `readInfo(tarData) → vector<EntryInfo>` | Read entry metadata only |
| `readFile(tarData, path) → optional<vector<uint8_t>>` | Extract single file by path |
| `iterate(tarData, callback) → bool` | Iterate entries with callback |
| `isValidTar(tarData) → bool` | Basic tar validity check |

### Manifest

**Files:** `src/core/manifest.cpp`, `src/core/manifest.h`

**Purpose:** Parse, validate, and serialize manifest.json.

**API:**

| Method | Description |
|--------|-------------|
| `fromJson(json) → optional<Manifest>` | Parse manifest from JSON string |
| `toJson() → string` | Serialize to deterministic JSON |
| `validate() → ValidationResult` | Validate manifest fields |
| `validateCompleteness(variants) → ValidationResult` | Validate variants ↔ main mapping |
| `setMain(variant, path)` | Set main entry (auto-lowercase key) |
| `removeMain(variant)` | Remove main entry |
| `getMain(variant) → optional<string>` | Get main entry (case-insensitive) |
| `getVariants() → set<string>` | Get all variant names from main |
| `compareMetadata(other) → ValidationResult` | Compare non-variant fields with another manifest |
| `isVersionSupported(version) → bool` | Check if manifest version is supported |

### Package

**Files:** `src/core/package.cpp`, `src/core/package.h`

**Purpose:** High-level package operations combining all components.

**API:**

| Method | Description |
|--------|-------------|
| `create(path, name) → Result` | Create new skeleton package |
| `load(path) → optional<Package>` | Load existing package |
| `save(path) → Result` | Save package to file |
| `verify(path) → VerifyResult` | Validate package against spec |
| `addVariant(variant, filesPath, mainPath) → Result` | Add/replace variant |
| `removeVariant(variant) → Result` | Remove variant |
| `extractVariant(variant, outputDir) → Result` | Extract variant to directory |
| `extractAll(outputDir) → Result` | Extract all variants to directory |
| `hasVariant(variant) → bool` | Check if variant exists |
| `getVariants() → set<string>` | Get all variant names |
| `getManifest() → Manifest&` | Access manifest |
| `signPackage(secretKey, name, url) → Result` | Sign package with Ed25519 key |
| `verifySignature() → SignatureInfo` | Verify signature and package integrity |
| `validatePackage() → Result` | Validate structure and content hashes |

## C API Library

**Files:** `src/lgx.h`, `src/lib.cpp`

**Purpose:** C API wrapper for cross-language interoperability. Useful for using this lib in other projects.

### Building the Library

The library can be built as a shared library (optional). Enable it with CMake:

```bash
cmake -DLGX_BUILD_SHARED=ON ..
make
```

This produces:
- `liblgx.so` on Linux
- `liblgx.dylib` on macOS  
- `lgx.dll` on Windows

The header file `src/lgx.h` is installed to `include/` when using `make install`.

### API Reference

**Package Creation and Loading:**
- `lgx_create(output_path, name) → lgx_result_t` - Create a new skeleton package
- `lgx_load(path) → lgx_package_t` - Load an existing package from file (returns NULL on error)
- `lgx_save(pkg, path) → lgx_result_t` - Save a package to file
- `lgx_verify(path) → lgx_verify_result_t` - Verify a package file

**Package Manipulation:**
- `lgx_add_variant(pkg, variant, files_path, main_path) → lgx_result_t` - Add/replace variant
- `lgx_remove_variant(pkg, variant) → lgx_result_t` - Remove a variant
- `lgx_extract(pkg, variant, output_dir) → lgx_result_t` - Extract variant(s) to directory (variant=NULL extracts all)
- `lgx_has_variant(pkg, variant) → bool` - Check if variant exists
- `lgx_get_variants(pkg) → const char**` - Get NULL-terminated array of variant names (free with `lgx_free_string_array`)

**Manifest Access:**
- `lgx_get_name(pkg) → const char*` - Get package name (owned by library)
- `lgx_get_version(pkg) → const char*` - Get package version (owned by library)
- `lgx_set_version(pkg, version) → lgx_result_t` - Set package version
- `lgx_get_description(pkg) → const char*` - Get package description (owned by library)
- `lgx_set_description(pkg, description)` - Set package description
- `lgx_get_icon(pkg) → const char*` - Get package icon path (owned by library)
- `lgx_set_icon(pkg, icon)` - Set package icon path
- `lgx_get_manifest_json(pkg) → const char*` - Get the full manifest as a JSON string (owned by library)

**Memory Management:**
- `lgx_free_package(pkg)` - Free a package handle
- `lgx_free_string_array(array)` - Free string array returned by library functions
- `lgx_free_verify_result(result)` - Free verification result structure

**Signing and Verification:**
- `lgx_sign(lgx_path, secret_key_path, signer_name, signer_url) → lgx_result_t` - Sign a package
- `lgx_verify_signature(lgx_path, keyring_dir) → lgx_signature_info_t` - Verify package signature
- `lgx_free_signature_info(info)` - Free signature info structure

**Key Management:**
- `lgx_keygen(name, output_dir) → lgx_result_t` - Generate Ed25519 signing keypair (output_dir=NULL for default)
- `lgx_keyring_add(keyring_dir, name, did, display_name, url) → lgx_result_t` - Add trusted key (keyring_dir=NULL for default)
- `lgx_keyring_remove(keyring_dir, name) → lgx_result_t` - Remove trusted key (keyring_dir=NULL for default)
- `lgx_keyring_list(keyring_dir) → lgx_keyring_list_t` - List trusted keys (keyring_dir=NULL for default)
- `lgx_free_keyring_list(list)` - Free keyring list structure

**Error Handling:**
- `lgx_get_last_error() → const char*` - Get the last error message (thread-local storage)

**Version Info:**
- `lgx_version() → const char*` - Get library version string (e.g., "0.1.0")

### Result Types

```c
typedef struct {
    bool success;
    const char* error;  /* NULL if success, owned by library */
} lgx_result_t;

typedef struct {
    bool valid;
    const char** errors;   /* NULL-terminated array, owned by library */
    const char** warnings; /* NULL-terminated array, owned by library */
} lgx_verify_result_t;
```

### Usage

Include the header:
```c
#include <lgx.h>
```

Link against the library:
```bash
gcc -o program program.c -llgx -L/path/to/lib -I/path/to/include
```

See `examples/example_c_usage.c` for a complete example.

## CLI Commands

### lgx create

Create a new skeleton package.

```
lgx create <name>
```

**Arguments:**
- `name` - Package name (will be lowercased)

**Output:** Creates `<name>.lgx` in current directory

**Example:**
```bash
lgx create mymodule
# Creates mymodule.lgx
```

### lgx add

Add files to a package variant.

```
lgx add <pkg.lgx> --variant <v> --files <path> [--main <relpath>] [--view <relpath>] [-y/--yes]
```

**Arguments:**
- `pkg.lgx` - Path to package file
- `--variant, -v` - Variant name (e.g., `linux-amd64`)
- `--files, -f` - Path to file or directory to add
- `--main, -m` - Path to main entry point. Optional for files, required for most directory variants, and optional for `ui_qml` where `view` is the required entry point and `main` is backend-only metadata
- `--view` - QML entry point relative to variant root. Required for `ui_qml` packages. Sets the manifest-level `view` field
- `--yes, -y` - Skip confirmation prompts

For `type == "ui_qml"` manifests, `view` (the QML entry point) is required.
`main`, when present, is the backend Qt plugin library path.

**Behavior:**
- If variant exists, it is **completely replaced**
- Single file: placed at `variants/<variant>/<filename>`
- Directory: contents placed at `variants/<variant>/...`

**Examples:**
```bash
# Add single file
lgx add mymodule.lgx --variant linux-amd64 --files ./libfoo.so

# Add directory with explicit main
lgx add mymodule.lgx -v web -f ./dist --main index.js

# Add ui_qml variant with view
lgx add mymodule.lgx -v darwin-arm64 -f ./dist --view qml/Main.qml

# Replace without confirmation
lgx add mymodule.lgx -v darwin-arm64 -f ./build -m lib.dylib -y
```

### lgx remove

Remove a variant from a package.

```
lgx remove <pkg.lgx> --variant <v> [-y/--yes]
```

**Arguments:**
- `pkg.lgx` - Path to package file
- `--variant, -v` - Variant name to remove
- `--yes, -y` - Skip confirmation prompt

**Example:**
```bash
lgx remove mymodule.lgx --variant linux-amd64
```

### lgx extract

Extract variant contents from a package.

```
lgx extract <pkg.lgx> [--variant <v>] [--output <dir>]
```

**Arguments:**
- `pkg.lgx` - Path to package file
- `--variant, -v` - (Optional) Variant name to extract (extracts all if omitted)
- `--output, -o` - (Optional) Output directory (defaults to current directory)

**Output Structure:**
- Each variant is extracted to `<output>/<variant-name>/`

**Examples:**
```bash
# Extract all variants to current directory
lgx extract mymodule.lgx

# Extract specific variant
lgx extract mymodule.lgx --variant linux-amd64

# Extract to specific directory
lgx extract mymodule.lgx -v web -o ./extracted
```

### lgx verify

Validate a package against the specification.

```
lgx verify <pkg.lgx> [--keyring-dir <dir>]
```

**Arguments:**
- `pkg.lgx` - Path to package file
- `--keyring-dir <dir>` - (Optional) Keyring directory for trust lookup (default: `~/.config/logos/trusted-keys/`)

**Exit Codes:**
- `0` - Package is valid
- `1` - Validation failed

**Example:**
```bash
lgx verify mymodule.lgx
# Package is valid: mymodule.lgx

lgx verify mymodule.lgx --keyring-dir /path/to/keyring
```

### lgx merge

Merge multiple `.lgx` packages into a single multi-variant package.

```
lgx merge <pkg1.lgx> <pkg2.lgx> ... [-o <output.lgx>] [--skip-duplicates] [-y/--yes]
```

**Arguments:**
- `pkg1.lgx`, `pkg2.lgx`, ... - Two or more input package files
- `--output, -o` - (Optional) Output path (defaults to `<name>.lgx`)
- `--skip-duplicates` - (Optional) Warn and skip duplicate variants instead of failing
- `--yes, -y` - Skip confirmation prompts

**Behavior:**
- All input manifests must match (ignoring the variant-specific `main` field)
- By default, fails if any variant appears in more than one input package
- With `--skip-duplicates`, keeps the first occurrence and warns about duplicates
- Creates a fresh output package and adds each variant via the standard `addVariant` flow

**Examples:**
```bash
# Merge two single-variant packages
lgx merge linux.lgx darwin.lgx -o mymodule.lgx

# Merge three packages, skip confirmation
lgx merge linux-amd64.lgx linux-arm64.lgx darwin-arm64.lgx -o mymodule.lgx -y

# Merge with duplicate tolerance
lgx merge pkg1.lgx pkg2.lgx --skip-duplicates -y
```

### lgx keygen

Generate an Ed25519 signing keypair.

```
lgx keygen --name <name> [--output-dir <dir>]
```

**Arguments:**
- `--name, -n <name>` - Name for the keypair (required)
- `--output-dir, -o <dir>` - (Optional) Directory to write key files (default: `~/.config/logos/keys/`)

Creates `<dir>/<name>.jwk` (secret key, JWK format, 0600), `<name>.pub` (SSH format), and `<name>.did` (DID string). Prints the `did:jwk:...` DID to stdout.

**Examples:**
```bash
lgx keygen --name my-key
lgx keygen --name ci-key --output-dir /etc/logos/keys
```

### lgx sign

Sign a package with an Ed25519 key and DID identity.

```
lgx sign <pkg.lgx> --key <name> [--keys-dir <dir>] [--name "Display Name"] [--url "https://..."]
```

**Arguments:**
- `--key, -k <name>` - Name of the signing key (required)
- `--keys-dir, -d <dir>` - (Optional) Directory containing key files (default: `~/.config/logos/keys/`)
- `--name <display-name>` - Signer display name (self-asserted metadata)
- `--url <url>` - Signer URL (self-asserted metadata)

Validates the package, then creates `manifest.sig` with the signer's DID, Ed25519 signature, and optional signer metadata.

**Examples:**
```bash
lgx sign mymodule.lgx --key my-key
lgx sign mymodule.lgx --key ci-key --keys-dir /etc/logos/keys --name "CI Build" --url "https://ci.example.com"
```

### lgx keyring

Manage trusted keys by DID.

```
lgx keyring add <name> <did:jwk:...> [--display-name "..."] [--url "..."] [--dir <dir>]
lgx keyring remove <name> [--dir <dir>]
lgx keyring list [--dir <dir>]
```

**Arguments:**
- `--dir, -d <dir>` - (Optional) Keyring directory (default: `~/.config/logos/trusted-keys/`)

Keys are stored as `.json` files in the keyring directory containing the DID and optional metadata.

**Examples:**
```bash
lgx keyring add publisher did:jwk:eyJj... --display-name "Publisher" --url "https://..."
lgx keyring list
lgx keyring list --dir /etc/logos/trusted-keys
lgx keyring remove publisher
```

### lgx publish

Publish a package (no-op in v0.1).

```
lgx publish <pkg.lgx>
```

**Status:** No-op, exits with code 0

---

## Sequence Flows

### Full Package Lifecycle

```
1. CREATE
   lgx create mymodule
   ┌─────────────────┐
   │ mymodule.lgx    │
   │ ├─manifest.json │
   │ └─variants/     │
   └─────────────────┘

2. ADD VARIANTS
   lgx add mymodule.lgx -v linux-amd64 -f ./build/lib.so
   lgx add mymodule.lgx -v web -f ./dist --main index.js
   ┌──────────────────────────────┐
   │ mymodule.lgx                 │
   │ ├─manifest.json              │
   │ └─variants/                  │
   │   ├─linux-amd64/             │
   │   │ └─lib.so                 │
   │   └─web/                     │
   │     ├─index.js               │
   │     └─...                    │
   └──────────────────────────────┘

3. MERGE (alternative to step 2 — combine per-platform CI outputs)
   lgx merge linux-amd64.lgx darwin-arm64.lgx -o mymodule.lgx
   → Validates manifests match, combines all variants

4. VERIFY
   lgx verify mymodule.lgx
   → Validates structure, manifest, completeness, content hashes

5. SIGN (optional)
   lgx keygen --name my-key
   lgx sign mymodule.lgx --key my-key --name "My Org" --url "https://example.com"
   → Creates manifest.sig with DID identity and Ed25519 signature

6. TRUST MANAGEMENT
   lgx keyring add my-org did:jwk:eyJj... --display-name "My Org"
   lgx keyring list
   → Manage trusted signing keys for verification

7. EXTRACT (for consumption)
   lgx extract mymodule.lgx --variant linux-amd64 --output ./extracted
   ┌──────────────────────────────┐
   │ ./extracted/                 │
   │ └─linux-amd64/               │
   │   └─lib.so                   │
   └──────────────────────────────┘

8. DISTRIBUTE
   (share mymodule.lgx)
```

## Operational

###  Nix (recommended)

Nix provides reproducible builds with all dependencies managed automatically.

**Building CLI Executable Only:**

To build just the `lgx` CLI binary:

```bash
nix build '.#lgx'
```

The binary will be available at `result/bin/lgx`.

**Building Shared Library Only:**

To build just the shared library and C API header:

```bash
nix build '.#lib'
```

This produces:
- `result/lib/liblgx.so` (Linux)
- `result/lib/liblgx.dylib` (macOS)
- `result/lib/lgx.dll` (Windows)
- `result/include/lgx.h` (C API header)

**Building Everything (Binary + Library + Tests):**

To build the CLI binary, shared library, and all test executables together:

```bash
nix build '.#all'
```

This is the default package and includes:
- `result/bin/lgx` - CLI executable
- `result/lib/liblgx.so` (or `.dylib`/`.dll`) - Shared library
- `result/include/lgx.h` - C API header
- `result/bin/lgx_tests` - Core test suite
- `result/bin/lgx_lib_tests` - Library API tests

**Running Tests with Nix:**

When building with `nix build '.#all'`, tests are automatically run during the build process. To run tests manually after building:

```bash
# Build everything including tests
nix build '.#all'

# Run core tests (including CLI integration tests)
export LGX_BINARY="$(pwd)/result/bin/lgx"
./result/bin/lgx_tests

# Run library API tests
./result/bin/lgx_lib_tests
```

**Note:** The `LGX_BINARY` environment variable tells the CLI tests where to find the `lgx` binary. Without it, the CLI integration tests will be skipped (though all other tests will still run).

### CMake

**Prerequisites:**

- CMake 3.16 or higher
- C++17 compatible compiler (GCC 8+, Clang 7+, MSVC 2019+)
- zlib development libraries
- ICU development libraries
- Google Test (optional, only required if building tests)

**macOS (Homebrew):**
```bash
brew install cmake icu4c
```

**Ubuntu/Debian:**
```bash
sudo apt install cmake libicu-dev zlib1g-dev
```

CMake provides traditional build system support for development and integration.

**Building CLI Executable Only:**

To build just the `lgx` CLI binary:

```bash
cd logos-package
mkdir -p build
cd build
cmake ..
make -j$(nproc)
```

The `lgx` executable will be created in the `build/` directory.

**Building Shared Library Only:**

To build just the shared library:

```bash
cd logos-package
mkdir -p build
cd build
cmake -DLGX_BUILD_SHARED=ON ..
make -j$(nproc)
```

This produces `liblgx.so` (Linux), `liblgx.dylib` (macOS), or `lgx.dll` (Windows) in the `build/` directory. The C API header `src/lgx.h` is installed to `include/` when using `make install`.

**Building with Tests:**

To build the CLI binary, shared library, and test executables:

```bash
cd logos-package
mkdir -p build
cd build
cmake -DLGX_BUILD_TESTS=ON -DLGX_BUILD_SHARED=ON ..
make -j$(nproc)
```

This builds:
- `build/lgx` - CLI executable
- `build/liblgx.so` (or `.dylib`/`.dll`) - Shared library
- `build/tests/lgx_tests` - Core test suite
- `build/tests/lgx_lib_tests` - Library API tests

**Running Tests with CMake:**

Tests are built using Google Test and can be run via CMake's CTest:

```bash
cd build
ctest --output-on-failure
```

**Installation:**

To install the built binaries and libraries system-wide:

```bash
cd build
sudo make install
# Installs lgx to /usr/local/bin
# Installs liblgx to /usr/local/lib (if built with -DLGX_BUILD_SHARED=ON)
# Installs lgx.h to /usr/local/include (if built with -DLGX_BUILD_SHARED=ON)
```

---

## Examples

### Creating and Populating a Package

```bash
# Create new package
lgx create mylib

# Build for different platforms
# ./out/linux/libmylib.so
# ./out/macos/libmylib.dylib

# Add variants
lgx add mylib.lgx -v linux-amd64 -f ./out/linux/libmylib.so -y
lgx add mylib.lgx -v darwin-arm64 -f ./out/macos/libmylib.dylib -y

# Verify
lgx verify mylib.lgx

# Inspect contents
tar -tzf mylib.lgx
```

### Merging Per-Platform CI Outputs

When each platform builds a single-variant `.lgx` in CI, merge them into a single distributable package:

```bash
# Each CI job produces a single-variant package:
#   linux-amd64/mylib.lgx   (contains linux-amd64 variant only)
#   darwin-arm64/mylib.lgx  (contains darwin-arm64 variant only)

# Merge into one multi-variant package
lgx merge linux-amd64/mylib.lgx darwin-arm64/mylib.lgx -o mylib.lgx -y

# Verify the result
lgx verify mylib.lgx
```

### Using the Library Programmatically

**C API:**
```c
#include <lgx.h>
#include <stdio.h>

int main(void) {
    // Create a package
    lgx_result_t result = lgx_create("mypackage.lgx", "mypackage");
    if (!result.success) {
        fprintf(stderr, "Error: %s\n", result.error);
        return 1;
    }
    
    // Load and modify
    lgx_package_t pkg = lgx_load("mypackage.lgx");
    if (pkg) {
        lgx_set_version(pkg, "1.0.0");
        lgx_set_description(pkg, "My awesome library");
        
        // Add a variant
        lgx_add_variant(pkg, "linux-amd64", "./build/lib.so", "lib.so");
        
        // Save changes
        lgx_save(pkg, "mypackage.lgx");
        
        // Cleanup
        lgx_free_package(pkg);
    }
    
    // Verify a package
    lgx_verify_result_t verify = lgx_verify("mypackage.lgx");
    if (!verify.valid) {
        if (verify.errors) {
            for (int i = 0; verify.errors[i]; i++) {
                fprintf(stderr, "Error: %s\n", verify.errors[i]);
            }
        }
    }
    lgx_free_verify_result(verify);
    
    return 0;
}
```

## Other

### Continuous Integration

The project uses GitHub Actions for automated testing. On every push or pull request to the `master` branch:

1. The project is built using Nix (`nix build`)
2. Core tests are run (`lgx_tests`)
3. Library API tests are run (`lgx_lib_tests`)

The workflow configuration is in `.github/workflows/ci.yml`.

### Known Issues

1. **Deterministic JSON not fully specified** - (note: TBC) The exact canonical JSON format (whitespace, key ordering, escaping) is not yet locked down. Current implementation uses nlohmann/json's default pretty-print with 2-space indent. Without canonical JSON, different serializers can emit different bytes for the same data (key order/whitespace/escaping), which breaks reproducible archives and any future signing/hashing over `manifest.json`. This breaks the core requirement that identical package contents must produce byte-identical package files, which is essential for reproducibility, caching, and security verification via content hashes.

### Future Improvements

1. **Registry integration** - Implement `lgx publish` with future package manager & registry
2. **List command** - Add `lgx list <pkg.lgx>` to show package contents
3. **Info command** - Add `lgx info <pkg.lgx>` to show manifest details
4. **Load-time verification** - Defense-in-depth signature verification at dlopen time in liblogos
