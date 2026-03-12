configure_file(testdir.h.in ${CMAKE_CURRENT_BINARY_DIR}/testdir.h @ONLY)
add_subdirectory(addon)
add_subdirectory(inputmethod)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(testlaren testlaren.cpp)
target_link_libraries(testlaren Fcitx5::Core Fcitx5::Module::TestFrontend)
add_dependencies(testlaren laren copy-addon copy-im)
add_test(NAME integration-testlaren COMMAND testlaren)
