# LGX Tests

# Test executable
add_executable(lgx_tests
    test_path_normalizer.cpp
    test_gzip_handler.cpp
    test_tar_writer.cpp
    test_tar_reader.cpp
    test_manifest.cpp
    test_package.cpp
    test_cli.cpp
)

# Link with appropriate GTest targets (handles both find_package and FetchContent)
if(TARGET GTest::gtest)
    target_link_libraries(lgx_tests PRIVATE
        lgx_core
        GTest::gtest
        GTest::gtest_main
    )
else()
    target_link_libraries(lgx_tests PRIVATE
        lgx_core
        gtest
        gtest_main
    )
endif()

# Discover tests
gtest_discover_tests(lgx_tests)

# Library tests (only if building shared library)
if(LGX_BUILD_SHARED)
    add_executable(lgx_lib_tests
        test_lib.cpp
    )
    
    if(TARGET GTest::gtest)
        target_link_libraries(lgx_lib_tests PRIVATE
            lgx_shared
            GTest::gtest
            GTest::gtest_main
        )
    else()
        target_link_libraries(lgx_lib_tests PRIVATE
            lgx_shared
            gtest
            gtest_main
        )
    endif()
    
    gtest_discover_tests(lgx_lib_tests)
endif()
