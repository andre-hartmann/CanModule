cmake_minimum_required(VERSION 2.8)

include_directories (
${PROJECT_SOURCE_DIR}/CanModule/CanInterface/include
${PROJECT_SOURCE_DIR}/CanModule/CanInterfaceImplementations/sockcan
)
link_directories(
/usr/local/lib
)
SET( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/CanModule/CanInterfaceImplementations/output/)
add_library (SocketCanImplementation SHARED
SockCanScan.cpp
../../CanInterface/src/CanStatistics.cpp
../../CanInterface/src/CanModuleUtils.cpp
$<TARGET_OBJECTS:LogIt>
)

target_link_libraries ( SocketCanImplementation 
	${BOOST_LIBS}

	libsocketcan.a
)

add_definitions(-std=gnu++0x)
set_property(TARGET SocketCanImplementation PROPERTY POSITION_INDEPENDENT_CODE TRUE)

