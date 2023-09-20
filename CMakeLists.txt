cmake_minimum_required(VERSION 3.17)
project("Raster comparison")

set(CMAKE_CXX_STANDARD 17)

add_executable(rastcomp main.cc)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    message("Also adding 'stdc++fs' for linking since the g++ version ${CMAKE_CXX_COMPILER_VERSION} requires it.")
    target_link_libraries(rastcomp PRIVATE gdal stdc++fs)
else ()
    target_link_libraries(rastcomp PRIVATE gdal)
endif()
