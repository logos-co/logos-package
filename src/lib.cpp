/**
 * @file lib.cpp
 * @brief LGX Package Format C API Implementation
 * 
 * Thin C wrapper around the C++ lgx::Package class.
 */

#include "lgx.h"
#include "core/package.h"
#include "core/manifest.h"

#include <string>
#include <vector>
#include <memory>
#include <cstring>

/* Thread-local error storage */
thread_local std::string g_last_error;

/* Helper to set error and return false */
static void set_error(const std::string& error) {
    g_last_error = error;
}

/* Helper to clear error */
static void clear_error() {
    g_last_error.clear();
}

/* Helper to copy std::string to C string (caller must free) */
static char* strdup_cpp(const std::string& str) {
    char* result = static_cast<char*>(malloc(str.length() + 1));
    if (result) {
        std::strcpy(result, str.c_str());
    }
    return result;
}

/* Helper to convert vector<string> to NULL-terminated C string array */
static const char** vector_to_array(const std::vector<std::string>& vec) {
    const char** result = static_cast<const char**>(malloc((vec.size() + 1) * sizeof(char*)));
    if (!result) return nullptr;
    
    for (size_t i = 0; i < vec.size(); ++i) {
        result[i] = strdup_cpp(vec[i]);
        if (!result[i]) {
            // Cleanup on allocation failure
            for (size_t j = 0; j < i; ++j) {
                free(const_cast<char*>(result[j]));
            }
            free(result);
            return nullptr;
        }
    }
    result[vec.size()] = nullptr;
    return result;
}

/* Helper to convert set<string> to NULL-terminated C string array */
static const char** set_to_array(const std::set<std::string>& s) {
    std::vector<std::string> vec(s.begin(), s.end());
    return vector_to_array(vec);
}

/* Package wrapper struct */
struct lgx_package_opaque {
    std::unique_ptr<lgx::Package> pkg;
    std::string name_cache;
    std::string version_cache;
    std::string description_cache;
    std::string icon_cache;
    std::string manifest_json_cache;
};

/* Package creation and loading */

LGX_EXPORT lgx_result_t lgx_create(const char* output_path, const char* name) {
    if (!output_path || !name) {
        set_error("Invalid arguments: output_path and name cannot be NULL");
        return {false, g_last_error.c_str()};
    }
    
    clear_error();
    auto result = lgx::Package::create(output_path, name);
    
    if (!result.success) {
        set_error(result.error);
        return {false, g_last_error.c_str()};
    }
    return {true, nullptr};
}

LGX_EXPORT lgx_package_t lgx_load(const char* path) {
    if (!path) {
        set_error("Invalid argument: path cannot be NULL");
        return nullptr;
    }
    
    clear_error();
    auto pkg_opt = lgx::Package::load(path);
    
    if (!pkg_opt) {
        set_error("Failed to load package: " + std::string(path));
        return nullptr;
    }
    
    auto wrapper = new lgx_package_opaque();
    wrapper->pkg = std::make_unique<lgx::Package>(std::move(*pkg_opt));
    return wrapper;
}

LGX_EXPORT lgx_result_t lgx_save(lgx_package_t pkg, const char* path) {
    if (!pkg || !path) {
        set_error("Invalid arguments: pkg and path cannot be NULL");
        return {false, g_last_error.c_str()};
    }
    
    clear_error();
    auto result = pkg->pkg->save(path);
    
    if (!result.success) {
        set_error(result.error);
        return {false, g_last_error.c_str()};
    }
    return {true, nullptr};
}

LGX_EXPORT lgx_verify_result_t lgx_verify(const char* path) {
    if (!path) {
        set_error("Invalid argument: path cannot be NULL");
        return {false, nullptr, nullptr};
    }
    
    clear_error();
    auto result = lgx::Package::verify(path);
    
    lgx_verify_result_t c_result;
    c_result.valid = result.valid;
    c_result.errors = result.errors.empty() ? nullptr : vector_to_array(result.errors);
    c_result.warnings = result.warnings.empty() ? nullptr : vector_to_array(result.warnings);
    
    return c_result;
}

/* Package manipulation */

LGX_EXPORT lgx_result_t lgx_add_variant(
    lgx_package_t pkg,
    const char* variant,
    const char* files_path,
    const char* main_path
) {
    if (!pkg || !variant || !files_path) {
        set_error("Invalid arguments: pkg, variant, and files_path cannot be NULL");
        return {false, g_last_error.c_str()};
    }
    
    clear_error();
    std::optional<std::string> main_opt = main_path ? std::optional<std::string>(main_path) : std::nullopt;
    auto result = pkg->pkg->addVariant(variant, files_path, main_opt);
    
    if (!result.success) {
        set_error(result.error);
        return {false, g_last_error.c_str()};
    }
    return {true, nullptr};
}

LGX_EXPORT lgx_result_t lgx_remove_variant(lgx_package_t pkg, const char* variant) {
    if (!pkg || !variant) {
        set_error("Invalid arguments: pkg and variant cannot be NULL");
        return {false, g_last_error.c_str()};
    }
    
    clear_error();
    auto result = pkg->pkg->removeVariant(variant);
    
    if (!result.success) {
        set_error(result.error);
        return {false, g_last_error.c_str()};
    }
    return {true, nullptr};
}

LGX_EXPORT lgx_result_t lgx_extract(lgx_package_t pkg, const char* variant, const char* output_dir) {
    if (!pkg || !output_dir) {
        set_error("Invalid arguments: pkg and output_dir cannot be NULL");
        return {false, g_last_error.c_str()};
    }
    
    clear_error();
    lgx::Package::Result result;
    
    if (variant) {
        result = pkg->pkg->extractVariant(variant, output_dir);
    } else {
        result = pkg->pkg->extractAll(output_dir);
    }
    
    if (!result.success) {
        set_error(result.error);
        return {false, g_last_error.c_str()};
    }
    return {true, nullptr};
}

LGX_EXPORT bool lgx_has_variant(lgx_package_t pkg, const char* variant) {
    if (!pkg || !variant) {
        set_error("Invalid arguments: pkg and variant cannot be NULL");
        return false;
    }
    
    clear_error();
    return pkg->pkg->hasVariant(variant);
}

LGX_EXPORT const char** lgx_get_variants(lgx_package_t pkg) {
    if (!pkg) {
        set_error("Invalid argument: pkg cannot be NULL");
        return nullptr;
    }
    
    clear_error();
    auto variants = pkg->pkg->getVariants();
    return set_to_array(variants);
}

/* Manifest access */

LGX_EXPORT const char* lgx_get_name(lgx_package_t pkg) {
    if (!pkg) {
        set_error("Invalid argument: pkg cannot be NULL");
        return nullptr;
    }
    
    clear_error();
    pkg->name_cache = pkg->pkg->getManifest().name;
    return pkg->name_cache.c_str();
}

LGX_EXPORT const char* lgx_get_version(lgx_package_t pkg) {
    if (!pkg) {
        set_error("Invalid argument: pkg cannot be NULL");
        return nullptr;
    }
    
    clear_error();
    pkg->version_cache = pkg->pkg->getManifest().version;
    return pkg->version_cache.c_str();
}

LGX_EXPORT lgx_result_t lgx_set_version(lgx_package_t pkg, const char* version) {
    if (!pkg || !version) {
        set_error("Invalid arguments: pkg and version cannot be NULL");
        return {false, g_last_error.c_str()};
    }
    
    clear_error();
    pkg->pkg->getManifest().version = version;
    return {true, nullptr};
}

LGX_EXPORT const char* lgx_get_description(lgx_package_t pkg) {
    if (!pkg) {
        set_error("Invalid argument: pkg cannot be NULL");
        return nullptr;
    }
    
    clear_error();
    pkg->description_cache = pkg->pkg->getManifest().description;
    return pkg->description_cache.c_str();
}

LGX_EXPORT void lgx_set_description(lgx_package_t pkg, const char* description) {
    if (!pkg || !description) {
        set_error("Invalid arguments: pkg and description cannot be NULL");
        return;
    }
    
    clear_error();
    pkg->pkg->getManifest().description = description;
}

LGX_EXPORT const char* lgx_get_icon(lgx_package_t pkg) {
    if (!pkg) {
        set_error("Invalid argument: pkg cannot be NULL");
        return nullptr;
    }

    clear_error();
    pkg->icon_cache = pkg->pkg->getManifest().icon;
    return pkg->icon_cache.c_str();
}

LGX_EXPORT void lgx_set_icon(lgx_package_t pkg, const char* icon) {
    if (!pkg || !icon) {
        set_error("Invalid arguments: pkg and icon cannot be NULL");
        return;
    }

    clear_error();
    pkg->pkg->getManifest().icon = icon;
}

LGX_EXPORT const char* lgx_get_manifest_json(lgx_package_t pkg) {
    if (!pkg) {
        set_error("Invalid argument: pkg cannot be NULL");
        return nullptr;
    }

    clear_error();
    pkg->manifest_json_cache = pkg->pkg->getManifest().toJson();
    return pkg->manifest_json_cache.c_str();
}

/* Memory management */

LGX_EXPORT void lgx_free_package(lgx_package_t pkg) {
    if (pkg) {
        delete pkg;
    }
}

LGX_EXPORT void lgx_free_string_array(const char** array) {
    if (!array) return;
    
    for (size_t i = 0; array[i] != nullptr; ++i) {
        free(const_cast<char*>(array[i]));
    }
    free(array);
}

LGX_EXPORT void lgx_free_verify_result(lgx_verify_result_t result) {
    if (result.errors) {
        lgx_free_string_array(result.errors);
    }
    if (result.warnings) {
        lgx_free_string_array(result.warnings);
    }
}

/* Error handling */

LGX_EXPORT const char* lgx_get_last_error(void) {
    return g_last_error.c_str();
}

/* Version info */

LGX_EXPORT const char* lgx_version(void) {
    return "0.1.0";
}
