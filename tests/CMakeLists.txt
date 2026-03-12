include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Copy test data
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/test_data
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

add_executable(laren_tests
    test_unicode.cpp
    test_rule_engine.cpp
    test_trie.cpp
    test_dictionary.cpp
    test_transliterator.cpp
    test_ranker.cpp
)

target_link_libraries(laren_tests PRIVATE
    laren_core
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(laren_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# Fcitx5 integration tests (requires TestFrontend module + setupTestingEnvironmentPath)
find_package(Fcitx5ModuleTestFrontend QUIET)
find_package(Fcitx5ModuleTestIM QUIET)
if(Fcitx5ModuleTestFrontend_FOUND AND Fcitx5ModuleTestIM_FOUND)
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_INCLUDES ${Fcitx5Core_INCLUDE_DIRS})
    set(CMAKE_REQUIRED_LIBRARIES Fcitx5::Core)
    set(CMAKE_REQUIRED_FLAGS "-std=c++20")
    check_cxx_source_compiles("
        #include <filesystem>
        #include <vector>
        #include <fcitx-utils/testing.h>
        int main() {
            std::filesystem::path p;
            std::vector<std::filesystem::path> v;
            fcitx::setupTestingEnvironmentPath(p, v, v);
        }
    " HAVE_SETUP_TESTING_ENV_PATH)
    if(HAVE_SETUP_TESTING_ENV_PATH)
        add_subdirectory(integration)
    else()
        message(STATUS "Fcitx5 too old (missing setupTestingEnvironmentPath), skipping integration tests")
    endif()
else()
    message(STATUS "Fcitx5 TestFrontend/TestIM not found, skipping integration tests")
endif()
