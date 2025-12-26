file(GLOB TEST_UTILS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/utils/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/utils/*.hpp")

if(TEST_UTILS_SOURCES AND NOT "${TEST_UTILS_SOURCES}" STREQUAL "")
    add_library(scheduling-module-test STATIC ${TEST_UTILS_SOURCES})
    target_link_libraries(scheduling-module-test
        LINK_PUBLIC scheduling-module
        LINK_PUBLIC GTest::gtest_main)

    target_include_directories(scheduling-module-test
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    )

    if(${SC_CLANG_FORMAT_CODE})
        target_clangformat_setup(scheduling-module-test)
    endif()

    set(TEST_DEPENDS sc-machine::sc-builder-lib scheduling-module-test)
else()
    set(TEST_DEPENDS sc-machine::sc-builder-lib scheduling-module)
endif()

make_tests_from_folder(${CMAKE_CURRENT_LIST_DIR}/units
    NAME scheduling-module-tests
    DEPENDS ${TEST_DEPENDS}
)

if(${SC_CLANG_FORMAT_CODE})
    target_clangformat_setup(scheduling-module-tests)
endif()
