# LGX Package

## Overall Description

LGX is a deterministic package format for distributing multi-platform artifacts. It provides a standardized way to bundle platform-specific binaries, libraries, or other files into a single distributable archive with strong guarantees about reproducibility and integrity.

The format is designed to:
- Support multiple platform variants (e.g., `linux-amd64`, `darwin-arm64`) in a single package
- Produce byte-identical archives given identical inputs (deterministic)
- Ensure cross-platform path compatibility through Unicode NFC normalization (macOS often uses decomposed forms; Linux commonly uses composed - NFC avoids "same name, different bytes" breaking lookups, hashing, and determinism)
- Enforce a strict, predictable internal structure
- Provide tooling for creating, modifying, and validating packages

## Definitions & Acronyms

| Term | Definition |
|------|------------|
| **LGX** | Logos Package Format - the package format specified in this document |
| **Variant** | A platform-specific or configuration-specific build of the package contents (e.g., `linux-amd64`) |
| **NFC** | Unicode Normalization Form C - a Unicode normalization form that uses canonical decomposition followed by canonical composition |
| **USTAR** | Unix Standard TAR - a standardized tar archive format |
| **Manifest** | The `manifest.json` file containing package metadata |
| **Main** | The entry point file for each variant, specified in the manifest |
| **COSE** | CBOR Object Signing and Encryption - reserved for future signature support |

## Domain Model

### Package Structure

An LGX package (`.lgx` file) is a gzip-compressed tar archive with the following structure. Gzip is used because it's the most universally supported compression with stable tooling on every OS, providing the simplest default for "any platform can unpack".

```
package.lgx (tar.gz)
├── manifest.json          # Required - package metadata
├── manifest.cose          # Optional - reserved for signatures
├── variants/              # Required - contains variant directories
│   ├── <variant-1>/       # Variant directory (lowercase name)
│   │   └── ...            # Variant contents
│   └── <variant-2>/
│       └── ...
├── docs/                  # Optional - documentation
│   └── ...
└── licenses/              # Optional - license files
    └── ...
```

**Root Entry Constraints:**
- Only `manifest.json`, `manifest.cose`, `variants/`, `docs/`, and `licenses/` are permitted at root
- Any other root entries cause validation failure
- Files directly under `variants/` are forbidden (only directories allowed)
- This strict structure keeps packages easy to validate and reduces ambiguity

### Manifest Schema

The manifest is a UTF-8 encoded JSON file with the following required fields:

```json
{
  "manifestVersion": "0.1.0",
  "name": "package-name",
  "version": "1.2.3",
  "description": "Package description",
  "author": "Author Name",
  "type": "library",
  "category": "crypto",
  "icon": "icon.png",
  "dependencies": ["dep1", "dep2"],
  "main": {
    "linux-amd64": "path/to/main.so",
    "darwin-arm64": "path/to/main.dylib"
  }
}
```

**Field Constraints:**

| Field | Type | Constraints | Purpose |
|-------|------|-------------|---------|
| `manifestVersion` | string | Semver format; tooling rejects unsupported major versions | Version compatibility |
| `name` | string | Canonical lowercase; auto-normalized by tooling | Package identity |
| `version` | string | Package version (semver recommended) | Package identity |
| `description` | string | Human-readable description | Human metadata |
| `author` | string | Author/maintainer name | Human metadata |
| `type` | string | Package type classification | Classification |
| `category` | string | Package category | Classification |
| `icon` | string | Relative path to icon file bundled in the package | Display/branding |
| `dependencies` | array | List of dependency strings | Runtime needs |
| `main` | object | Map of variant name → relative path to entry point (e.g ) `"linux-amd64": "path/to/main.so"` means `linux-amd64/path/to/main.so` | Entry point resolution |

All fields are required to ensure consistent metadata for hosts/registries and applications.

**Main Entry Constraints:**
- Keys must be lowercase (auto-normalized)
- Values must be valid relative paths (no absolute paths, no `..` segments)
- Values must be NFC-normalized
- Each path must resolve to an existing regular file within the variant directory

### Variant Structure

- Variant names are user-defined strings, stored in lowercase (case-insensitive behavior avoids Windows/macOS filesystem quirks and human typos; canonical lowercase makes matching deterministic)
- Variant directories contain platform-specific files
- The directory structure within a variant is preserved from source

**Completeness Constraint:**
- Every variant directory must have a corresponding `main` entry
- Every `main` entry must have a corresponding variant directory
- No extras on either side (bidirectional completeness)
- This ensures installers don't have to guess entrypoints: every variant is loadable, and the manifest can't reference missing content

## Features & Requirements

### Determinism Requirements

LGX packages must be deterministic - identical inputs must produce byte-identical outputs.

**Tar Determinism:**
- Entries sorted lexicographically by NFC-normalized path bytes
- Fixed metadata: `uid=0`, `gid=0`, `uname=""`, `gname=""` (tar headers include uid/gid/mtime/mode; normalizing them prevents host-specific differences from changing checksums)
- Fixed timestamps: `mtime=0`
- Fixed permissions: directories `0755`, files `0644`
- USTAR format

**Gzip Determinism:**
- Header mtime = 0
- No original filename in header
- Fixed OS byte (0xFF = unknown)
- Gzip headers can embed timestamps/filenames/OS markers; zeroing them prevents two builds from differing despite identical content

### Path Safety Rules

All archive paths must satisfy:
- Not absolute (no leading `/`)
- No `..` segments after normalization
- Not empty
- No backslash characters (`\`)
- Unicode NFC-normalized

**Forbidden File Types:**
- Symlinks
- Hardlinks
- Device nodes
- FIFOs

These are forbidden for portability and security: links can escape variant roots or behave differently on extract; special files are unsafe/meaningless for plugins.

### Package Creation Workflow

```
lgx create <name>
```

1. Normalize `name` to lowercase
2. Check if `<name>.lgx` already exists; if so, exit with error
3. Create skeleton manifest with default values:
   - `name`: normalized lowercase name
   - `version`: `"0.0.1"`
   - `description`: `""`
   - `author`: `""`
   - `type`: `""`
   - `category`: `""`
   - `icon`: `""`
   - `dependencies`: `[]`
   - `main`: `{}`
4. Create empty `variants/` directory
5. Write deterministic tar.gz to `<name>.lgx`

### Variant Addition Workflow

```
lgx add <pkg.lgx> --variant <v> --files <path> [--main <relpath>] [-y]
```

1. Load existing package
2. Verify package file exists; if not, exit with error
3. Verify files path exists; if not, exit with error
4. Normalize variant name to lowercase
5. Determine effective main path:
   - If `--files` is a directory: `--main` is required
   - If `--files` is a single file: use basename if `--main` not provided
6. Check if confirmation is needed (unless `-y`):
   - If variant exists and will be replaced
   - If `main[variant]` would change (even if variant doesn't exist yet)
   - If both variant exists and `main` would change
7. **Replace** entire variant directory (no merging - a variant is treated as an atomic build output; merge risks stale leftovers and hard-to-debug installs)
8. Copy files/directory to `variants/<variant>/`
   - Single file: `variants/<variant>/<filename>`
   - Directory: `variants/<variant>/...` (contents placed directly)
9. Update `main[variant]` entry with effective main path
10. Validate and save package

**Confirmation Required When:**
- Replacing existing variant
- Changing existing `main` entry (even if variant is new)
- Both variant replacement and `main` change

**Option Aliases:**
- `-v` for `--variant`
- `-f` for `--files`
- `-m` for `--main`
- `-y` for `--yes`

### Variant Removal Workflow

```
lgx remove <pkg.lgx> --variant <v> [-y]
lgx remove <pkg.lgx> -v <v> [-y]
```

1. Load existing package
2. Verify package file exists; if not, exit with error
3. Verify variant exists; if not, exit with error
4. Prompt for confirmation (unless `-y`)
5. Remove variant directory entries
6. Remove `main[variant]` entry
7. Save package

**Option Aliases:**
- `-v` for `--variant`
- `-y` for `--yes`

### Variant Extraction Workflow

```
lgx extract <pkg.lgx> [--variant <v>] [--output <dir>]
```

1. Verify package file exists; if not, exit with error
2. Load existing package
3. If `--variant` specified:
   - Normalize variant name to lowercase
   - Verify variant exists; if not, exit with error
   - Extract variant contents to `<output>/<variant>/`
4. If `--variant` not specified:
   - Extract all variants to `<output>/<variant>/` for each variant
5. Create directories as needed
6. Write files preserving internal directory structure

**Output Structure:**
- Each variant is extracted to `<output>/<variant-name>/`
- The internal variant structure (from `variants/<variant>/`) is preserved
- For example, `variants/linux-amd64/lib.so` extracts to `<output>/linux-amd64/lib.so`

**Option Aliases:**
- `-v` for `--variant`
- `-o` for `--output`

### Verification Workflow

```
lgx verify <pkg.lgx>
```

1. Verify package file exists; if not, exit with error
2. Load and validate package

**Validation Checks:**
1. Archive is valid tar.gz
2. Root layout restrictions enforced
3. All manifest required fields present
4. Manifest version is supported (major version check)
5. All paths are NFC-normalized
6. Completeness constraint satisfied (variants ↔ main)
7. Each `main` entry points to existing regular file
8. No forbidden file types present

**Output:**
- Errors: Validation failures that prevent package from being valid
- Warnings: Non-fatal issues that should be noted
- Exit code: `0` if valid, `1` if validation failed

## CLI Interface

### Global Options

All commands support the following global options:

- `--help, -h`: Show help information (global or command-specific)
- `--version, -V`: Show version information

### Exit Codes

- `0`: Success
- `1`: Error (validation failure, file not found, etc.)

### Error Handling

Commands perform validation before operations:
- File existence checks for package files and source paths
- Required option validation with clear error messages
- Confirmation prompts for destructive operations (unless `-y` flag is used)

## Future Work

The following commands are implemented as stubs in v0.1 and will be fully implemented in future versions:

### Sign Command

```
lgx sign <pkg.lgx>
```

**Status:** Planned. Not implemented in v0.1. Always exits with error code 1.

**Planned Behavior:**
- Sign the package manifest using COSE (CBOR Object Signing and Encryption)
- Write signature to `manifest.cose` in the package archive
- Support for key management and signature verification

### Publish Command

```
lgx publish <pkg.lgx>
```

**Status:** TBC. No-op in v0.1. Always exits with success code 0.

**Planned Behavior:**
- Publish package to a registry
- Upload package file and metadata
- Registry authentication and authorization
- Version conflict detection and resolution
