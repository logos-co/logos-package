# Signing and Trusting an lgx Package

[`lgx`](https://github.com/logos-co/logos-package) packages can be **signed** so
consumers can verify who produced them. Signing uses an **Ed25519** keypair whose public
half is published as a [`did:jwk:`](https://github.com/quartzjer/did-jwk/blob/main/spec.md)
decentralized identifier; `lgx sign` writes a `manifest.sig` next to the manifest, and
`lgx verify` checks it. A **keyring** of trusted DIDs lets `verify` go one step further
and report whether the signer is one you trust.

This doc-test builds **this** `lgx` commit and runs the full signing-and-trust lifecycle:

1. Build `lgx` and create a small package to sign.
2. `keygen` an Ed25519 keypair (printing its `did:jwk:` DID).
3. `sign` the package and inspect the signature with `manifest` and `signature`.
4. `verify` it — first as an *untrusted* signer, then after adding the DID to a keyring.

Every key directory is passed explicitly (`--output-dir`, `--keys-dir`, `--keyring-dir`)
so the whole flow is self-contained and never touches your real `~/.config/logos`.

**What you'll build:** A signed `greeter.lgx`, an Ed25519 signing key, and a trusted-key keyring that makes `lgx verify` recognise the signer.

**What you'll learn:**

- How to generate an Ed25519 signing keypair and its `did:jwk:` DID with `lgx keygen`
- How to sign a package and read back its signature with `lgx sign` / `lgx signature`
- How `lgx verify` reports signature validity and distinguishes trusted from untrusted signers
- How to manage trusted DIDs with `lgx keyring add` / `list`

## Prerequisites

- **Nix** with flakes enabled. Install from [nixos.org](https://nixos.org/download.html), then enable flakes:

```bash
mkdir -p ~/.config/nix
echo 'experimental-features = nix-command flakes' >> ~/.config/nix/nix.conf
```

Verify: `nix flake --help >/dev/null 2>&1 && echo "Flakes enabled"`

- A Linux or macOS machine.

---

## Step 1: Build lgx and create a package to sign

Build the `lgx` CLI from this commit, then create a one-variant package — the thing
we will sign. (The [packaging doc-test](lgx-cli.md) covers `create` / `add` in
detail; here they are just setup.)

> The `` placeholder pins the build to the commit under test (this
> checkout's `HEAD` locally, the PR/push commit in CI). Against a local checkout use
> `nix build '.#lgx' -o lgx`.

### 1.1 Build lgx

```bash
# From inside the clone this is simply: nix build '.#lgx' -o lgx
nix build 'github:logos-co/logos-package/41cae9e4546899eb11912372588a6707ca43efe4#lgx' -o lgx
```

### 1.2 Create and populate greeter.lgx

Detect this machine's variant (and shared-library extension), create the
package, and add a stub payload so there is content to sign.

```bash
lgx create greeter
echo 'stub library' > libgreeter.so          # .dylib on macOS
lgx add greeter.lgx --variant linux-x86_64 --files libgreeter.so
```

---

## Step 2: Generate a signing key

`lgx keygen` creates an Ed25519 keypair and writes three files — the secret key
(`.jwk`), the public key (`.pub`), and the DID string (`.did`). It prints the
`did:jwk:` DID to stdout. We point `--output-dir` at a local `./keys` directory and
stash the DID in a file so later steps can refer to it.

### 2.1 Run keygen into ./keys

`keygen` prints the DID to stdout and also writes it to `./keys/demo-key.did`. We
copy that file to `./did.txt` so later steps can pass the exact DID without
re-deriving it.

```bash
lgx keygen --name demo-key --output-dir ./keys
# → did:jwk:eyJjcnYiOiJFZDI1NTE5Ii...   (also saved to ./keys/demo-key.did)
```

The keypair now lives under `./keys/` as `demo-key.{jwk,pub,did}`. The `.jwk`
file is the **secret** key — keep it safe; only the DID is meant to be shared.

---

## Step 3: Sign the package

`lgx sign` validates the package, then writes a `manifest.sig` containing the
signer's DID, an Ed25519 signature over the manifest bytes, and optional
self-asserted signer metadata (`--name` / `--url`). We pass `--keys-dir ./keys` so it
finds the key we just generated.

### 3.1 Sign greeter.lgx

```bash
lgx sign greeter.lgx --key demo-key --keys-dir ./keys \
  --name "Logos Demo" --url "https://logos.co"
```

### 3.2 See the signature in the manifest

`lgx manifest` now reports the package as signed, along with the signer's DID and
self-asserted name/URL.

```bash
lgx manifest greeter.lgx
```

### 3.3 Read the raw signature

`lgx signature` prints the raw `manifest.sig` JSON — byte-identical to the file
inside the `.lgx`. It carries the algorithm, the signer DID, the base64
signature, and the signer metadata. Unsigned packages print nothing and exit 0,
so tooling tells "unsigned" from "error" by the exit status, not the output.

```bash
lgx signature greeter.lgx
```

---

## Step 4: Verify an untrusted signer

`lgx verify` always checks structure and (for signed packages) the signature itself.
Trust is separate: with no matching entry in the keyring, the signature is reported
**valid** but the signer is flagged as **not** trusted. We point `--keyring-dir` at an
empty `./trusted` directory to make that explicit.

### 4.1 Verify against an empty keyring

```bash
mkdir -p trusted
lgx verify greeter.lgx --keyring-dir ./trusted
```

---

## Step 5: Trust the signer

`lgx keyring add` records a DID as trusted under a short name. We add the DID we
stashed earlier (still in `did.txt`) to the `./trusted` keyring, then list it.

### 5.1 Add the DID to the keyring

```bash
lgx keyring add logos-demo "$DID" \
  --display-name "Logos Demo" --url "https://logos.co" --dir ./trusted
```

### 5.2 List trusted keys

```bash
lgx keyring list --dir ./trusted
```

---

## Step 6: Verify a trusted signer

Re-run `verify` against the populated keyring. Now that the signer's DID is trusted,
`verify` adds a final line naming the trusted key — the same valid signature, now
recognised.

### 6.1 Verify against the populated keyring

```bash
lgx verify greeter.lgx --keyring-dir ./trusted
```

---

## Recap

You built `lgx` from this commit and ran the full signing-and-trust lifecycle —
generating a key, signing a package, inspecting the signature, and verifying it both
as an untrusted and a trusted signer:

| Command | What it does |
|---|---|
| `keygen --name <n> --output-dir <dir>` | Generate an Ed25519 keypair; print its `did:jwk:` DID |
| `sign <pkg> --key <n> --keys-dir <dir> [--name …] [--url …]` | Write `manifest.sig` with the signer's DID |
| `signature <pkg>` | Print the raw `manifest.sig` JSON (unsigned → empty + exit 0) |
| `verify <pkg> [--keyring-dir <dir>]` | Validate structure + signature; report trust |
| `keyring add <name> <did> [--dir <dir>]` | Trust a DID under a short name |
| `keyring list [--dir <dir>]` | List trusted DIDs |

Creating, adding variants, extracting, and merging packages are covered in the
companion [packaging doc-test](lgx-cli.md).
