cmake_minimum_required(VERSION 3.16)
project(vcs LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
add_executable(vcs
    main.cpp
    src/app.cpp
    src/repository.cpp
    src/commands/init.cpp
    src/commands/staging.cpp
    src/commands/history.cpp
    src/commands/checkout.cpp
    src/commands/restore.cpp
)
target_include_directories(vcs PRIVATE include)
target_link_libraries(vcs PRIVATE OpenSSL::Crypto ZLIB::ZLIB)
