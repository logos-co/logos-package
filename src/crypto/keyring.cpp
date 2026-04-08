#include "keyring.h"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>

#include <sodium.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace lgx {
namespace crypto {

thread_local std::string Keyring::lastError_;

bool Keyring::validateKeyName(const std::string& name) {
    if (name.empty()) {
        lastError_ = "Key name cannot be empty";
        return false;
    }
    if (name.find('/') != std::string::npos ||
        name.find('\\') != std::string::npos ||
        name.find("..") != std::string::npos) {
        lastError_ = "Key name contains invalid characters (path separators or '..' are not allowed)";
        return false;
    }
    return true;
}

Keyring::Keyring(const std::filesystem::path& dir) : dir_(dir) {
    ensureDirectory(dir_);
}

std::filesystem::path Keyring::defaultDirectory() {
    namespace fs = std::filesystem;

    const char* xdgConfig = std::getenv("XDG_CONFIG_HOME");
    fs::path configDir;
    if (xdgConfig && xdgConfig[0] != '\0') {
        configDir = fs::path(xdgConfig);
    } else {
        const char* home = std::getenv("HOME");
        if (!home) {
            return fs::path{};
        }
        configDir = fs::path(home) / ".config";
    }
    return configDir / "logos" / "trusted-keys";
}

std::filesystem::path Keyring::defaultKeysDirectory() {
    namespace fs = std::filesystem;

    const char* xdgConfig = std::getenv("XDG_CONFIG_HOME");
    fs::path configDir;
    if (xdgConfig && xdgConfig[0] != '\0') {
        configDir = fs::path(xdgConfig);
    } else {
        const char* home = std::getenv("HOME");
        if (!home) {
            return fs::path{};
        }
        configDir = fs::path(home) / ".config";
    }
    return configDir / "logos" / "keys";
}

bool Keyring::addKey(const std::string& name, const std::string& did,
                     const std::string& displayName, const std::string& url) {
    namespace fs = std::filesystem;

    if (!validateKeyName(name)) {
        return false;
    }

    // Validate DID by extracting public key
    auto pk = didToPublicKey(did);
    if (!pk) {
        lastError_ = "Invalid DID string: " + did;
        return false;
    }

    auto filePath = dir_ / (name + ".json");

    json j;
    j["did"] = did;
    if (!displayName.empty()) {
        j["name"] = displayName;
    }
    if (!url.empty()) {
        j["url"] = url;
    }
    j["addedAt"] = currentTimestamp();

    std::ofstream file(filePath);
    if (!file) {
        lastError_ = "Cannot write key file: " + filePath.string();
        return false;
    }

    file << j.dump(2) << "\n";
    file.close();

    // Set 0600 permissions
    std::error_code ec;
    fs::permissions(filePath, fs::perms::owner_read | fs::perms::owner_write,
                    fs::perm_options::replace, ec);
    if (ec) {
        lastError_ = "Failed to set permissions on: " + filePath.string();
        return false;
    }

    return true;
}

bool Keyring::removeKey(const std::string& name) {
    namespace fs = std::filesystem;

    if (!validateKeyName(name)) {
        return false;
    }

    auto filePath = dir_ / (name + ".json");
    std::error_code ec;
    if (!fs::exists(filePath, ec)) {
        lastError_ = "Key not found: " + name;
        return false;
    }

    if (!fs::remove(filePath, ec) || ec) {
        lastError_ = "Failed to remove key file: " + filePath.string();
        return false;
    }

    return true;
}

std::optional<TrustedKey> Keyring::findByDid(const std::string& did) const {
    auto keys = listKeys();
    for (const auto& key : keys) {
        if (key.did == did) {
            return key;
        }
    }
    return std::nullopt;
}

std::optional<TrustedKey> Keyring::findByPublicKey(const PublicKey& pk) const {
    std::string did = publicKeyToDid(pk);
    return findByDid(did);
}

std::optional<TrustedKey> Keyring::findByName(const std::string& name) const {
    namespace fs = std::filesystem;

    auto filePath = dir_ / (name + ".json");
    std::error_code ec;
    if (!fs::exists(filePath, ec)) {
        return std::nullopt;
    }

    std::ifstream file(filePath);
    if (!file) {
        return std::nullopt;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    try {
        json j = json::parse(content);
        if (!j.contains("did") || !j["did"].is_string()) {
            return std::nullopt;
        }

        std::string did = j["did"].get<std::string>();
        auto pk = didToPublicKey(did);
        if (!pk) {
            return std::nullopt;
        }

        TrustedKey key;
        key.name = name;
        key.did = did;
        key.publicKey = *pk;
        if (j.contains("name") && j["name"].is_string()) {
            key.displayName = j["name"].get<std::string>();
        }
        if (j.contains("url") && j["url"].is_string()) {
            key.url = j["url"].get<std::string>();
        }
        if (j.contains("addedAt") && j["addedAt"].is_string()) {
            key.addedAt = j["addedAt"].get<std::string>();
        }

        return key;
    } catch (...) {
        return std::nullopt;
    }
}

std::vector<TrustedKey> Keyring::listKeys() const {
    namespace fs = std::filesystem;

    std::vector<TrustedKey> keys;
    std::error_code ec;

    if (!fs::is_directory(dir_, ec)) {
        return keys;
    }

    for (const auto& entry : fs::directory_iterator(dir_, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".json") continue;

        std::string name = entry.path().stem().string();
        auto key = findByName(name);
        if (key) {
            keys.push_back(std::move(*key));
        }
    }

    // Sort by name for deterministic output
    std::sort(keys.begin(), keys.end(),
        [](const auto& a, const auto& b) { return a.name < b.name; });

    return keys;
}

bool Keyring::saveKeypair(
    const std::filesystem::path& keysDir,
    const std::string& name,
    const KeyPair& kp)
{
    namespace fs = std::filesystem;

    if (!validateKeyName(name)) {
        return false;
    }

    if (!ensureDirectory(keysDir)) {
        return false;
    }

    // Extract 32-byte seed from libsodium's 64-byte secret key
    unsigned char seed[crypto_sign_SEEDBYTES];
    if (crypto_sign_ed25519_sk_to_seed(seed, kp.secretKey.data()) != 0) {
        lastError_ = "Failed to extract seed from secret key";
        return false;
    }

    // Build JWK with alphabetically sorted keys: crv, d, kty, x
    std::string xValue = base64UrlEncode(kp.publicKey.data(), kp.publicKey.size());
    std::string dValue = base64UrlEncode(seed, crypto_sign_SEEDBYTES);

    json jwk;
    jwk["crv"] = "Ed25519";
    jwk["d"] = dValue;
    jwk["kty"] = "OKP";
    jwk["x"] = xValue;

    // Write JWK secret key file
    auto jwkPath = keysDir / (name + ".jwk");
    {
        std::ofstream file(jwkPath);
        if (!file) {
            lastError_ = "Cannot write secret key: " + jwkPath.string();
            return false;
        }
        file << jwk.dump(2) << "\n";
        file.close();

        std::error_code ec;
        fs::permissions(jwkPath, fs::perms::owner_read | fs::perms::owner_write,
                        fs::perm_options::replace, ec);
        if (ec) {
            lastError_ = "Failed to set permissions on: " + jwkPath.string();
            return false;
        }
    }

    // Write public key in SSH format
    auto pubPath = keysDir / (name + ".pub");
    {
        std::ofstream file(pubPath);
        if (!file) {
            lastError_ = "Cannot write public key: " + pubPath.string();
            return false;
        }
        file << formatSSHPubKey(kp.publicKey, name) << "\n";
        file.close();

        std::error_code ec;
        fs::permissions(pubPath, fs::perms::owner_read | fs::perms::owner_write,
                        fs::perm_options::replace, ec);
    }

    // Write DID file
    auto didPath = keysDir / (name + ".did");
    {
        std::string did = publicKeyToDid(kp.publicKey);
        std::ofstream file(didPath);
        if (!file) {
            lastError_ = "Cannot write DID file: " + didPath.string();
            return false;
        }
        file << did << "\n";
        file.close();

        std::error_code ec;
        fs::permissions(didPath, fs::perms::owner_read | fs::perms::owner_write,
                        fs::perm_options::replace, ec);
    }

    // Securely zero the seed
    sodium_memzero(seed, sizeof(seed));

    return true;
}

std::optional<SecretKey> Keyring::loadSecretKey(
    const std::filesystem::path& keysDir,
    const std::string& name)
{
    if (!validateKeyName(name)) {
        return std::nullopt;
    }

    auto jwkPath = keysDir / (name + ".jwk");

    std::ifstream file(jwkPath);
    if (!file) {
        lastError_ = "Cannot read secret key: " + jwkPath.string();
        return std::nullopt;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    try {
        json j = json::parse(content);

        if (!j.contains("d") || !j["d"].is_string()) {
            lastError_ = "Invalid JWK: missing 'd' field";
            return std::nullopt;
        }

        std::string dValue = j["d"].get<std::string>();
        auto seedBytes = base64UrlDecode(dValue);
        if (!seedBytes || seedBytes->size() != crypto_sign_SEEDBYTES) {
            lastError_ = "Invalid JWK: 'd' field has wrong size (expected " +
                         std::to_string(crypto_sign_SEEDBYTES) + " bytes, got " +
                         std::to_string(seedBytes ? seedBytes->size() : 0) + ")";
            return std::nullopt;
        }

        // Reconstruct the full 64-byte libsodium secret key from the seed
        PublicKey pk;
        SecretKey sk;
        if (crypto_sign_seed_keypair(pk.data(), sk.data(), seedBytes->data()) != 0) {
            lastError_ = "Failed to reconstruct keypair from seed";
            return std::nullopt;
        }

        return sk;
    } catch (const json::exception& e) {
        lastError_ = std::string("Failed to parse JWK: ") + e.what();
        return std::nullopt;
    }
}

std::string Keyring::getLastError() {
    return lastError_;
}

std::string Keyring::formatSSHPubKey(const PublicKey& pk, const std::string& name) {
    // Build SSH wire format for ed25519 public key
    std::vector<uint8_t> wireFormat;

    // Key type: "ssh-ed25519"
    const std::string keyType = "ssh-ed25519";
    uint32_t typeLen = static_cast<uint32_t>(keyType.size());
    wireFormat.push_back((typeLen >> 24) & 0xFF);
    wireFormat.push_back((typeLen >> 16) & 0xFF);
    wireFormat.push_back((typeLen >> 8) & 0xFF);
    wireFormat.push_back(typeLen & 0xFF);
    wireFormat.insert(wireFormat.end(), keyType.begin(), keyType.end());

    // Key data
    uint32_t keyLen = PUBLIC_KEY_SIZE;
    wireFormat.push_back((keyLen >> 24) & 0xFF);
    wireFormat.push_back((keyLen >> 16) & 0xFF);
    wireFormat.push_back((keyLen >> 8) & 0xFF);
    wireFormat.push_back(keyLen & 0xFF);
    wireFormat.insert(wireFormat.end(), pk.begin(), pk.end());

    return "ssh-ed25519 " + base64Encode(wireFormat.data(), wireFormat.size()) + " " + name;
}

bool Keyring::ensureDirectory(const std::filesystem::path& dir) {
    namespace fs = std::filesystem;
    std::error_code ec;

    if (fs::exists(dir, ec)) {
        if (!fs::is_directory(dir, ec)) {
            lastError_ = "Path exists but is not a directory: " + dir.string();
            return false;
        }
        return true;
    }

    if (!fs::create_directories(dir, ec) || ec) {
        lastError_ = "Failed to create directory: " + dir.string() + " - " + ec.message();
        return false;
    }

    // Set 0700 permissions
    fs::permissions(dir,
        fs::perms::owner_all,
        fs::perm_options::replace, ec);
    if (ec) {
        lastError_ = "Failed to set permissions on: " + dir.string();
        return false;
    }

    return true;
}

std::string Keyring::currentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&time, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

} // namespace crypto
} // namespace lgx
