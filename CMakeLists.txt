cmake_minimum_required(VERSION 3.17)
project("Raster comparison")

set(CMAKE_CXX_STANDARD 17)

add_executable(rastcomp main.cc)

target_link_libraries(rastcomp PRIVATE gdal)
