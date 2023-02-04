cmake_minimum_required(VERSION 3.15.0)
project(slang-tool)
include(ExternalProject)
include(GNUInstallDirs)

# Boost
set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.68.0 REQUIRED COMPONENTS
             unit_test_framework)

# Python
find_package(Python3 REQUIRED COMPONENTS
             Interpreter)

# Options
set(NUM_PARALLEL_JOBS 8)

# Compiler
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_options(-Wall -pedantic)

include(FetchContent)
FetchContent_Declare(
  slang
  GIT_REPOSITORY https://github.com/MikePopoloski/slang.git
  GIT_SHALLOW ON
  GIT_COMMIT a695ad2522e874e0593737402f24e8cc08278d16)
FetchContent_MakeAvailable(slang)

add_executable(main main.cpp)
target_link_libraries(main PRIVATE slang::slang fmt::fmt)

install(TARGETS main DESTINATION ${CMAKE_INSTALL_BINDIR})

enable_testing()
add_subdirectory(tests)
