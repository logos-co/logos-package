#pragma once

#include <optional>
#include <string>
#include <vector>

namespace lgx {
namespace crypto {

/**
 * Represents the content of manifest.sig in an LGX package.
 * JSON format for human readability and debuggability.
 */
struct ManifestSig {
    int version = 1;
    std::string algorithm = "ed25519";
    std::string did;            // did:jwk:... DID string (replaces publicKey)
    std::string signature;      // base64-encoded 64-byte Ed25519 signature

    // Optional self-asserted signer metadata
    std::string signerName;     // display name (e.g. "Logos Foundation")
    std::string signerUrl;      // URL (e.g. "https://logos.co")

    // Reserved for future did:pkh entries
    std::vector<std::string> linkedDids;

    /**
     * Serialize to deterministic JSON string.
     */
    std::string toJson() const;

    /**
     * Parse from JSON string.
     * @return ManifestSig if valid, nullopt on error
     */
    static std::optional<ManifestSig> fromJson(const std::string& json);

    /**
     * Get last parse error.
     */
    static std::string getLastError();

private:
    static thread_local std::string lastError_;
};

} // namespace crypto
} // namespace lgx
