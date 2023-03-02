cmake_minimum_required(VERSION 3.17)
project(gdal_cmp_color)

set(CMAKE_CXX_STANDARD 17)

add_executable(gdal_cmp_color main.cc)

target_link_libraries(gdal_cmp_color PRIVATE gdal)