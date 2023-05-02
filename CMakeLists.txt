cmake_minimum_required(VERSION 3.20)

project(ipmifan LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

find_package(Doxygen)

find_package(PkgConfig REQUIRED)
pkg_search_module(IPMI REQUIRED libipmimonitoring)

add_executable(${PROJECT_NAME} main.cpp ipmi.cpp lm.cpp)
target_link_libraries(${PROJECT_NAME} ${IPMI_LINK_LIBRARIES} ${IPMIPOSIX_LINK_LIBRARIES} sensors)
target_include_directories(${PROJECT_NAME} PRIVATE ${IPMI_INCLUDE_DIRS} ${IPMIPOSIX_INCLUDE_DIRS})
target_compile_options(${PROJECT_NAME} PRIVATE "-Wall" "-O2" "-g" "-fno-strict-aliasing" "-Wfatal-errors" "-Wno-reorder")
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

