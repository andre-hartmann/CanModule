cmake_minimum_required(VERSION 2.8)

IF (WIN32)
	SET( CAN_LIBRARY_LOADER_PLATFORM_IMPL CanLibLoader/src/CanLibLoaderWin.cpp)
	SET( CAN_LIBRARY_LOADER_PLATFORM_IMPL_HEADER CanLibLoader/include/CanLibLoaderWin.h)
	SET( CAN_LIBRARY_GETTIMEOFDAY CanInterface/src/gettimeofday.cpp)
	SET( CAN_LIBRARY_GETTIMEOFDAY_HEADER CanInterface/include/gettimeofday.h)
ELSE()
	SET( CAN_LIBRARY_LOADER_PLATFORM_IMPL CanLibLoader/src/CanLibLoaderLin.cpp)
	SET( CAN_LIBRARY_LOADER_PLATFORM_IMPL_HEADER CanLibLoader/include/CanLibLoaderLin.h)
	SET( CAN_LIBRARY_GETTIMEOFDAY )
	SET( CAN_LIBRARY_GETTIMEOFDAY_HEADER )
ENDIF()

add_subdirectory(CanInterfaceImplementations) #just to build the shared lib (reqd at runtime - loaded dynamically)
add_subdirectory(GoogleTest)
add_subdirectory(CanModuleTest)

#add_subdirectory(CanLibLoader)
#add_subdirectory(CanInterface)

add_library (CanModule OBJECT
		CanLibLoader/src/CanLibLoader.cpp
		CanLibLoader/include/CanLibLoader.h
		${CAN_LIBRARY_LOADER_PLATFORM_IMPL}
		${CAN_LIBRARY_LOADER_PLATFORM_IMPL_HEADER}
		CanInterface/src/CanLibExternCWrapper.cpp
		CanInterface/src/CanStatistics.cpp    
		CanInterface/src/CanModuleUtils.cpp
		CanInterface/src/MessageSharedQueue.cpp
		CanInterface/include/CanLibExternCWrapper.h
		CanInterface/include/CanStatistics.h 
		CanInterface/include/CanModuleUtils.h
		CanInterface/include/CanMessage.h
		CanInterface/include/CCanAccess.h
		CanInterface/include/MessageSharedQueue.h
		${CAN_LIBRARY_GETTIMEOFDAY}
		${CAN_LIBRARY_GETTIMEOFDAY_HEADER}
		)

#set_target_properties(CanModule PROPERTIES LINKER_LANGUAGE Cxx)
