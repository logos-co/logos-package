#include "signing.h"
#include "../core/tar_writer.h"

#include <sodium.h>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <set>

namespace lgx {
namespace crypto {

bool init() {
    static bool initialized = false;
    static bool success = false;
    if (!initialized) {
        success = (sodium_init() >= 0);
        initialized = true;
    }
    return success;
}

KeyPair generateKeypair() {
    init();
    KeyPair kp;
    crypto_sign_ed25519_keypair(kp.publicKey.data(), kp.secretKey.data());
    return kp;
}

Signature sign(const std::vector<uint8_t>& message, const SecretKey& sk) {
    init();
    Signature sig;
    unsigned long long sigLen;
    crypto_sign_ed25519_detached(
        sig.data(), &sigLen,
        message.data(), message.size(),
        sk.data());
    return sig;
}

bool verify(const std::vector<uint8_t>& message, const PublicKey& pk, const Signature& sig) {
    init();
    return crypto_sign_ed25519_verify_detached(
        sig.data(),
        message.data(), message.size(),
        pk.data()) == 0;
}

std::string sha256Hex(const std::vector<uint8_t>& data) {
    return sha256Hex(data.data(), data.size());
}

std::string sha256Hex(const uint8_t* data, size_t len) {
    init();
    unsigned char hash[crypto_hash_sha256_BYTES];
    crypto_hash_sha256(hash, data, len);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < crypto_hash_sha256_BYTES; ++i) {
        oss << std::setw(2) << static_cast<unsigned>(hash[i]);
    }
    return oss.str();
}

std::string base64Encode(const uint8_t* data, size_t len) {
    init();
    size_t b64Len = sodium_base64_encoded_len(len, sodium_base64_VARIANT_ORIGINAL);
    std::string result(b64Len, '\0');
    sodium_bin2base64(&result[0], b64Len, data, len, sodium_base64_VARIANT_ORIGINAL);
    // Remove trailing null
    while (!result.empty() && result.back() == '\0') {
        result.pop_back();
    }
    return result;
}

std::optional<std::vector<uint8_t>> base64Decode(const std::string& encoded) {
    init();
    std::vector<uint8_t> decoded(encoded.size());
    size_t decodedLen = 0;
    if (sodium_base642bin(decoded.data(), decoded.size(),
                          encoded.c_str(), encoded.size(),
                          nullptr, &decodedLen, nullptr,
                          sodium_base64_VARIANT_ORIGINAL) != 0) {
        return std::nullopt;
    }
    decoded.resize(decodedLen);
    return decoded;
}

PublicKey extractPublicKey(const SecretKey& sk) {
    init();
    PublicKey pk;
    crypto_sign_ed25519_sk_to_pk(pk.data(), sk.data());
    return pk;
}

std::string computeLeafDirectoryHash(
    const std::vector<TarEntry>& entries,
    const std::string& prefix)
{
    // Collect non-directory entries under prefix/
    std::string prefixSlash = prefix + "/";
    std::vector<std::pair<std::string, const std::vector<uint8_t>*>> files;

    for (const auto& entry : entries) {
        if (entry.isDirectory) continue;
        if (entry.path.substr(0, prefixSlash.size()) != prefixSlash) continue;

        std::string relPath = entry.path.substr(prefixSlash.size());
        if (relPath.empty()) continue;

        files.emplace_back(relPath, &entry.data);
    }

    if (files.empty()) return "";

    // Sort by relative path
    std::sort(files.begin(), files.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });

    // Build concatenation: path + '\0' + hex_hash + '\n'
    std::vector<uint8_t> concat;
    for (const auto& [relPath, data] : files) {
        std::string fileHash = sha256Hex(*data);
        concat.insert(concat.end(), relPath.begin(), relPath.end());
        concat.push_back('\0');
        concat.insert(concat.end(), fileHash.begin(), fileHash.end());
        concat.push_back('\n');
    }

    return sha256Hex(concat);
}

std::string computeParentDirectoryHash(
    const std::map<std::string, std::string>& childHashes)
{
    if (childHashes.empty()) return "";

    // childHashes is already sorted (std::map)
    std::vector<uint8_t> concat;
    for (const auto& [name, hash] : childHashes) {
        concat.insert(concat.end(), name.begin(), name.end());
        concat.push_back('\0');
        concat.insert(concat.end(), hash.begin(), hash.end());
        concat.push_back('\n');
    }

    return sha256Hex(concat);
}

std::map<std::string, std::string> computeMerkleTree(
    const std::vector<TarEntry>& entries)
{
    std::map<std::string, std::string> result;

    // Discover all top-level directories and their children
    // Skip manifest.json and manifest.sig
    std::set<std::string> topLevelDirs;
    // For "variants", also track child directories
    std::set<std::string> variantChildren;

    for (const auto& entry : entries) {
        if (entry.path == "manifest.json" || entry.path == "manifest.sig") continue;

        // Get first path component
        size_t slashPos = entry.path.find('/');
        std::string topDir;
        if (slashPos == std::string::npos) {
            // Top-level file (shouldn't exist per spec, but handle gracefully)
            continue;
        }
        topDir = entry.path.substr(0, slashPos);
        topLevelDirs.insert(topDir);

        // For "variants", track child directories
        if (topDir == "variants") {
            size_t secondSlash = entry.path.find('/', slashPos + 1);
            if (secondSlash != std::string::npos || entry.isDirectory) {
                std::string variantName;
                if (secondSlash != std::string::npos) {
                    variantName = entry.path.substr(slashPos + 1, secondSlash - slashPos - 1);
                } else {
                    // Directory entry like "variants/darwin-arm64"
                    variantName = entry.path.substr(slashPos + 1);
                    // Remove trailing slash if present
                    while (!variantName.empty() && variantName.back() == '/') {
                        variantName.pop_back();
                    }
                }
                if (!variantName.empty()) {
                    variantChildren.insert(variantName);
                }
            }
        }
    }

    // Compute leaf hashes for non-variant top-level directories (docs, licenses, etc.)
    std::map<std::string, std::string> topLevelHashes;

    for (const auto& topDir : topLevelDirs) {
        if (topDir == "variants") {
            // Handle variants separately (parent directory)
            std::map<std::string, std::string> variantHashes;
            for (const auto& variant : variantChildren) {
                std::string prefix = "variants/" + variant;
                std::string hash = computeLeafDirectoryHash(entries, prefix);
                if (!hash.empty()) {
                    variantHashes[variant] = hash;
                    result["variants/" + variant] = hash;
                }
            }
            if (!variantHashes.empty()) {
                std::string variantsHash = computeParentDirectoryHash(variantHashes);
                result["variants"] = variantsHash;
                topLevelHashes["variants"] = variantsHash;
            }
        } else {
            // Leaf directory (docs, licenses, etc.)
            std::string hash = computeLeafDirectoryHash(entries, topDir);
            if (!hash.empty()) {
                result[topDir] = hash;
                topLevelHashes[topDir] = hash;
            }
        }
    }

    // Compute root hash
    if (!topLevelHashes.empty()) {
        result["root"] = computeParentDirectoryHash(topLevelHashes);
    }

    return result;
}

std::string base64UrlEncode(const uint8_t* data, size_t len) {
    init();
    size_t b64Len = sodium_base64_encoded_len(len, sodium_base64_VARIANT_URLSAFE_NO_PADDING);
    std::string result(b64Len, '\0');
    sodium_bin2base64(&result[0], b64Len, data, len, sodium_base64_VARIANT_URLSAFE_NO_PADDING);
    // Remove trailing null
    while (!result.empty() && result.back() == '\0') {
        result.pop_back();
    }
    return result;
}

std::string base64UrlEncode(const std::string& data) {
    return base64UrlEncode(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

std::optional<std::vector<uint8_t>> base64UrlDecode(const std::string& encoded) {
    init();
    std::vector<uint8_t> decoded(encoded.size());
    size_t decodedLen = 0;
    if (sodium_base642bin(decoded.data(), decoded.size(),
                          encoded.c_str(), encoded.size(),
                          nullptr, &decodedLen, nullptr,
                          sodium_base64_VARIANT_URLSAFE_NO_PADDING) != 0) {
        return std::nullopt;
    }
    decoded.resize(decodedLen);
    return decoded;
}

std::string publicKeyToDid(const PublicKey& pk) {
    // 1. Base64url-encode the 32-byte public key (no padding)
    std::string x = base64UrlEncode(pk.data(), pk.size());

    // 2. Construct minimal JWK with keys sorted alphabetically: crv, kty, x
    std::string jwk = "{\"crv\":\"Ed25519\",\"kty\":\"OKP\",\"x\":\"" + x + "\"}";

    // 3. Base64url-encode the JWK JSON string
    std::string jwkEncoded = base64UrlEncode(jwk);

    // 4. Prepend did:jwk:
    return "did:jwk:" + jwkEncoded;
}

std::optional<PublicKey> didToPublicKey(const std::string& did) {
    // Check prefix
    const std::string prefix = "did:jwk:";
    if (did.size() <= prefix.size() || did.substr(0, prefix.size()) != prefix) {
        return std::nullopt;
    }

    // Decode the JWK portion
    std::string jwkEncoded = did.substr(prefix.size());
    auto jwkBytes = base64UrlDecode(jwkEncoded);
    if (!jwkBytes) {
        return std::nullopt;
    }

    std::string jwkStr(jwkBytes->begin(), jwkBytes->end());

    // Parse the "x" field from the JWK JSON
    // Simple parsing: find "x":"<value>"
    const std::string xKey = "\"x\":\"";
    auto xPos = jwkStr.find(xKey);
    if (xPos == std::string::npos) {
        return std::nullopt;
    }

    size_t valueStart = xPos + xKey.size();
    auto valueEnd = jwkStr.find('"', valueStart);
    if (valueEnd == std::string::npos) {
        return std::nullopt;
    }

    std::string xValue = jwkStr.substr(valueStart, valueEnd - valueStart);

    // Decode the base64url x value to get the 32-byte public key
    auto pkBytes = base64UrlDecode(xValue);
    if (!pkBytes || pkBytes->size() != PUBLIC_KEY_SIZE) {
        return std::nullopt;
    }

    PublicKey pk;
    std::copy(pkBytes->begin(), pkBytes->end(), pk.begin());
    return pk;
}

} // namespace crypto
} // namespace lgx
