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
lgx add mymodule.lgx --variant web --files ./dist --main dist/index.js
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
| `lgx verify <pkg>` | Validate package structure |
| `lgx sign <pkg>` | Sign package (TODO) |
| `lgx publish <pkg>` | Publish package (TODO) |

## Package Structure

```
mymodule.lgx (tar.gz)
├── manifest.json          # Package metadata
├── variants/
│   ├── linux-amd64/
│   │   └── libfoo.so
│   ├── darwin-arm64/
│   │   └── libfoo.dylib
│   └── web/
│       └── dist/
│           └── index.js
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

# Inspect
tar -tzf mylib.lgx
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

After building with `nix build '.'` or `nix build '.#all'`, run the tests:

```bash
# Run core tests (including CLI integration tests)
export LGX_BINARY="$(pwd)/result/bin/lgx"
./result/bin/lgx_tests

# Run library API tests
./result/bin/lgx_lib_tests
```

**Note:** The `LGX_BINARY` environment variable tells the CLI tests where to find the `lgx` binary. Without it, the CLI integration tests will be skipped (though all other tests will still run).

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
cmake .. -DLGX_BUILD_TESTS=ON
make -j$(nproc)
```

To also test the library:

```bash
cmake .. -DLGX_BUILD_TESTS=ON -DLGX_BUILD_SHARED=ON
make -j$(nproc)
```

### Running Tests

```bash
cd build
ctest --output-on-failure
```

Or run the test executable directly:

```bash
./build/lgx_tests
```
