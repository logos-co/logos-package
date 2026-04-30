#include "merge_command.h"
#include "core/package.h"
#include "core/path_normalizer.h"

#include <chrono>
#include <filesystem>
#include <set>

#include <unistd.h>

namespace lgx {

namespace {

// RAII helper to clean up a temporary directory on scope exit.
struct TempDir {
    std::filesystem::path path;

    explicit TempDir(const std::filesystem::path& p) : path(p) {}
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
};

} // anonymous namespace

int MergeCommand::execute(const std::vector<std::string>& args) {
    std::vector<std::string> positional;
    auto opts = parseArgs(args, positional);

    if (positional.size() < 2) {
        printError("At least two .lgx packages are required");
        std::cerr << "\nUsage: " << usage() << std::endl;
        return 1;
    }

    std::string outputPath = getOption(opts, "output", "o");
    bool autoYes = hasFlag(opts, "yes", "y");
    bool skipDuplicates = hasFlag(opts, "skip-duplicates");

    // Validate all input files exist
    for (const auto& path : positional) {
        if (!std::filesystem::exists(path)) {
            printError("Package not found: " + path);
            return 1;
        }
    }

    // Load all packages
    std::vector<Package> packages;
    packages.reserve(positional.size());

    for (const auto& path : positional) {
        auto pkgOpt = Package::load(path);
        if (!pkgOpt) {
            printError("Failed to load package '" + path + "': " + Package::getLastError());
            return 1;
        }
        packages.push_back(std::move(*pkgOpt));
    }

    // Compare manifests (ignoring the 'main' field which is variant-specific)
    const auto& refManifest = packages[0].getManifest();

    for (size_t i = 1; i < packages.size(); ++i) {
        auto comparison = refManifest.compareMetadata(packages[i].getManifest());
        if (!comparison.valid) {
            printError("Manifest mismatch between '" + positional[0] +
                       "' and '" + positional[i] + "':");
            for (const auto& err : comparison.errors) {
                std::cerr << "  - " << err << "\n";
            }
            return 1;
        }
    }

    // Check for duplicate variants across packages
    std::map<std::string, std::string> variantSource; // variant -> source file
    for (size_t i = 0; i < packages.size(); ++i) {
        for (const auto& variant : packages[i].getVariants()) {
            auto it = variantSource.find(variant);
            if (it != variantSource.end()) {
                if (skipDuplicates) {
                    printInfo("Warning: skipping duplicate variant '" + variant +
                              "' from '" + positional[i] + "' (already in '" +
                              it->second + "')");
                } else {
                    printError("Duplicate variant '" + variant + "' found in '" +
                               it->second + "' and '" + positional[i] +
                               "' (use --skip-duplicates to skip)");
                    return 1;
                }
            } else {
                variantSource[variant] = positional[i];
            }
        }
    }

    // Determine output path
    if (outputPath.empty()) {
        outputPath = refManifest.name.empty()
            ? "merged.lgx"
            : refManifest.name + ".lgx";
    }

    // Check if output exists
    if (std::filesystem::exists(outputPath) && !autoYes) {
        if (!confirm("Output file '" + outputPath + "' exists. Overwrite?", true)) {
            printInfo("Aborted.");
            return 1;
        }
    }

    // Create a fresh output package
    auto createResult = Package::create(outputPath, refManifest.name);
    if (!createResult.success) {
        printError("Failed to create output package: " + createResult.error);
        return 1;
    }

    auto mergedOpt = Package::load(outputPath);
    if (!mergedOpt) {
        printError("Failed to load created package: " + Package::getLastError());
        return 1;
    }
    Package& merged = *mergedOpt;

    // Copy metadata from the reference manifest
    Manifest& mergedManifest = merged.getManifest();
    mergedManifest.manifestVersion = refManifest.manifestVersion;
    mergedManifest.name = refManifest.name;
    mergedManifest.version = refManifest.version;
    mergedManifest.description = refManifest.description;
    mergedManifest.author = refManifest.author;
    mergedManifest.type = refManifest.type;
    mergedManifest.category = refManifest.category;
    mergedManifest.icon = refManifest.icon;
    mergedManifest.view = refManifest.view;
    mergedManifest.dependencies = refManifest.dependencies;

    // Create a unique temp directory for extracting variants
    auto tmpBase = std::filesystem::temp_directory_path() /
        ("lgx-merge-" + std::to_string(getpid()) + "-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(tmpBase);
    TempDir tmpGuard(tmpBase);

    // Extract each variant from each input and add it to the merged package
    std::set<std::string> addedVariants;
    for (size_t i = 0; i < packages.size(); ++i) {
        for (const auto& variant : packages[i].getVariants()) {
            // Skip duplicate variants (already added from an earlier package)
            if (addedVariants.count(variant)) continue;
            addedVariants.insert(variant);

            // Extract this variant's files to a temp directory
            auto extractDir = tmpBase / ("pkg" + std::to_string(i));
            std::error_code dirEc;
            std::filesystem::create_directories(extractDir, dirEc);
            if (dirEc) {
                printError("Failed to create temporary directory '" +
                           extractDir.string() + "': " + dirEc.message());
                return 1;
            }

            auto extractResult = packages[i].extractVariant(variant, extractDir);
            if (!extractResult.success) {
                printError("Failed to extract variant '" + variant + "' from '" +
                           positional[i] + "': " + extractResult.error);
                return 1;
            }

            // extractVariant puts files under extractDir/variant/
            auto variantDir = extractDir / variant;

            // Get the main entry for this variant
            auto mainEntry = packages[i].getManifest().getMain(variant);
            std::optional<std::string> mainOpt = mainEntry
                ? std::make_optional(*mainEntry)
                : std::nullopt;

            auto addResult = merged.addVariant(variant, variantDir, mainOpt);
            if (!addResult.success) {
                printError("Failed to add variant '" + variant + "': " + addResult.error);
                return 1;
            }
        }
    }

    // Save the merged package
    auto saveResult = merged.save(outputPath);
    if (!saveResult.success) {
        printError("Failed to save merged package: " + saveResult.error);
        return 1;
    }

    // Summary
    std::string variants;
    for (const auto& [variant, source] : variantSource) {
        if (!variants.empty()) variants += ", ";
        variants += variant;
    }

    printSuccess("Merged " + std::to_string(positional.size()) + " packages into " +
                 outputPath + " (" + variants + ")");
    return 0;
}

} // namespace lgx
