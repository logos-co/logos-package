# Project Description

## Project Structure

```
logos-package/
├── CMakeLists.txt              # Build configuration
├── README.md                   # Project documentation
├── flake.nix                   # Nix flake configuration
├── flake.lock                  # Nix flake lock file
├── docs/
│   ├── specs.md                # This specification
│   ├── specs_draft.md          # Draft specifications
│   ├── specs copy.md           # Specification backup
│   └── lib.md                  # Library documentation
├── include/
│   └── lgx/
│       └── lgx.h               # Public API header
├── src/
│   ├── main.cpp                # CLI entry point
│   ├── commands/               # CLI command implementations
│   │   ├── command.cpp/h       # Base command class
│   │   ├── create_command.cpp/h
│   │   ├── add_command.cpp/h
│   │   ├── remove_command.cpp/h
│   │   ├── verify_command.cpp/h
│   │   ├── sign_command.cpp/h
│   │   └── publish_command.cpp/h
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
│   ├── test_package.cpp        # Package operation tests
│   ├── test_manifest.cpp       # Manifest handling tests
│   ├── test_tar_reader.cpp     # Tar reader tests
│   ├── test_tar_writer.cpp     # Tar writer tests
│   ├── test_gzip_handler.cpp   # Gzip handler tests
│   └── test_path_normalizer.cpp # Path normalizer tests
├── examples/
│   └── example_c_usage.c       # C API usage example
├── nix/
│   ├── default.nix             # Nix package definition
│   └── bin.nix                 # Nix binary configuration
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
| `hasVariant(variant) → bool` | Check if variant exists |
| `getVariants() → set<string>` | Get all variant names |
| `getManifest() → Manifest&` | Access manifest |

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
lgx add <pkg.lgx> --variant <v> --files <path> [--main <relpath>] [-y/--yes]
```

**Arguments:**
- `pkg.lgx` - Path to package file
- `--variant, -v` - Variant name (e.g., `linux-amd64`)
- `--files, -f` - Path to file or directory to add
- `--main, -m` - (Optional for files, required for directories) Path to main entry point
- `--yes, -y` - Skip confirmation prompts

**Behavior:**
- If variant exists, it is **completely replaced**
- Single file: placed at `variants/<variant>/<filename>`
- Directory: placed at `variants/<variant>/<dirname>/...`

**Examples:**
```bash
# Add single file
lgx add mymodule.lgx --variant linux-amd64 --files ./libfoo.so

# Add directory with explicit main
lgx add mymodule.lgx -v web -f ./dist --main dist/index.js

# Replace without confirmation
lgx add mymodule.lgx -v darwin-arm64 -f ./build -m build/lib.dylib -y
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

### lgx verify

Validate a package against the specification.

```
lgx verify <pkg.lgx>
```

**Arguments:**
- `pkg.lgx` - Path to package file

**Exit Codes:**
- `0` - Package is valid
- `1` - Validation failed

**Example:**
```bash
lgx verify mymodule.lgx
# Package is valid: mymodule.lgx
```

### lgx sign

Sign a package (not implemented in v0.1).

```
lgx sign <pkg.lgx>
```

**Status:** Exits with code 1, does not write `manifest.cose`

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
   lgx add mymodule.lgx -v web -f ./dist --main dist/index.js
   ┌──────────────────────────────┐
   │ mymodule.lgx                 │
   │ ├─manifest.json              │
   │ └─variants/                  │
   │   ├─linux-amd64/             │
   │   │ └─lib.so                 │
   │   └─web/                     │
   │     └─dist/                  │
   │       ├─index.js             │
   │       └─...                  │
   └──────────────────────────────┘

3. VERIFY
   lgx verify mymodule.lgx
   → Validates structure, manifest, completeness

4. DISTRIBUTE
   (share mymodule.lgx)

5. CONSUME
   (extract and use variant-specific files)
```

## Operational

### Prerequisites

- CMake 3.16 or higher
- C++17 compatible compiler (GCC 8+, Clang 7+, MSVC 2019+)
- zlib development libraries
- ICU development libraries

**macOS (Homebrew):**
```bash
brew install cmake icu4c
```

**Ubuntu/Debian:**
```bash
sudo apt install cmake libicu-dev zlib1g-dev
```

### Building

```bash
cd logos-package
mkdir -p build
cd build
cmake ..
make -j$(nproc)
```

The `lgx` executable will be created in the `build/` directory.

### Installation

```bash
cd build
sudo make install
# Installs lgx to /usr/local/bin
```

### Running Tests

(Tests not yet implemented)

```bash
cd build
ctest
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

### Using the Library Programmatically

```cpp
#include <lgx/lgx.h>

// Create a package
auto result = lgx::Package::create("mypackage.lgx", "mypackage");
if (!result.success) {
    std::cerr << "Error: " << result.error << std::endl;
    return 1;
}

// Load and modify
auto pkgOpt = lgx::Package::load("mypackage.lgx");
if (pkgOpt) {
    auto& pkg = *pkgOpt;
    
    // Add a variant
    pkg.addVariant("linux-amd64", "./build/lib.so");
    
    // Access manifest
    pkg.getManifest().description = "My awesome library";
    
    // Save changes
    pkg.save("mypackage.lgx");
}

// Verify a package
auto verifyResult = lgx::Package::verify("mypackage.lgx");
if (!verifyResult.valid) {
    for (const auto& err : verifyResult.errors) {
        std::cerr << "Error: " << err << std::endl;
    }
}
```

---

## Other

### Known Issues

1. **Deterministic JSON not fully specified** - The exact canonical JSON format (whitespace, key ordering, escaping) is not yet locked down. Current implementation uses nlohmann/json's default pretty-print with 2-space indent.

### Future Improvements

1. **Signature support** - Implement COSE signatures via `manifest.cose`
2. **Registry integration** - Implement `lgx publish` with future package manager & registry
3. **Extraction command** - Add `lgx extract <pkg.lgx> [--variant <v>] [--output <dir>]`
4. **List command** - Add `lgx list <pkg.lgx>` to show package contents
5. **Info command** - Add `lgx info <pkg.lgx>` to show manifest details
6. **Deterministic JSON** - Adopt RFC 8785 (JCS) or define canonical format
