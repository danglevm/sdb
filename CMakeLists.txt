cmake_minimum_required(VERSION 3.19)

project("sdb" LANGUAGES CXX ASM) # enable c++ support

# these has to be before add_subdirectory because they need to be found before the add_subdirectory usage
find_package(PkgConfig MODULE REQUIRED) # find the PkgConfig module, then use pkg_check_modules function to locate readline module
pkg_check_modules(readline REQUIRED IMPORTED_TARGET readline)
find_package(fmt CONFIG REQUIRED)

include(CTest)

add_subdirectory("src")
add_subdirectory("tools")

if(BUILD_TESTING)
    find_package(Catch2 CONFIG REQUIRED) # find CMake config file provided by the package itself, Catch2 in this case
    add_subdirectory("test")
endif()