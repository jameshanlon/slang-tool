cmake_minimum_required(VERSION 3.16.3)
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
set(NUM_PARALLEL_JOBS 16)

# Compiler
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_options(-Wall -pedantic)

# Setup slang
ExternalProject_Add(slang
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/slang
  PREFIX slang
  CONFIGURE_COMMAND cmake <SOURCE_DIR> -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
  BUILD_COMMAND make -j ${NUM_PARALLEL_JOBS}
  BUILD_ALWAYS 1
  INSTALL_DIR ${CMAKE_INSTALL_PREFIX}
  INSTALL_COMMAND make install)

add_library(libslangcompiler STATIC IMPORTED GLOBAL)
add_library(libslangcore STATIC IMPORTED GLOBAL)
add_library(libslangparser STATIC IMPORTED GLOBAL)
add_library(libslangruntime STATIC IMPORTED GLOBAL)

set_target_properties(libslangcompiler PROPERTIES
  IMPORTED_LOCATION ${CMAKE_INSTALL_PREFIX}/lib/libslangcompiler.a)

set_target_properties(libslangcore PROPERTIES
  IMPORTED_LOCATION ${CMAKE_INSTALL_PREFIX}/lib/libslangcore.a)

set_target_properties(libslangparser PROPERTIES
  IMPORTED_LOCATION ${CMAKE_INSTALL_PREFIX}/lib/libslangparser.a)

set_target_properties(libslangruntime PROPERTIES
  IMPORTED_LOCATION ${CMAKE_INSTALL_PREFIX}/lib/libslangruntime.a)

include_directories(${CMAKE_INSTALL_PREFIX}/include
                    ${Boost_INCLUDE_DIRS})

# Main
add_executable(main main.cpp)
add_dependencies(main slang)
target_link_libraries(main
  libslangcompiler
  libslangcore
  libslangparser
  libslangruntime
  ${Boost_LIBRARIES})

install(TARGETS main DESTINATION ${CMAKE_INSTALL_BINDIR})

enable_testing()
add_subdirectory(tests)
