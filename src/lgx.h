/**
 * @file lgx.h
 * @brief LGX Package Format C API
 * 
 * This header provides a C API for working with LGX packages.
 * It wraps the C++ implementation with a stable C interface for
 * cross-language interoperability.
 */

#ifndef LGX_H
#define LGX_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Platform-specific export macros */
#if defined(_WIN32) || defined(__CYGWIN__)
  #ifdef LGX_BUILD_SHARED
    #ifdef __GNUC__
      #define LGX_EXPORT __attribute__((dllexport))
    #else
      #define LGX_EXPORT __declspec(dllexport)
    #endif
  #else
    #ifdef __GNUC__
      #define LGX_EXPORT __attribute__((dllimport))
    #else
      #define LGX_EXPORT __declspec(dllimport)
    #endif
  #endif
#else
  #if __GNUC__ >= 4
    #define LGX_EXPORT __attribute__((visibility("default")))
  #else
    #define LGX_EXPORT
  #endif
#endif

/* Opaque handle types */
typedef struct lgx_package_opaque* lgx_package_t;

/* Result structures */
typedef struct {
    bool success;
    const char* error;  /* NULL if success, owned by library */
} lgx_result_t;

typedef struct {
    bool valid;
    const char** errors;   /* NULL-terminated array, owned by library */
    const char** warnings; /* NULL-terminated array, owned by library */
} lgx_verify_result_t;

/* Package creation and loading */

/**
 * Create a new skeleton LGX package.
 * 
 * @param output_path Path where the .lgx file will be created
 * @param name Package name (will be lowercased)
 * @return Result indicating success or failure
 */
LGX_EXPORT lgx_result_t lgx_create(const char* output_path, const char* name);

/**
 * Load an existing LGX package from file.
 * 
 * @param path Path to the .lgx file
 * @return Package handle, or NULL on error (check lgx_get_last_error())
 */
LGX_EXPORT lgx_package_t lgx_load(const char* path);

/**
 * Save a package to file.
 * 
 * @param pkg Package handle
 * @param path Path where the .lgx file will be written
 * @return Result indicating success or failure
 */
LGX_EXPORT lgx_result_t lgx_save(lgx_package_t pkg, const char* path);

/**
 * Verify a package file.
 * 
 * @param path Path to the .lgx file to verify
 * @return Verification result with errors and warnings
 */
LGX_EXPORT lgx_verify_result_t lgx_verify(const char* path);

/* Package manipulation */

/**
 * Add files to a variant in the package.
 * If the variant exists, it will be completely replaced.
 * 
 * @param pkg Package handle
 * @param variant Variant name (will be lowercased)
 * @param files_path Path to file or directory to add
 * @param main_path Relative path to main file (required for directories, can be NULL for single files)
 * @return Result indicating success or failure
 */
LGX_EXPORT lgx_result_t lgx_add_variant(
    lgx_package_t pkg,
    const char* variant,
    const char* files_path,
    const char* main_path
);

/**
 * Remove a variant from the package.
 * 
 * @param pkg Package handle
 * @param variant Variant name (case-insensitive)
 * @return Result indicating success or failure
 */
LGX_EXPORT lgx_result_t lgx_remove_variant(lgx_package_t pkg, const char* variant);

/**
 * Extract variant contents from a package to a directory.
 * 
 * @param pkg Package handle
 * @param variant Variant name (NULL to extract all variants)
 * @param output_dir Output directory path (variant contents go to output_dir/variant/)
 * @return Result indicating success or failure
 */
LGX_EXPORT lgx_result_t lgx_extract(lgx_package_t pkg, const char* variant, const char* output_dir);

/**
 * Check if a variant exists in the package.
 * 
 * @param pkg Package handle
 * @param variant Variant name
 * @return true if variant exists
 */
LGX_EXPORT bool lgx_has_variant(lgx_package_t pkg, const char* variant);

/**
 * Get list of variants in the package.
 * 
 * @param pkg Package handle
 * @return NULL-terminated array of variant names, owned by library.
 *         Free with lgx_free_string_array().
 */
LGX_EXPORT const char** lgx_get_variants(lgx_package_t pkg);

/* Manifest access */

/**
 * Get the package name from manifest.
 * 
 * @param pkg Package handle
 * @return Package name, owned by library (valid until package is freed)
 */
LGX_EXPORT const char* lgx_get_name(lgx_package_t pkg);

/**
 * Get the package version from manifest.
 * 
 * @param pkg Package handle
 * @return Package version, owned by library (valid until package is freed)
 */
LGX_EXPORT const char* lgx_get_version(lgx_package_t pkg);

/**
 * Set the package version in manifest.
 * 
 * @param pkg Package handle
 * @param version New version string
 * @return Result indicating success or failure
 */
LGX_EXPORT lgx_result_t lgx_set_version(lgx_package_t pkg, const char* version);

/**
 * Get the package description from manifest.
 * 
 * @param pkg Package handle
 * @return Package description, owned by library (valid until package is freed)
 */
LGX_EXPORT const char* lgx_get_description(lgx_package_t pkg);

/**
 * Set the package description in manifest.
 *
 * @param pkg Package handle
 * @param description New description string
 */
LGX_EXPORT void lgx_set_description(lgx_package_t pkg, const char* description);

/**
 * Get the package icon path from manifest.
 *
 * @param pkg Package handle
 * @return Package icon path, owned by library (valid until package is freed)
 */
LGX_EXPORT const char* lgx_get_icon(lgx_package_t pkg);

/**
 * Set the package icon path in manifest.
 *
 * @param pkg Package handle
 * @param icon New icon path string
 */
LGX_EXPORT void lgx_set_icon(lgx_package_t pkg, const char* icon);

/**
 * Get the full manifest as a JSON string.
 *
 * @param pkg Package handle
 * @return JSON string of the manifest, owned by library (valid until package is freed)
 */
LGX_EXPORT const char* lgx_get_manifest_json(lgx_package_t pkg);

/* Memory management */

/**
 * Free a package handle.
 * 
 * @param pkg Package handle to free
 */
LGX_EXPORT void lgx_free_package(lgx_package_t pkg);

/**
 * Free a string array returned by library functions.
 * 
 * @param array NULL-terminated string array
 */
LGX_EXPORT void lgx_free_string_array(const char** array);

/**
 * Free a verify result structure.
 * 
 * @param result Verify result to free
 */
LGX_EXPORT void lgx_free_verify_result(lgx_verify_result_t result);

/* Error handling */

/**
 * Get the last error message.
 * 
 * @return Error message string, owned by library (thread-local storage)
 */
LGX_EXPORT const char* lgx_get_last_error(void);

/* Version info */

/**
 * Get the library version string.
 * 
 * @return Version string (e.g., "0.1.0")
 */
LGX_EXPORT const char* lgx_version(void);

#ifdef __cplusplus
}
#endif

#endif /* LGX_H */
