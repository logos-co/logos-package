#pragma once

/**
 * @file lgx.h
 * @brief LGX Package Format Public API
 * 
 * This header provides the public API for working with LGX packages
 * programmatically.
 */

#include "../../src/core/package.h"
#include "../../src/core/manifest.h"
#include "../../src/core/path_normalizer.h"
#include "../../src/core/tar_writer.h"
#include "../../src/core/tar_reader.h"
#include "../../src/core/gzip_handler.h"

/**
 * @namespace lgx
 * @brief LGX Package Format namespace
 * 
 * The lgx namespace contains all types and functions for working with
 * LGX packages.
 * 
 * ## Quick Start
 * 
 * ### Creating a package
 * @code
 * auto result = lgx::Package::create("mymodule.lgx", "mymodule");
 * if (!result.success) {
 *     std::cerr << "Error: " << result.error << std::endl;
 * }
 * @endcode
 * 
 * ### Loading and modifying a package
 * @code
 * auto pkgOpt = lgx::Package::load("mymodule.lgx");
 * if (pkgOpt) {
 *     auto& pkg = *pkgOpt;
 *     pkg.addVariant("linux-amd64", "./build/lib.so", "lib.so");
 *     pkg.save("mymodule.lgx");
 * }
 * @endcode
 * 
 * ### Verifying a package
 * @code
 * auto result = lgx::Package::verify("mymodule.lgx");
 * if (!result.valid) {
 *     for (const auto& err : result.errors) {
 *         std::cerr << "Error: " << err << std::endl;
 *     }
 * }
 * @endcode
 */
