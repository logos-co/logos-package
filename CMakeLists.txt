cmake_minimum_required(VERSION 3.16)
project(lgx VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find required packages
find_package(ZLIB REQUIRED)

# ICU configuration - check homebrew paths on macOS
if(APPLE)
    # Try to find ICU in homebrew keg-only location
    execute_process(
        COMMAND brew --prefix icu4c
        OUTPUT_VARIABLE ICU_BREW_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE ICU_BREW_RESULT
    )
    
    if(ICU_BREW_RESULT EQUAL 0 AND EXISTS "${ICU_BREW_PREFIX}")
        message(STATUS "Found ICU via Homebrew: ${ICU_BREW_PREFIX}")
        set(ICU_ROOT "${ICU_BREW_PREFIX}")
        set(ENV{PKG_CONFIG_PATH} "${ICU_BREW_PREFIX}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
    endif()
endif()

find_package(ICU REQUIRED COMPONENTS uc i18n)

# Try to find nlohmann_json first (for Nix and system installations)
# Fall back to FetchContent if not found
find_package(nlohmann_json 3.11.0 QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found, fetching from GitHub")
    include(FetchContent)
    FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
    )
    FetchContent_MakeAvailable(json)
endif()

# Core library
add_library(lgx_core STATIC
    src/core/path_normalizer.cpp
    src/core/gzip_handler.cpp
    src/core/tar_writer.cpp
    src/core/tar_reader.cpp
    src/core/manifest.cpp
    src/core/package.cpp
)

target_include_directories(lgx_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(lgx_core PUBLIC
    ZLIB::ZLIB
    ICU::uc
    ICU::i18n
    nlohmann_json::nlohmann_json
)

# Shared library (optional)
option(LGX_BUILD_SHARED "Build shared library" OFF)
if(LGX_BUILD_SHARED)
    add_library(lgx_shared SHARED
        src/lib.cpp
        src/core/path_normalizer.cpp
        src/core/gzip_handler.cpp
        src/core/tar_writer.cpp
        src/core/tar_reader.cpp
        src/core/manifest.cpp
        src/core/package.cpp
    )
    
    target_include_directories(lgx_shared PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    
    target_link_libraries(lgx_shared PUBLIC
        ZLIB::ZLIB
        ICU::uc
        ICU::i18n
        nlohmann_json::nlohmann_json
    )
    
    set_target_properties(lgx_shared PROPERTIES
        OUTPUT_NAME "lgx"
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
    
    target_compile_definitions(lgx_shared PRIVATE LGX_BUILD_SHARED)
    
    install(TARGETS lgx_shared
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
    )
    install(FILES src/lgx.h DESTINATION include)
endif()

# CLI executable
add_executable(lgx
    src/main.cpp
    src/commands/command.cpp
    src/commands/create_command.cpp
    src/commands/add_command.cpp
    src/commands/remove_command.cpp
    src/commands/verify_command.cpp
    src/commands/sign_command.cpp
    src/commands/publish_command.cpp
)

target_link_libraries(lgx PRIVATE lgx_core)

# Installation
install(TARGETS lgx RUNTIME DESTINATION bin)
install(TARGETS lgx_core ARCHIVE DESTINATION lib)

# Testing (optional)
option(LGX_BUILD_TESTS "Build tests" OFF)
if(LGX_BUILD_TESTS)
    # Try to find Google Test first (for Nix and system installations)
    # Fall back to FetchContent if not found
    find_package(GTest QUIET)
    if(NOT GTest_FOUND)
        message(STATUS "GTest not found, fetching from GitHub")
        include(FetchContent)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.14.0
        )
        # Prevent overriding the parent project's compiler/linker settings on Windows
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()
    
    enable_testing()
    include(GoogleTest)
    add_subdirectory(tests)
endif()
