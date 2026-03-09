include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
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
