#include "manifest_sig.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace lgx {
namespace crypto {

thread_local std::string ManifestSig::lastError_;

std::string ManifestSig::toJson() const {
    json j;
    j["algorithm"] = algorithm;
    j["did"] = did;
    j["linkedDids"] = json::array();
    for (const auto& ld : linkedDids) {
        j["linkedDids"].push_back(ld);
    }
    j["signature"] = signature;
    // signer object (always present, even if empty)
    json signer = json::object();
    if (!signerName.empty()) {
        signer["name"] = signerName;
    }
    if (!signerUrl.empty()) {
        signer["url"] = signerUrl;
    }
    j["signer"] = signer;
    j["version"] = version;
    return j.dump(2);
}

std::optional<ManifestSig> ManifestSig::fromJson(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);

        ManifestSig sig;

        if (!j.contains("version") || !j["version"].is_number_integer()) {
            lastError_ = "Missing or invalid 'version' field";
            return std::nullopt;
        }
        sig.version = j["version"].get<int>();

        if (sig.version != 1) {
            lastError_ = "Unsupported manifest.sig version: " + std::to_string(sig.version);
            return std::nullopt;
        }

        if (!j.contains("algorithm") || !j["algorithm"].is_string()) {
            lastError_ = "Missing or invalid 'algorithm' field";
            return std::nullopt;
        }
        sig.algorithm = j["algorithm"].get<std::string>();

        if (sig.algorithm != "ed25519") {
            lastError_ = "Unsupported algorithm: " + sig.algorithm;
            return std::nullopt;
        }

        if (!j.contains("did") || !j["did"].is_string()) {
            lastError_ = "Missing or invalid 'did' field";
            return std::nullopt;
        }
        sig.did = j["did"].get<std::string>();

        if (!j.contains("signature") || !j["signature"].is_string()) {
            lastError_ = "Missing or invalid 'signature' field";
            return std::nullopt;
        }
        sig.signature = j["signature"].get<std::string>();

        // Optional signer metadata
        if (j.contains("signer") && j["signer"].is_object()) {
            auto& signer = j["signer"];
            if (signer.contains("name") && signer["name"].is_string()) {
                sig.signerName = signer["name"].get<std::string>();
            }
            if (signer.contains("url") && signer["url"].is_string()) {
                sig.signerUrl = signer["url"].get<std::string>();
            }
        }

        // Optional linkedDids array
        if (j.contains("linkedDids") && j["linkedDids"].is_array()) {
            for (const auto& ld : j["linkedDids"]) {
                if (ld.is_string()) {
                    sig.linkedDids.push_back(ld.get<std::string>());
                }
            }
        }

        return sig;
    } catch (const json::exception& e) {
        lastError_ = std::string("JSON parse error: ") + e.what();
        return std::nullopt;
    }
}

std::string ManifestSig::getLastError() {
    return lastError_;
}

} // namespace crypto
} // namespace lgx
