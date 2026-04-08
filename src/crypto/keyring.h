#pragma once

#include "signing.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace lgx {
namespace crypto {

/**
 * A trusted key entry.
 */
struct TrustedKey {
    std::string name;         // local keyring name (e.g., "logos-official")
    std::string did;          // did:jwk:... string
    PublicKey publicKey;       // extracted from DID for crypto operations
    std::string displayName;  // cached signer.name from manifest.sig
    std::string url;          // cached signer.url
    std::string addedAt;      // ISO 8601 timestamp
};

/**
 * Directory-based keyring using JSON files.
 *
 * Trusted keys are stored as individual .json files in a directory:
 *   ~/.config/logos/trusted-keys/<name>.json
 * Each file contains:
 *   {"did": "did:jwk:...", "name": "Display Name", "url": "https://...", "addedAt": "..."}
 *
 * Secret keys for signing are stored separately as JWK files:
 *   ~/.config/logos/keys/<name>.jwk
 */
class Keyring {
public:
    /**
     * Construct a keyring rooted at the given directory.
     * Creates the directory with 0700 permissions if it doesn't exist.
     */
    explicit Keyring(const std::filesystem::path& dir);

    /**
     * Get the default keyring directory.
     * Uses $XDG_CONFIG_HOME/logos/trusted-keys or ~/.config/logos/trusted-keys.
     */
    static std::filesystem::path defaultDirectory();

    /**
     * Get the default keys directory (for secret/public keypairs).
     * Uses $XDG_CONFIG_HOME/logos/keys or ~/.config/logos/keys.
     */
    static std::filesystem::path defaultKeysDirectory();

    /**
     * Add a trusted key by DID.
     * Writes <name>.json with DID and optional metadata.
     *
     * @param name Local name for the key
     * @param did DID string (did:jwk:...)
     * @param displayName Optional display name
     * @param url Optional URL
     * @return true on success
     */
    bool addKey(const std::string& name, const std::string& did,
                const std::string& displayName = "",
                const std::string& url = "");

    /**
     * Remove a trusted key by name.
     * @return true if key existed and was removed
     */
    bool removeKey(const std::string& name);

    /**
     * Find a trusted key by its DID string.
     * @return Key entry if found, nullopt otherwise
     */
    std::optional<TrustedKey> findByDid(const std::string& did) const;

    /**
     * Find a trusted key by its public key bytes.
     * Derives DID from public key and calls findByDid.
     * @return Key entry if found, nullopt otherwise
     */
    std::optional<TrustedKey> findByPublicKey(const PublicKey& pk) const;

    /**
     * Find a trusted key by name.
     * @return Key entry if found, nullopt otherwise
     */
    std::optional<TrustedKey> findByName(const std::string& name) const;

    /**
     * List all trusted keys.
     */
    std::vector<TrustedKey> listKeys() const;

    /**
     * Save a generated keypair.
     * Writes secret key to keysDir/<name>.jwk (JWK format, 0600) and
     * public key to keysDir/<name>.pub (SSH format) and
     * DID to keysDir/<name>.did (plain text).
     *
     * @param keysDir Directory for keypair files
     * @param name Name for the keypair
     * @param kp Keypair to save
     * @return true on success
     */
    static bool saveKeypair(
        const std::filesystem::path& keysDir,
        const std::string& name,
        const KeyPair& kp);

    /**
     * Load a secret key from JWK file.
     *
     * @param keysDir Directory containing key files
     * @param name Key name
     * @return Secret key, or nullopt if not found / invalid
     */
    static std::optional<SecretKey> loadSecretKey(
        const std::filesystem::path& keysDir,
        const std::string& name);

    /**
     * Get last error message.
     */
    static std::string getLastError();

private:
    std::filesystem::path dir_;

    static thread_local std::string lastError_;

    /**
     * Validate a key name, rejecting path separators and empty names.
     */
    static bool validateKeyName(const std::string& name);

    /**
     * Format a public key in SSH format.
     */
    static std::string formatSSHPubKey(const PublicKey& pk, const std::string& name);

    /**
     * Ensure directory exists with proper permissions (0700).
     */
    static bool ensureDirectory(const std::filesystem::path& dir);

    /**
     * Get current time as ISO 8601 string.
     */
    static std::string currentTimestamp();
};

} // namespace crypto
} // namespace lgx
