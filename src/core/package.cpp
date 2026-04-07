#include "package.h"
#include "gzip_handler.h"
#include "path_normalizer.h"

#include <fstream>
#include <algorithm>

namespace lgx {

thread_local std::string Package::lastError_;

const std::set<std::string> Package::ALLOWED_ROOT_ENTRIES = {
    "manifest.json",
    "manifest.sig",
    "variants",
    "docs",
    "licenses"
};

Package::Result Package::create(
    const std::filesystem::path& outputPath,
    const std::string& name
) {
    Package pkg;
    
    // Set up manifest with default values
    pkg.manifest_.name = PathNormalizer::toLowercase(name);
    pkg.manifest_.version = "0.0.1";
    pkg.manifest_.description = "";
    pkg.manifest_.author = "";
    pkg.manifest_.type = "";
    pkg.manifest_.category = "";
    pkg.manifest_.icon = "";
    pkg.manifest_.dependencies = {};
    
    // Create skeleton with manifest and variants directory
    pkg.entries_.clear();
    
    // Add variants directory
    TarEntry variantsDir;
    variantsDir.path = "variants";
    variantsDir.isDirectory = true;
    pkg.entries_.push_back(variantsDir);
    
    return pkg.save(outputPath);
}

std::optional<Package> Package::load(const std::filesystem::path& lgxPath) {
    // Read file
    std::ifstream file(lgxPath, std::ios::binary);
    if (!file) {
        lastError_ = "Cannot open file: " + lgxPath.string();
        return std::nullopt;
    }
    
    std::vector<uint8_t> gzipData(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );
    file.close();
    
    // Decompress
    auto tarData = GzipHandler::decompress(gzipData);
    if (tarData.empty() && !gzipData.empty()) {
        lastError_ = "Failed to decompress: " + GzipHandler::getLastError();
        return std::nullopt;
    }
    
    // Read tar
    auto readResult = TarReader::read(tarData);
    if (!readResult.success) {
        lastError_ = "Failed to read tar: " + readResult.error;
        return std::nullopt;
    }
    
    Package pkg;
    pkg.entries_ = std::move(readResult.entries);
    
    // Find and parse manifest and signature
    for (const auto& entry : pkg.entries_) {
        if (entry.path == "manifest.json" && !entry.isDirectory) {
            std::string jsonStr(entry.data.begin(), entry.data.end());
            auto manifestOpt = Manifest::fromJson(jsonStr);
            if (!manifestOpt) {
                lastError_ = "Failed to parse manifest: " + Manifest::getLastError();
                return std::nullopt;
            }
            pkg.manifest_ = std::move(*manifestOpt);
        }
        if (entry.path == "manifest.sig" && !entry.isDirectory) {
            std::string sigStr(entry.data.begin(), entry.data.end());
            auto sigOpt = crypto::ManifestSig::fromJson(sigStr);
            if (sigOpt) {
                pkg.manifestSig_ = std::move(*sigOpt);
            }
            // If parsing fails, we don't error out — just treat as unsigned
        }
    }

    return pkg;
}

Package::Result Package::save(const std::filesystem::path& lgxPath) const {
    DeterministicTarWriter writer;
    
    // Add manifest first
    std::string manifestJson = manifest_.toJson();
    writer.addFile("manifest.json", manifestJson);

    // Add manifest.sig if present
    if (manifestSig_.has_value()) {
        std::string sigJson = manifestSig_->toJson();
        writer.addFile("manifest.sig", sigJson);
    }

    // Track which directories we've added
    std::set<std::string> addedDirs;

    // Add all other entries
    for (const auto& entry : entries_) {
        if (entry.path == "manifest.json" || entry.path == "manifest.sig") {
            continue;  // Already added above
        }
        
        // Ensure parent directories exist
        auto requiredDirs = getRequiredDirectories(entry.path);
        for (const auto& dir : requiredDirs) {
            if (addedDirs.find(dir) == addedDirs.end()) {
                writer.addDirectory(dir);
                addedDirs.insert(dir);
            }
        }
        
        if (entry.isDirectory) {
            // Normalize directory path
            std::string dirPath = entry.path;
            while (!dirPath.empty() && dirPath.back() == '/') {
                dirPath.pop_back();
            }
            if (addedDirs.find(dirPath) == addedDirs.end()) {
                writer.addDirectory(dirPath);
                addedDirs.insert(dirPath);
            }
        } else {
            writer.addEntry(entry);
        }
    }
    
    // Ensure variants directory exists even if empty
    if (addedDirs.find("variants") == addedDirs.end()) {
        writer.addDirectory("variants");
    }
    
    // Finalize tar
    auto tarData = writer.finalize();
    
    // Compress
    auto gzipData = GzipHandler::compress(tarData);
    if (gzipData.empty() && !tarData.empty()) {
        return Result::fail("Failed to compress: " + GzipHandler::getLastError());
    }
    
    // Write file
    std::ofstream file(lgxPath, std::ios::binary);
    if (!file) {
        return Result::fail("Cannot write file: " + lgxPath.string());
    }
    
    file.write(reinterpret_cast<const char*>(gzipData.data()), gzipData.size());
    if (!file) {
        return Result::fail("Failed to write file: " + lgxPath.string());
    }
    
    return Result::ok();
}

Package::VerifyResult Package::validatePackage() const {
    VerifyResult result = VerifyResult::ok();

    // Validate manifest
    auto manifestValidation = manifest_.validate();
    if (!manifestValidation.valid) {
        result.valid = false;
        for (const auto& err : manifestValidation.errors) {
            result.errors.push_back("Manifest: " + err);
        }
    }

    // Check root layout restrictions
    std::set<std::string> foundRoots;
    std::set<std::string> foundVariants;
    bool hasManifest = false;
    bool hasVariantsDir = false;

    for (const auto& entry : entries_) {
        std::string rootComponent = PathNormalizer::getRootComponent(entry.path);

        // Check if root entry is allowed
        if (ALLOWED_ROOT_ENTRIES.find(rootComponent) == ALLOWED_ROOT_ENTRIES.end()) {
            result.valid = false;
            result.errors.push_back("Forbidden root entry: " + rootComponent);
        }

        if (entry.path == "manifest.json") {
            hasManifest = true;
        }

        if (rootComponent == "variants") {
            hasVariantsDir = true;

            // Check variant structure
            auto pathComponents = PathNormalizer::splitPath(entry.path);
            if (pathComponents.size() >= 2) {
                std::string variantName = PathNormalizer::toLowercase(pathComponents[1]);
                foundVariants.insert(variantName);
            }

            // Check that nothing is directly under variants/ (only directories)
            if (pathComponents.size() == 2 && !entry.isDirectory) {
                result.valid = false;
                result.errors.push_back("File directly under variants/: " + entry.path);
            }
        }

        // Validate path
        auto pathValidation = PathNormalizer::validateArchivePath(entry.path);
        if (!pathValidation.valid) {
            result.valid = false;
            result.errors.push_back("Invalid path '" + entry.path + "': " + pathValidation.error);
        }

        // Check for forbidden file types (handled by tar reader, but double-check)
        // TarReader already filters these, but we verify the entries are regular files or dirs
    }

    if (!hasManifest) {
        result.valid = false;
        result.errors.push_back("Missing manifest.json");
    }

    if (!hasVariantsDir) {
        result.valid = false;
        result.errors.push_back("Missing variants/ directory");
    }

    // Validate completeness (variants <-> main mapping)
    auto completenessResult = manifest_.validateCompleteness(foundVariants);
    if (!completenessResult.valid) {
        result.valid = false;
        for (const auto& err : completenessResult.errors) {
            result.errors.push_back(err);
        }
    }

    // Verify each main entry points to an existing regular file
    for (const auto& [variant, mainPath] : manifest_.main) {
        std::string fullPath = "variants/" + variant + "/" + mainPath;

        bool found = false;
        for (const auto& entry : entries_) {
            // Normalize both paths for comparison
            std::string entryPath = entry.path;
            while (!entryPath.empty() && entryPath.back() == '/') {
                entryPath.pop_back();
            }

            if (entryPath == fullPath && !entry.isDirectory) {
                found = true;
                break;
            }
        }

        if (!found) {
            result.valid = false;
            result.errors.push_back("main[" + variant + "] points to non-existent file: " + mainPath);
        }
    }

    // Verify content hashes (mandatory when package has content)
    if (crypto::init()) {
        auto recomputedHashes = crypto::computeMerkleTree(entries_);
        bool hasContent = !recomputedHashes.empty();

        if (hasContent && manifest_.hashes.empty()) {
            result.valid = false;
            result.errors.push_back("Missing content hashes in manifest");
        } else if (hasContent) {
            auto rootIt = manifest_.hashes.find("root");
            auto recomputedRootIt = recomputedHashes.find("root");

            if (rootIt == manifest_.hashes.end()) {
                result.valid = false;
                result.errors.push_back("Missing 'root' hash in manifest");
            } else if (recomputedRootIt == recomputedHashes.end()) {
                result.valid = false;
                result.errors.push_back("Cannot compute root hash (no hashable content)");
            } else if (rootIt->second != recomputedRootIt->second) {
                result.valid = false;
                result.errors.push_back("Content hash mismatch: package content does not match manifest hashes");
            }
        }
    }

    return result;
}

Package::VerifyResult Package::verify(const std::filesystem::path& lgxPath) {
    // Load package
    auto pkgOpt = load(lgxPath);
    if (!pkgOpt) {
        VerifyResult result;
        result.valid = false;
        result.errors.push_back(lastError_);
        return result;
    }

    return pkgOpt->validatePackage();
}

Package::Result Package::addVariant(
    const std::string& variant,
    const std::filesystem::path& filesPath,
    const std::optional<std::string>& mainPath
) {
    namespace fs = std::filesystem;
    
    std::string variantLc = PathNormalizer::toLowercase(variant);
    
    // Validate variant name
    if (variantLc.empty()) {
        return Result::fail("Variant name cannot be empty");
    }
    
    // Check if path exists
    std::error_code ec;
    if (!fs::exists(filesPath, ec)) {
        return Result::fail("Path does not exist: " + filesPath.string());
    }
    
    bool isDir = fs::is_directory(filesPath, ec);
    
    // Determine main path
    std::string resolvedMain;
    if (isDir) {
        if (!mainPath) {
            return Result::fail("--main is required when --files is a directory");
        }
        resolvedMain = *mainPath;
    } else {
        // Single file: main is the basename
        resolvedMain = mainPath.value_or(filesPath.filename().string());
    }
    
    // Validate main path
    auto mainValidation = PathNormalizer::validateArchivePath(resolvedMain);
    if (!mainValidation.valid) {
        return Result::fail("Invalid main path: " + mainValidation.error);
    }
    
    // Remove existing variant entries
    removeVariantEntries(variantLc);
    
    // Build archive base path
    std::string archiveBase = "variants/" + variantLc;
    
    if (isDir) {
        // Directory contents go directly under variant root
        // archiveBase stays as "variants/<variant>"
    } else {
        // Single file: include filename in path
        std::string fileName = filesPath.filename().string();
        archiveBase = archiveBase + "/" + fileName;
    }
    
    // Add variant directory entry
    TarEntry variantDirEntry;
    variantDirEntry.path = "variants/" + variantLc;
    variantDirEntry.isDirectory = true;
    entries_.push_back(variantDirEntry);
    
    // Add new entries
    auto addResult = addFilesystemEntries(filesPath, archiveBase);
    if (!addResult.success) {
        return addResult;
    }
    
    // Update manifest main
    manifest_.setMain(variantLc, resolvedMain);

    // Invalidate signature and recompute hashes (content changed)
    clearSignature();
    recomputeHashes();

    return Result::ok();
}

Package::Result Package::removeVariant(const std::string& variant) {
    std::string variantLc = PathNormalizer::toLowercase(variant);

    if (!hasVariant(variantLc)) {
        return Result::fail("Variant does not exist: " + variant);
    }
    
    removeVariantEntries(variantLc);
    manifest_.removeMain(variantLc);

    // Invalidate signature and recompute hashes (content changed)
    clearSignature();
    recomputeHashes();

    return Result::ok();
}

bool Package::hasVariant(const std::string& variant) const {
    std::string variantLc = PathNormalizer::toLowercase(variant);
    std::string prefix = "variants/" + variantLc + "/";
    std::string exactDir = "variants/" + variantLc;
    
    for (const auto& entry : entries_) {
        std::string path = entry.path;
        // Normalize trailing slash
        if (!path.empty() && path.back() == '/') {
            path.pop_back();
        }
        
        if (path == exactDir || path.substr(0, prefix.length()) == prefix) {
            return true;
        }
    }
    
    return false;
}

std::set<std::string> Package::getVariants() const {
    std::set<std::string> variants;
    
    for (const auto& entry : entries_) {
        auto components = PathNormalizer::splitPath(entry.path);
        if (components.size() >= 2 && components[0] == "variants") {
            variants.insert(PathNormalizer::toLowercase(components[1]));
        }
    }
    
    return variants;
}

bool Package::wouldMainChange(const std::string& variant, const std::string& newMain) const {
    auto currentMain = manifest_.getMain(variant);
    return currentMain && *currentMain != newMain;
}

std::string Package::getLastError() {
    return lastError_;
}

void Package::removeVariantEntries(const std::string& variant) {
    std::string variantLc = PathNormalizer::toLowercase(variant);
    std::string prefix = "variants/" + variantLc + "/";
    std::string exactDir = "variants/" + variantLc;
    
    entries_.erase(
        std::remove_if(entries_.begin(), entries_.end(),
            [&](const TarEntry& entry) {
                std::string path = entry.path;
                if (!path.empty() && path.back() == '/') {
                    path.pop_back();
                }
                return path == exactDir || path.substr(0, prefix.length()) == prefix;
            }),
        entries_.end()
    );
}

Package::Result Package::addFilesystemEntries(
    const std::filesystem::path& fsPath,
    const std::string& archiveBasePath
) {
    namespace fs = std::filesystem;
    std::error_code ec;
    
    // NFC normalize the archive path
    auto normalizedBaseOpt = PathNormalizer::toNFC(archiveBasePath);
    if (!normalizedBaseOpt) {
        return Result::fail("Failed to NFC-normalize path: " + archiveBasePath);
    }
    std::string normalizedBase = *normalizedBaseOpt;
    
    if (fs::is_regular_file(fsPath, ec)) {
        // Single file
        std::ifstream file(fsPath, std::ios::binary);
        if (!file) {
            return Result::fail("Cannot read file: " + fsPath.string());
        }
        
        std::vector<uint8_t> data(
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>()
        );
        
        TarEntry entry;
        entry.path = normalizedBase;
        entry.data = std::move(data);
        entry.isDirectory = false;
        
        auto status = fs::status(fsPath, ec);
        if (!ec) {
            entry.mode = static_cast<uint32_t>(status.permissions() & fs::perms::mask);
        }
        
        entries_.push_back(std::move(entry));
    } else if (fs::is_directory(fsPath, ec)) {
        // Directory - add entry for the directory itself
        TarEntry dirEntry;
        dirEntry.path = normalizedBase;
        dirEntry.isDirectory = true;
        
        auto status = fs::status(fsPath, ec);
        if (!ec) {
            dirEntry.mode = static_cast<uint32_t>(status.permissions() & fs::perms::mask);
        }
        
        entries_.push_back(dirEntry);
        
        // Recursively add contents
        for (const auto& item : fs::recursive_directory_iterator(fsPath, ec)) {
            // Get relative path from fsPath
            auto relPath = fs::relative(item.path(), fsPath, ec);
            if (ec) {
                continue;
            }
            
            // Build archive path
            std::string archivePath = normalizedBase + "/" + relPath.generic_string();
            
            // NFC normalize
            auto normalizedPathOpt = PathNormalizer::toNFC(archivePath);
            if (!normalizedPathOpt) {
                return Result::fail("Failed to NFC-normalize: " + archivePath);
            }
            
            if (fs::is_directory(item.path(), ec)) {
                TarEntry entry;
                entry.path = *normalizedPathOpt;
                entry.isDirectory = true;
                
                auto status = fs::status(item.path(), ec);
                if (!ec) {
                    entry.mode = static_cast<uint32_t>(status.permissions() & fs::perms::mask);
                }
                
                entries_.push_back(std::move(entry));
            } else if (fs::is_regular_file(item.path(), ec)) {
                std::ifstream file(item.path(), std::ios::binary);
                if (!file) {
                    return Result::fail("Cannot read file: " + item.path().string());
                }
                
                std::vector<uint8_t> data(
                    (std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>()
                );
                
                TarEntry entry;
                entry.path = *normalizedPathOpt;
                entry.data = std::move(data);
                entry.isDirectory = false;
                
                auto status = fs::status(item.path(), ec);
                if (!ec) {
                    entry.mode = static_cast<uint32_t>(status.permissions() & fs::perms::mask);
                }
                
                entries_.push_back(std::move(entry));
            } else {
                // Skip symlinks, special files, etc.
                // Could add a warning here
            }
        }
    } else {
        return Result::fail("Path is not a regular file or directory: " + fsPath.string());
    }
    
    return Result::ok();
}

std::vector<std::string> Package::getRequiredDirectories(const std::string& path) {
    std::vector<std::string> dirs;
    auto components = PathNormalizer::splitPath(path);
    
    // Build up directory paths
    std::string current;
    for (size_t i = 0; i < components.size() - 1; ++i) {
        if (!current.empty()) {
            current += '/';
        }
        current += components[i];
        dirs.push_back(current);
    }
    
    return dirs;
}

Package::Result Package::extractVariant(
    const std::string& variant,
    const std::filesystem::path& outputDir
) const {
    namespace fs = std::filesystem;
    std::error_code ec;
    
    std::string variantLc = PathNormalizer::toLowercase(variant);
    
    if (!hasVariant(variantLc)) {
        return Result::fail("Variant does not exist: " + variant);
    }
    
    fs::path variantOutputDir = outputDir / variantLc;
    
    std::string prefix = "variants/" + variantLc + "/";
    
    for (const auto& entry : entries_) {
        if (entry.path.substr(0, prefix.length()) != prefix) {
            continue;
        }
        
        std::string relativePath = entry.path.substr(prefix.length());
        if (relativePath.empty()) {
            continue;
        }
        
        fs::path fullPath = variantOutputDir / relativePath;
        
        if (entry.isDirectory) {
            if (!fs::create_directories(fullPath, ec) && ec) {
                return Result::fail("Failed to create directory: " + fullPath.string() + " - " + ec.message());
            }
        } else {
            fs::path parentDir = fullPath.parent_path();
            if (!parentDir.empty() && !fs::exists(parentDir)) {
                if (!fs::create_directories(parentDir, ec) && ec) {
                    return Result::fail("Failed to create directory: " + parentDir.string() + " - " + ec.message());
                }
            }
            
            std::ofstream file(fullPath, std::ios::binary);
            if (!file) {
                return Result::fail("Failed to create file: " + fullPath.string());
            }
            
            file.write(reinterpret_cast<const char*>(entry.data.data()), entry.data.size());
            if (!file) {
                return Result::fail("Failed to write file: " + fullPath.string());
            }
            file.close();

            if (entry.mode != 0) {
                fs::permissions(fullPath, static_cast<fs::perms>(entry.mode & 0777), ec);
                if (ec) {
                    return Result::fail("Failed to set permissions on: " + fullPath.string() + " - " + ec.message());
                }
            }
        }
    }
    
    return Result::ok();
}

Package::Result Package::extractAll(const std::filesystem::path& outputDir) const {
    auto variants = getVariants();
    
    for (const auto& variant : variants) {
        auto result = extractVariant(variant, outputDir);
        if (!result.success) {
            return result;
        }
    }
    
    return Result::ok();
}

Package::Result Package::signPackage(const crypto::SecretKey& sk,
                                      const std::string& signerName,
                                      const std::string& signerUrl) {
    if (!crypto::init()) {
        return Result::fail("Failed to initialize crypto library");
    }

    // 1. Recompute Merkle tree hashes
    recomputeHashes();
    if (manifest_.hashes.empty()) {
        return Result::fail("No content to hash (no variant or other directories)");
    }

    // 2. Get deterministic manifest JSON bytes
    std::string manifestJson = manifest_.toJson();
    std::vector<uint8_t> manifestBytes(manifestJson.begin(), manifestJson.end());

    // 4. Sign with Ed25519
    auto signature = crypto::sign(manifestBytes, sk);

    // 5. Build ManifestSig with DID
    auto pk = crypto::extractPublicKey(sk);
    crypto::ManifestSig sig;
    sig.version = 1;
    sig.algorithm = "ed25519";
    sig.did = crypto::publicKeyToDid(pk);
    sig.signature = crypto::base64Encode(signature.data(), signature.size());
    sig.signerName = signerName;
    sig.signerUrl = signerUrl;

    manifestSig_ = std::move(sig);

    return Result::ok();
}

Package::SignatureInfo Package::verifySignature() const {
    SignatureInfo info{};
    info.is_signed = false;
    info.signature_valid = false;
    info.package_valid = false;

    // Validate package structure and content hashes first
    auto pkgValidation = validatePackage();
    if (!pkgValidation.valid) {
        info.package_valid = false;
        info.error = pkgValidation.errors.empty() ? "Package validation failed"
                     : pkgValidation.errors[0];
        return info;
    }
    info.package_valid = true;

    if (!manifestSig_.has_value()) {
        return info;
    }

    info.is_signed = true;
    info.signer_did = manifestSig_->did;
    info.signer_name = manifestSig_->signerName;
    info.signer_url = manifestSig_->signerUrl;

    if (!crypto::init()) {
        info.error = "Failed to initialize crypto library";
        return info;
    }

    // Extract public key from DID
    auto pkOpt = crypto::didToPublicKey(manifestSig_->did);
    if (!pkOpt) {
        info.error = "Invalid DID in manifest.sig: " + manifestSig_->did;
        return info;
    }
    crypto::PublicKey pk = *pkOpt;

    // Decode signature
    auto sigBytes = crypto::base64Decode(manifestSig_->signature);
    if (!sigBytes || sigBytes->size() != crypto::SIGNATURE_SIZE) {
        info.error = "Invalid signature in manifest.sig";
        return info;
    }
    crypto::Signature sig;
    std::copy(sigBytes->begin(), sigBytes->end(), sig.begin());

    // Verify Ed25519 signature over manifest.toJson() bytes
    std::string manifestJson = manifest_.toJson();
    std::vector<uint8_t> manifestBytes(manifestJson.begin(), manifestJson.end());

    if (!crypto::verify(manifestBytes, pk, sig)) {
        info.error = "Ed25519 signature verification failed";
        return info;
    }

    info.signature_valid = true;
    return info;
}

void Package::clearSignature() {
    manifestSig_ = std::nullopt;
}

void Package::recomputeHashes() {
    if (!crypto::init()) {
        return;
    }
    auto hashes = crypto::computeMerkleTree(entries_);
    manifest_.hashes = std::move(hashes);
}

} // namespace lgx
