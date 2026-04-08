#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace lgx {

struct TarEntry;

namespace crypto {

// Ed25519 key sizes
static constexpr size_t PUBLIC_KEY_SIZE = 32;
static constexpr size_t SECRET_KEY_SIZE = 64;
static constexpr size_t SIGNATURE_SIZE = 64;
static constexpr size_t SHA256_SIZE = 32;

using PublicKey = std::array<uint8_t, PUBLIC_KEY_SIZE>;
using SecretKey = std::array<uint8_t, SECRET_KEY_SIZE>;
using Signature = std::array<uint8_t, SIGNATURE_SIZE>;

struct KeyPair {
    PublicKey publicKey;
    SecretKey secretKey;
};

/**
 * Initialize libsodium. Must be called before any other crypto functions.
 * Safe to call multiple times.
 * @return true on success
 */
bool init();

/**
 * Generate a new Ed25519 keypair.
 */
KeyPair generateKeypair();

/**
 * Create a detached Ed25519 signature.
 *
 * @param message Data to sign
 * @param sk Secret key
 * @return 64-byte signature
 */
Signature sign(const std::vector<uint8_t>& message, const SecretKey& sk);

/**
 * Verify a detached Ed25519 signature.
 *
 * @param message Original data
 * @param pk Public key
 * @param sig Signature to verify
 * @return true if valid
 */
bool verify(const std::vector<uint8_t>& message, const PublicKey& pk, const Signature& sig);

/**
 * Compute SHA-256 hash and return as hex string (64 chars).
 */
std::string sha256Hex(const std::vector<uint8_t>& data);

/**
 * Compute SHA-256 hash and return as hex string from raw pointer.
 */
std::string sha256Hex(const uint8_t* data, size_t len);

/**
 * Base64 encode raw bytes.
 */
std::string base64Encode(const uint8_t* data, size_t len);

/**
 * Base64 decode a string.
 * @return decoded bytes, or nullopt on error
 */
std::optional<std::vector<uint8_t>> base64Decode(const std::string& encoded);

/**
 * Compute Merkle-style hash for all files under a given archive path prefix.
 *
 * Collects all non-directory entries under prefix/, sorts by relative path,
 * hashes each file, concatenates (path + '\0' + hex_hash + '\n'), and returns
 * the SHA-256 of the concatenation.
 *
 * @param entries All tar entries in the archive
 * @param prefix Directory prefix (e.g., "variants/darwin-arm64" or "docs")
 * @return Hex-encoded SHA-256 hash, or empty string if no files found
 */
std::string computeLeafDirectoryHash(
    const std::vector<TarEntry>& entries,
    const std::string& prefix);

/**
 * Compute parent directory hash from sorted child directory hashes.
 *
 * Concatenates (dirname + '\0' + child_hash + '\n') for each child sorted
 * by name, returns SHA-256 of the concatenation.
 *
 * @param childHashes Map of child directory name -> hash
 * @return Hex-encoded SHA-256 hash
 */
std::string computeParentDirectoryHash(
    const std::map<std::string, std::string>& childHashes);

/**
 * Build a full Merkle tree over all archive content.
 *
 * Excludes manifest.json and manifest.sig. Returns a map with entries at
 * every level of the tree:
 *   "root" - hash over all top-level entries
 *   "variants" - hash over all variant child hashes
 *   "variants/darwin-arm64" - leaf hash of that variant's files
 *   "docs" - leaf hash of docs/ files
 *   "licenses" - leaf hash of licenses/ files
 *
 * @param entries All tar entries in the archive
 * @return Map of path -> hex SHA-256 hash
 */
std::map<std::string, std::string> computeMerkleTree(
    const std::vector<TarEntry>& entries);

/**
 * Extract the public key from a secret key.
 */
PublicKey extractPublicKey(const SecretKey& sk);

/**
 * Base64url encode raw bytes (RFC 4648 §5, no padding).
 */
std::string base64UrlEncode(const uint8_t* data, size_t len);

/**
 * Base64url encode a string (RFC 4648 §5, no padding).
 */
std::string base64UrlEncode(const std::string& data);

/**
 * Base64url decode a string (RFC 4648 §5, no padding).
 * @return decoded bytes, or nullopt on error
 */
std::optional<std::vector<uint8_t>> base64UrlDecode(const std::string& encoded);

/**
 * Convert an Ed25519 public key to a did:jwk DID string.
 *
 * Format: did:jwk:<base64url({"crv":"Ed25519","kty":"OKP","x":"<base64url-pubkey>"})>
 * JWK keys are sorted alphabetically for determinism.
 */
std::string publicKeyToDid(const PublicKey& pk);

/**
 * Extract an Ed25519 public key from a did:jwk DID string.
 * @return Public key, or nullopt if the DID is invalid
 */
std::optional<PublicKey> didToPublicKey(const std::string& did);

} // namespace crypto
} // namespace lgx
