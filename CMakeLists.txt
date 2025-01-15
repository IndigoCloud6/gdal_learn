cmake_minimum_required(VERSION 3.30)
project(gdal_learn)

set(CMAKE_CXX_STANDARD 20)
find_package(GDAL REQUIRED)
find_package(Crow CONFIG REQUIRED)
find_package(GEOS REQUIRED)
find_package(rapidjson CONFIG REQUIRED)

add_definitions(-D_UNICODE -DUNICODE)

add_executable(gdal_learn main.cpp
        main.cpp
)
target_link_libraries(gdal_learn PRIVATE GDAL::GDAL)
target_link_libraries(gdal_learn PRIVATE Crow::Crow)
target_link_libraries(gdal_learn PRIVATE GEOS::geos GEOS::geos_c)
target_link_libraries(gdal_learn PRIVATE rapidjson)
