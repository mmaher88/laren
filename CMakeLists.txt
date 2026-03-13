cmake_minimum_required(VERSION 3.21)
project(laren VERSION 0.3.5 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(GNUInstallDirs)
find_package(Fcitx5Core REQUIRED)
find_package(Gettext REQUIRED)

include("${FCITX_INSTALL_CMAKECONFIG_DIR}/Fcitx5Utils/Fcitx5CompilerSettings.cmake")

# Override Fcitx5's C++17 default — we need C++20 for std::span etc.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DFCITX_GETTEXT_DOMAIN="fcitx5-laren")
fcitx5_add_i18n_definition()

option(BUILD_TESTS "Build unit tests" ON)

add_subdirectory(po)
add_subdirectory(src)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
