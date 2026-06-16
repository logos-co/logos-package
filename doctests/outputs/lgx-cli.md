# Packaging a Module with lgx

[`lgx`](https://github.com/logos-co/logos-package) is the Logos **package tool**.
An `.lgx` file is just a **gzipped tar archive** with a `manifest.json` at its root
and one or more platform payloads under `variants/<platform>/`. `lgx` is the CLI that
builds, inspects, extracts, merges, and (in the companion
[signing doc-test](lgx-signing.md)) signs those archives — a self-contained C++ tool
with no Qt and no module runtime behind it.

This doc-test builds **this** `lgx` commit and drives the full packaging lifecycle:

1. Build the `lgx` CLI from this repository's flake.
2. `create` an empty package and read its `manifest`.
3. `add` this machine's platform variant and `verify` the result.
4. `extract` a variant back out, and inspect the raw archive with `tar`.
5. `merge` two single-variant packages into one multi-variant `.lgx`.

Every command is the real binary built from the commit under test, so a green run is
evidence that this change keeps the packaging workflow working end-to-end.

**What you'll build:** A `greeter.lgx` package built, inspected, and extracted with `lgx`, plus a multi-variant `widget.lgx` assembled with `lgx merge`.

**What you'll learn:**

- What an `.lgx` package is on disk (a gzipped tar of a manifest + a `variants/` tree)
- How to create a package and read its manifest with `lgx create` / `lgx manifest`
- How to add a platform variant, verify structural validity, and extract it back out
- How to merge per-platform single-variant packages into one multi-variant `.lgx`

## Prerequisites

- **Nix** with flakes enabled. Install from [nixos.org](https://nixos.org/download.html), then enable flakes:

```bash
mkdir -p ~/.config/nix
echo 'experimental-features = nix-command flakes' >> ~/.config/nix/nix.conf
```

Verify: `nix flake --help >/dev/null 2>&1 && echo "Flakes enabled"`

- A Linux or macOS machine.

---

## Step 1: Build the lgx CLI

`lgx` ships as a C++ CLI. Build it straight from the flake's `#lgx` output and
link the result as `./lgx`, so the binary lands at `./lgx/bin/lgx`.

> The `` in the URL is what pins the build to a specific commit: the
> doc-test runner expands it to a concrete ref. Locally that is this checkout's
> `HEAD` (see `run.sh`); in CI it is the commit being tested. With no pin it falls
> back to the latest `master`. Developing against a local checkout? Replace the
> GitHub reference with `.`, e.g. `nix build '.#lgx' -o lgx`.

### 1.1 Build lgx

```bash
# From inside the clone this is simply: nix build '.#lgx' -o lgx
nix build 'github:logos-co/logos-package/41cae9e4546899eb11912372588a6707ca43efe4#lgx' -o lgx
```

The `-o lgx` flag names the result symlink, so the executable is at `./lgx/bin/lgx`.

### 1.2 Confirm it runs

```bash
./lgx/bin/lgx --version
```

---

## Step 2: Detect the platform variant

An `.lgx` package carries one or more **platform variants** — `linux-x86_64`,
`darwin-arm64`, and so on — each holding the payload for that platform. Detect this
machine's variant once (and the matching shared-library extension, `.so` on Linux /
`.dylib` on macOS) and stash both in files so the packaging steps below can reuse
them.

### 2.1 Write the variant and library extension to ./variant and ./ext

```bash
case "$(uname -s) $(uname -m)" in
  "Linux x86_64")   echo linux-x86_64  > variant; echo so    > ext ;;
  "Linux aarch64")  echo linux-arm64   > variant; echo so    > ext ;;
  "Darwin arm64")   echo darwin-arm64  > variant; echo dylib > ext ;;
  "Darwin x86_64")  echo darwin-x86_64 > variant; echo dylib > ext ;;
  *) echo "unsupported platform: $(uname -sm)" >&2; exit 1 ;;
esac
echo "variant: $(cat variant)  ext: $(cat ext)"

```

---

## Step 3: Create a package

`lgx create <name>` writes an empty skeleton package — a `manifest.json` with
sensible defaults plus an empty `variants/` directory — to `<name>.lgx`.

### 3.1 Create greeter.lgx

```bash
./lgx/bin/lgx create greeter
```

### 3.2 Read the fresh manifest

`lgx manifest` prints the embedded `manifest.json` in human-readable form. A
freshly created package starts at version `0.0.1`, manifest schema `0.3.0`, with
no type, no variants, and no signature yet.

```bash
lgx manifest greeter.lgx
```

---

## Step 4: Add a platform variant

`lgx add` drops a payload into the package under `variants/<variant>/` and records
the entry point in the manifest's `main` map. We add a stub shared library for this
machine's variant. (In a real build the payload is the module's compiled `.so` /
`.dylib`; `lgx` does not load it — it just packages it.)

### 4.1 Add this machine's variant

```bash
# On Linux x86_64 this expands to:
echo 'stub library' > libgreeter.so
lgx add greeter.lgx --variant linux-x86_64 --files libgreeter.so
```

Confirm the archive layout — a `manifest.json` beside the `variants/` tree:

```bash
tar -tzf greeter.lgx
```

```
manifest.json
variants/
variants/<platform>/
variants/<platform>/libgreeter.so   # .dylib on macOS
```

### 4.2 See the variant in the manifest

The manifest now lists the variant and points its `main` entry at the file we
added. A content hash (`Root hash`) is recomputed on every save.

```bash
lgx manifest greeter.lgx
```

---

## Step 5: Verify the package

`lgx verify` validates a package against the LGX spec — it recomputes the content
hashes (the Merkle tree) and checks them against the manifest, then reports the
signature state. Our package is unsigned, so verification passes and notes it is
unsigned. The exit code is `0` on success, so `verify` is safe to gate CI on.

### 5.1 Verify greeter.lgx

```bash
lgx verify greeter.lgx
```

---

## Step 6: Extract a variant

`lgx extract` unpacks a variant's payload back onto disk — the inverse of `add`.
Files land under `<output>/<variant>/`.

### 6.1 Extract this machine's variant

```bash
lgx extract greeter.lgx --variant linux-x86_64 --output ./extracted
```

The payload is now on disk under `./extracted/<variant>/`:

```bash
ls ./extracted/<variant>
```

---

## Step 7: Merge per-platform packages

Real releases ship one `.lgx` covering every platform. A common pattern is to build
a **single-variant** package on each platform's CI runner, then `lgx merge` them
into one multi-variant archive. The inputs must share identical metadata (only the
variant-specific `main` entries differ). We simulate it here by hand-building a
Linux and a macOS package for the same module and merging them.

### 7.1 Build two single-variant packages

```bash
# A Linux-only package...
./lgx/bin/lgx create widget && mv widget.lgx widget-linux.lgx
echo 'linux stub' > libwidget.so
./lgx/bin/lgx add widget-linux.lgx --variant linux-x86_64 --files libwidget.so

# ...and a macOS-only package for the same module.
./lgx/bin/lgx create widget && mv widget.lgx widget-darwin.lgx
echo 'darwin stub' > libwidget.dylib
./lgx/bin/lgx add widget-darwin.lgx --variant darwin-arm64 --files libwidget.dylib

```

### 7.2 Merge them into one .lgx

`lgx merge` writes a single package carrying both variants. The variants are
listed alphabetically in the summary.

```bash
lgx merge widget-linux.lgx widget-darwin.lgx -o widget.lgx
```

### 7.3 Confirm both variants are present

The merged manifest carries both platforms, and the package verifies:

```bash
lgx manifest widget.lgx
```

```bash
lgx verify widget.lgx
```

---

## Recap

You built `lgx` from this commit and ran a full packaging lifecycle — creating a
package, adding a platform variant, verifying it, extracting it, and merging
per-platform packages into one multi-variant `.lgx`:

| Command | What it does |
|---|---|
| `create <name>` | Write an empty skeleton `<name>.lgx` |
| `add <pkg> -v <variant> -f <path>` | Add a payload for a variant (records `main`) |
| `manifest <pkg> [--json]` | Print the embedded `manifest.json` (`--json` for raw bytes) |
| `verify <pkg>` | Validate structure + content hashes, report signature state |
| `extract <pkg> -v <variant> -o <dir>` | Unpack a variant's payload to disk |
| `merge <pkg…> -o <out>` | Combine single-variant packages into one multi-variant `.lgx` |

Signing, signature inspection, and trusted-key management are covered in the
companion [signing doc-test](lgx-signing.md).
