cmake_minimum_required(VERSION 3.13)

project(recreate-git)

set(CMAKE_CXX_STANDARD 23) # Enable the C++23 standard

file(GLOB_RECURSE SOURCE_FILES src/*.cpp src/*.hpp)

find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(cpr REQUIRED)

add_executable(mygit ${SOURCE_FILES})

target_link_libraries(mygit PRIVATE OpenSSL::Crypto)
target_link_libraries(mygit PRIVATE ZLIB::ZLIB)
target_link_libraries(mygit PRIVATE cpr::cpr)