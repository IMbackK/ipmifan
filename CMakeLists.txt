cmake_minimum_required(VERSION 3.20)

project(ipmifan LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "..." FORCE)
endif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

find_package(PkgConfig REQUIRED)
pkg_search_module(IPMI_MONITORING REQUIRED libipmimonitoring)
pkg_search_module(IPMI REQUIRED libfreeipmi)
pkg_search_module(SYSTEMD systemd)


add_executable(${PROJECT_NAME} main.cpp ipmi.cpp lm.cpp)
target_link_libraries(${PROJECT_NAME} ${IPMI_LINK_LIBRARIES} ${IPMI_MONITORING_LINK_LIBRARIES} sensors)
target_include_directories(${PROJECT_NAME} PRIVATE ${IPMI_INCLUDE_DIRS} ${IPMI_MONITORING_INCLUDE_DIRS})
target_compile_options(${PROJECT_NAME} PRIVATE "-Wall" "-O2" "-g" "-fno-strict-aliasing" "-Wfatal-errors" "-Wno-reorder")
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

if(SYSTEMD_FOUND)
	pkg_get_variable(SYSTEMD_UNIT_DIR_PKG systemd systemd_system_unit_path)
	message(STATUS ${SYSTEMD_UNIT_DIR_PKG})
	string(REPLACE ":" ";" SYSTEMD_UNIT_DIR_LIST ${SYSTEMD_UNIT_DIR_PKG})
	list(GET SYSTEMD_UNIT_DIR_LIST 0 SYSTEMD_UNIT_DIR)

	install(FILES ipmifan.service DESTINATION ${SYSTEMD_UNIT_DIR})
endif(SYSTEMD_FOUND)
