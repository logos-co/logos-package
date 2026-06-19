#include "manifest_command.h"
#include "core/package.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>

namespace lgx {

namespace {

// Find the raw bytes of manifest.json inside the loaded package's tar entries.
// We intentionally return the bytes as they appear inside the .lgx (not the
// re-serialised in-memory representation) so callers like the release action
// receive the exact JSON that was signed/stored.
std::optional<std::string> findRawManifestBytes(const Package& pkg) {
    for (const auto& entry : pkg.getEntries()) {
        if (entry.path == "manifest.json" && !entry.isDirectory) {
            return std::string(entry.data.begin(), entry.data.end());
        }
    }
    return std::nullopt;
}

// Find the raw bytes of manifest.sig inside the loaded package's tar entries.
// Returns nullopt when the package is unsigned.
std::optional<std::string> findRawSignatureBytes(const Package& pkg) {
    for (const auto& entry : pkg.getEntries()) {
        if (entry.path == "manifest.sig" && !entry.isDirectory) {
            return std::string(entry.data.begin(), entry.data.end());
        }
    }
    return std::nullopt;
}

std::string joinKeys(const std::map<std::string, std::string>& m) {
    std::ostringstream out;
    bool first = true;
    for (const auto& [k, _] : m) {
        if (!first) out << ", ";
        out << k;
        first = false;
    }
    return out.str();
}

void printHumanReadable(const Package& pkg) {
    const Manifest& m = pkg.getManifest();

    std::cout << "Name:           " << m.name << "\n"
              << "Display name:   "
              << (m.displayName.empty() ? "(unset — falls back to name)" : m.displayName) << "\n"
              << "Version:        " << m.version << "\n"
              << "Manifest ver.:  " << m.manifestVersion << "\n"
              << "Type:           " << (m.type.empty() ? "(unset)" : m.type) << "\n"
              << "Category:       " << (m.category.empty() ? "(unset)" : m.category) << "\n"
              << "Author:         " << (m.author.empty() ? "(unset)" : m.author) << "\n";

    if (!m.description.empty()) {
        std::cout << "Description:    " << m.description << "\n";
    }
    if (!m.icon.empty()) {
        std::cout << "Icon:           " << m.icon << "\n";
    }
    if (!m.view.empty()) {
        std::cout << "View:           " << m.view << "\n";
    }

    auto rootIt = m.hashes.find("root");
    if (rootIt != m.hashes.end()) {
        std::cout << "Root hash:      " << rootIt->second << "\n";
    }

    if (m.main.empty()) {
        std::cout << "Variants:       (none)\n";
    } else {
        std::cout << "Variants:       " << joinKeys(m.main) << "\n";
        for (const auto& [variant, path] : m.main) {
            std::cout << "                " << variant << " -> " << path << "\n";
        }
    }

    if (m.dependencies.empty()) {
        std::cout << "Dependencies:   (none)\n";
    } else {
        std::cout << "Dependencies:\n";
        for (const auto& dep : m.dependencies) {
            std::cout << "  - " << dep.toString() << "\n";
        }
    }

    // Signature info (read directly from the package, do not verify)
    auto sigBytes = findRawSignatureBytes(pkg);
    if (sigBytes.has_value()) {
        try {
            auto j = nlohmann::json::parse(*sigBytes);
            std::cout << "Signed:         yes\n";
            if (j.contains("did") && j["did"].is_string()) {
                std::cout << "Signer DID:     " << j["did"].get<std::string>() << "\n";
            }
            if (j.contains("signer") && j["signer"].is_object()) {
                const auto& signer = j["signer"];
                if (signer.contains("name") && signer["name"].is_string()) {
                    std::cout << "Signer name:    " << signer["name"].get<std::string>()
                              << " (self-asserted)\n";
                }
                if (signer.contains("url") && signer["url"].is_string()) {
                    std::cout << "Signer URL:     " << signer["url"].get<std::string>()
                              << " (self-asserted)\n";
                }
            }
        } catch (const nlohmann::json::exception&) {
            std::cout << "Signed:         yes (manifest.sig present but unparseable)\n";
        }
    } else {
        std::cout << "Signed:         no\n";
    }
}

} // namespace

int ManifestCommand::execute(const std::vector<std::string>& args) {
    std::vector<std::string> positional;
    auto opts = parseArgs(args, positional);

    if (positional.empty()) {
        printError("Missing package path");
        std::cerr << "\nUsage: " << usage() << std::endl;
        return 1;
    }

    std::string pkgPath = positional[0];
    if (!std::filesystem::exists(pkgPath)) {
        printError("Package not found: " + pkgPath);
        return 1;
    }

    auto pkg = Package::load(pkgPath);
    if (!pkg) {
        printError("Failed to load package: " + Package::getLastError());
        return 1;
    }

    auto rawManifest = findRawManifestBytes(*pkg);
    if (!rawManifest.has_value()) {
        printError("Package does not contain a manifest.json entry");
        return 1;
    }

    const bool jsonMode = hasFlag(opts, "json");

    if (jsonMode) {
        // Write the raw manifest bytes verbatim, exactly as they live inside
        // the .lgx. No extra newline so the output is byte-identical to the
        // embedded file.
        std::cout.write(rawManifest->data(),
                        static_cast<std::streamsize>(rawManifest->size()));
        return 0;
    }

    printHumanReadable(*pkg);
    return 0;
}

} // namespace lgx
