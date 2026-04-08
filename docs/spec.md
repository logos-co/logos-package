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
| **Ed25519** | An elliptic-curve digital signature algorithm used for package signing |
| **Merkle Tree** | A hierarchical hash structure used to verify package content integrity |

## Domain Model

### Package Structure

An LGX package (`.lgx` file) is a gzip-compressed tar archive with the following structure. Gzip is used because it's the most universally supported compression with stable tooling on every OS, providing the simplest default for "any platform can unpack".

```
package.lgx (tar.gz)
├── manifest.json          # Required - package metadata
├── manifest.sig           # Optional - Ed25519 signature with DID identity
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

### Package Merge Workflow

```
lgx merge <pkg1.lgx> <pkg2.lgx> ... [-o <output.lgx>] [--skip-duplicates] [-y]
```

1. Verify all input package files exist; if any missing, exit with error
2. Load all input packages
3. Compare manifests across all inputs, ignoring the `main` field (which is variant-specific):
   - All non-variant fields must be identical (`manifestVersion`, `name`, `version`, `description`, `author`, `type`, `category`, `icon`, `dependencies`)
   - If any mismatch is found, report all mismatching fields and exit with error
4. Check for duplicate variants across all input packages:
   - By default, exit with error if any variant appears in more than one input
   - With `--skip-duplicates`: warn and keep only the first occurrence
5. Determine output path:
   - Use `--output` if provided
   - Otherwise default to `<name>.lgx` (from the manifest name)
6. If output file exists, prompt for confirmation (unless `-y`)
7. Create a fresh skeleton package with the shared metadata
8. For each input package, for each variant:
   - Extract variant files to a temporary directory
   - Add variant to the output package using the standard `addVariant` flow
   - Preserve the `main` entry from the source package
9. Save merged package
10. Clean up temporary files

**Manifest Comparison:**
The merge command compares all manifest fields except `main`, which is expected to differ across platform-specific builds. This ensures the merged package represents the same logical module across all variants.

**Option Aliases:**
- `-o` for `--output`
- `-y` for `--yes`

### Verification Workflow

```
lgx verify <pkg.lgx> [--keyring-dir <dir>]
```

Options:
- `--keyring-dir <dir>` — Keyring directory for trust lookup (default: `~/.config/logos/trusted-keys/`)

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
9. Content hashes present and valid (Merkle tree root hash matches recomputed value)

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

## Package Signing

### Overview

Packages can be cryptographically signed using Ed25519 via libsodium. Signing creates a `manifest.sig` file containing the signer's DID (`did:jwk:...`), the Ed25519 signature over `manifest.json`, and optional signer metadata.

### Content Hashes

Content hashes (Merkle tree) are **always present** in `manifest.json`, regardless of whether the package is signed. They are automatically recomputed whenever package content is modified (adding/removing variants). The `lgx verify` command validates these hashes for all packages.

### Sign Command

```
lgx sign <pkg.lgx> --key <name> [--keys-dir <dir>] [--name "Display Name"] [--url "https://..."]
```

Signs a package by:
1. Validating the package (structure and content hashes)
2. Creating an Ed25519 detached signature over the exact bytes of `manifest.json`
3. Writing `manifest.sig` with the signer's DID, signature, and optional metadata

Options:
- `--key, -k <name>` — Name of the signing key (required)
- `--keys-dir, -d <dir>` — Directory containing key files (default: `~/.config/logos/keys/`)
- `--name <display-name>` — Signer display name (self-asserted metadata)
- `--url <url>` — Signer URL (self-asserted metadata)

The secret key is loaded from `<keys-dir>/<name>.jwk`.

### Keygen Command

```
lgx keygen --name <name> [--output-dir <dir>]
```

Generates an Ed25519 signing keypair.

Options:
- `--name, -n <name>` — Name for the keypair (required)
- `--output-dir, -o <dir>` — Directory to write key files (default: `~/.config/logos/keys/`)

Files created:
- `<dir>/<name>.jwk` — Secret key (JWK format, permissions 0600)
- `<dir>/<name>.pub` — Public key (SSH public key format)
- `<dir>/<name>.did` — DID string (plain text `did:jwk:...`)

Prints the `did:jwk:...` DID to stdout.

#### Secret Key Format (JWK)

```json
{
  "crv": "Ed25519",
  "d": "<base64url-encoded 32-byte private seed>",
  "kty": "OKP",
  "x": "<base64url-encoded 32-byte public key>"
}
```

Keys are sorted alphabetically for determinism. The `d` field contains the 32-byte Ed25519 seed (not libsodium's 64-byte expanded key).

### Keyring Command

```
lgx keyring add <name> <did:jwk:...> [--display-name "..."] [--url "..."] [--dir <dir>]
lgx keyring remove <name> [--dir <dir>]
lgx keyring list [--dir <dir>]
```

Options:
- `--dir, -d <dir>` — Keyring directory (default: `~/.config/logos/trusted-keys/`)

Manages trusted keys stored as `.json` files in the keyring directory:

```json
{
  "did": "did:jwk:eyJjcnYi...",
  "name": "Logos Foundation",
  "url": "https://logos.co",
  "addedAt": "2026-04-06T12:00:00Z"
}
```

### DID Identity

Signers are identified by **DID (Decentralized Identifier)** strings using the `did:jwk` method:

```
did:jwk:<base64url({"crv":"Ed25519","kty":"OKP","x":"<base64url-pubkey>"})>
```

Construction:
1. Take 32-byte Ed25519 public key
2. Base64url-encode it (RFC 4648 §5, no padding) → the `x` value
3. Construct minimal JWK: `{"crv":"Ed25519","kty":"OKP","x":"<x>"}` (keys sorted alphabetically)
4. Base64url-encode the JWK JSON string
5. Prepend `did:jwk:`

The DID is deterministic from the public key — the same key always produces the same DID string.

### manifest.sig Format

```json
{
  "algorithm": "ed25519",
  "did": "did:jwk:eyJjcnYiOiJFZDI1NTE5Iiwia3R5IjoiT0tQIiwieCI6IjExcVlBWUt4Q3JmVlNfN1R5V1FIT2c3aGN2UGFwaU1scndJYWFQY0hVUm8ifQ",
  "linkedDids": [],
  "signature": "<base64-encoded 64-byte Ed25519 signature>",
  "signer": {
    "name": "Logos Foundation",
    "url": "https://logos.co"
  },
  "version": 1
}
```

- `did` — signer identity as a `did:jwk:` string. The public key is encoded in the DID itself.
- `signer` — optional, self-asserted metadata (display name, URL). Not covered by the signature. Used for trust prompts.
- `linkedDids` — reserved for future `did:pkh` entries (blockchain-verified identity). Always `[]` for now. The current type is `string[]` as a placeholder; the final schema will evolve to `{did, proofType, proof}[]` when did:pkh is implemented — each entry will carry a cryptographic proof (e.g., CACAO/EIP-4361) binding the blockchain identity to the signer's `did:jwk`. This is not a breaking change since the field is currently always empty.

### Merkle Tree Hashing

The `hashes` field in `manifest.json` is a map of directory paths to SHA-256 hex digests. Hashes are always present and kept up to date whenever content changes.

- **File hashes**: SHA-256 of each file's content
- **Leaf directory hashes** (e.g., `variants/darwin-arm64`): Sort files by relative path, concatenate `(path + '\0' + file_hash + '\n')`, SHA-256 the result
- **Parent directory hashes** (e.g., `variants`): Sort child directory names, concatenate `(dirname + '\0' + child_hash + '\n')`, SHA-256 the result
- **Root hash**: Same algorithm over all top-level entries

### Signature Invalidation

Any operation that modifies package content:
- Clears the existing signature (`manifest.sig`)
- Recomputes content hashes in `manifest.json`
- `save()` without a valid signature does NOT write `manifest.sig`

### Verification

`lgx verify <pkg.lgx>` performs:
1. Structural validation (existing checks)
2. Content hash validation — recomputes Merkle tree and verifies root hash matches (all packages)
3. If `manifest.sig` is present: verify Ed25519 signature, report signer DID and trust status

### Install-Time Verification (lgpm)

`lgpm install` verifies signatures before extracting packages:
- **Default (warn)**: Unsigned packages accepted with a warning; signed packages from unknown signers are accepted with trust info in the response
- `--allow-unsigned`: No signature checking
- `--require-signatures`: Reject unsigned packages and packages signed by untrusted keys

Keyring management (adding/removing trusted keys) is handled separately via `lgx keyring` or the package-manager module's `addTrustedKey`/`removeTrustedKey`/`listTrustedKeys` API.

## Future Work

### did:pkh — Blockchain-Verified Identity

The current DID implementation uses `did:jwk` exclusively — a self-contained, offline identity where the public key is embedded in the DID string. No DID Documents are produced or consumed; the DID string is used directly as a structured public key encoding. This is sufficient because `did:jwk` DID Documents are deterministically derivable from the string itself.

When `did:pkh` support is added, signers will be able to prove they control a blockchain account in addition to their Ed25519 signing key. This provides stronger identity guarantees (the signer is tied to a public, auditable on-chain identity).

#### linkedDids schema evolution

The `linkedDids` field in `manifest.sig` is currently `string[]` (always empty). When did:pkh is implemented, each entry will become an object carrying both the DID and a cryptographic proof:

```json
"linkedDids": [
  {
    "did": "did:pkh:eip155:1:0xab16a96D359eC26a11e2C2b3d8f8B8942d5Bfcdb",
    "proofType": "cacao",
    "proof": "<CACAO/EIP-4361 signature proving the blockchain account authorized this did:jwk>"
  }
]
```

Without a proof, a `linkedDids` entry is just a claim — anyone could list any blockchain address. The proof cryptographically binds the `did:pkh` to the signer's `did:jwk`.

#### Proof mechanisms

| Mechanism | Description | Offline verification? |
|-----------|-------------|----------------------|
| **CACAO (EIP-4361)** | Blockchain account signs a "Sign-In with Ethereum" message authorizing the `did:jwk` | Yes (proof is self-contained) |
| **Verifiable Credential** | A VC issued by the `did:pkh` subject attesting ownership of the `did:jwk` | Yes (proof is self-contained) |
| **On-chain registry** | Smart contract mapping blockchain addresses to `did:jwk` strings | No (requires chain query) |
| **Bidirectional signatures** | Ed25519 key signs `did:pkh` + blockchain key signs `did:jwk` | Yes (both proofs stored) |

CACAO/EIP-4361 is the most practical option: standardized, self-contained, and widely supported.

#### New dependencies

- `secp256k1` library for Ethereum signature verification
- CACAO/EIP-4361 message format parsing
- Optional: blockchain RPC client for on-chain registry approaches
- DID Document resolution for `did:pkh` (extracting verification methods)

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
