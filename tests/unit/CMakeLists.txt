add_executable(UnitTests
               main.cpp
               foo.cpp)
target_link_libraries(UnitTests
                      ${Boost_LIBRARIES})

target_include_directories(UnitTests PUBLIC
                           ${CMAKE_BINARY_DIR})

add_test(NAME UnitTests
         COMMAND ./UnitTests)
