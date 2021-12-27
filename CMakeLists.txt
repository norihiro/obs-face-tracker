cmake_minimum_required(VERSION 3.16)

project(obs-face-tracker VERSION 0.5.3)
add_library(${CMAKE_PROJECT_NAME} MODULE)

set(PLUGIN_AUTHOR "Norihiro Kamae")
set(MACOS_BUNDLEID "net.nagater.obs-face-tracker")
set(LINUX_MAINTAINER_EMAIL "norihiro@nagater.net")

option(WITH_PTZ_TCP "Enable to connect PTZ camera through TCP socket" ON)
option(ENABLE_MONITOR_USER "Enable monitor source for user" OFF)

# Add your custom source files here - header files are optional and only required for visibility
# e.g. in Xcode or Visual Studio
target_sources(${CMAKE_PROJECT_NAME}
	PRIVATE
	src/module-main.c
	src/face-tracker.cpp
	src/source_list.cc
	src/face-tracker-preset.cpp
	src/face-tracker-manager.cpp
	src/face-tracker-ptz.cpp
	src/face-tracker-monitor.cpp
	src/face-detector-base.cpp
	src/face-detector-dlib.cpp
	src/face-tracker-base.cpp
	src/face-tracker-dlib.cpp
	src/texture-object.cpp
	src/ptz-backend.cpp
	src/obsptz-backend.cpp
	src/dummy-backend.cpp
)

target_sources(${CMAKE_PROJECT_NAME}
	PRIVATE
	src/face-tracker.hpp
	src/source_list.h
	src/face-tracker-preset.h
	src/face-tracker-manager.hpp
	src/face-tracker-ptz.hpp
	src/plugin-macros.generated.h
	src/face-detector-base.h
	src/face-detector-dlib.h
	src/face-tracker-base.h
	src/face-tracker-dlib.h
	src/texture-object.h
	src/helper.hpp
	src/ptz-backend.hpp
	src/obsptz-backend.hpp
	src/dummy-backend.hpp
)


find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)
include(external/ObsPluginHelpers.cmake)
find_package(Qt5Core REQUIRED)
find_package(Qt5Widgets REQUIRED)

if (WITH_PTZ_TCP)
	set(CMAKE_POSITION_INDEPENDENT_CODE true)
	set(WITH_VISCA_SERIAL OFF)
	add_definitions("-DVISCA_API= ")
	add_subdirectory(libvisca/visca)
	target_link_libraries(${CMAKE_PROJECT_NAME} visca)
	target_include_directories(${CMAKE_PROJECT_NAME} libvisca/visca)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE True)
set(DLIB_NO_GUI_SUPPORT ON)
set(DLIB_PNG_SUPPORT OFF)
set(DLIB_GIF_SUPPORT OFF)
set(DLIB_JPEG_SUPPORT OFF)
set(DLIB_LINK_WITH_SQLITE3 OFF)
add_subdirectory(dlib)

if (WITH_PTZ_TCP)
	target_sources(${CMAKE_PROJECT_NAME}
		PRIVATE
		src/libvisca-thread.cpp
		src/libvisca-thread.hpp)
endif()

configure_file(
	src/plugin-macros.h.in
	${CMAKE_SOURCE_DIR}/src/plugin-macros.generated.h)

target_sources(${CMAKE_PROJECT_NAME}
	PRIVATE
		src/plugin-macros.generated.h)

# --- Platform-independent build settings ---

target_include_directories(${CMAKE_PROJECT_NAME}
	PRIVATE ${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_SOURCE_DIR}/dlib
)

target_link_libraries(${CMAKE_PROJECT_NAME}
	PRIVATE
		OBS::libobs
		OBS::obs-frontend-api
		Qt5::Core
		Qt5::Widgets)

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
	AUTOMOC ON
	AUTOUIC ON
	AUTORCC ON)

target_compile_features(${CMAKE_PROJECT_NAME}
	PRIVATE
		cxx_std_17)

# --- End of section ---

# --- Windows-specific build settings and tasks ---
if(OS_WINDOWS)
	configure_file(
		installer/installer-Windows.iss.in
		${CMAKE_SOURCE_DIR}/installer/installer-Windows.generated.iss)

	configure_file(
		CI/include/build_environment.ps1.in
		${CMAKE_SOURCE_DIR}/CI/include/build_environment.ps1)

	if(MSVC)
		target_compile_options(${CMAKE_PROJECT_NAME}
			PRIVATE
				/MP
				/d2FH4-)
	endif()

	target_link_libraries(${CMAKE_PROJECT_NAME} ${PTHREAD_LIBS})
# --- End of section ---

# -- macOS specific build settings and tasks --
elseif(OS_MACOS)
	configure_file(
		bundle/installer-macOS.pkgproj.in
		${CMAKE_SOURCE_DIR}/bundle/installer-macOS.generated.pkgproj)

	configure_file(
		CI/include/build_environment.sh.in
		${CMAKE_SOURCE_DIR}/CI/include/build_environment.sh
	)

	set(MACOSX_PLUGIN_GUI_IDENTIFIER "${MACOS_BUNDLEID}")
	set(MACOSX_PLUGIN_BUNDLE_VERSION "${CMAKE_PROJECT_VERSION}")
	set(MACOSX_PLUGIN_SHORT_VERSION_STRING "1")

	target_compile_options(${CMAKE_PROJECT_NAME}
		PRIVATE
			-Wall
			-Wextra
			-Werror-implicit-function-declaration
			-stdlib=libc++
			-fvisibility=default)

	set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES PREFIX "")
# --- End of section ---

# --- Linux-specific build settings and tasks ---
else()
	configure_file(
		CI/include/build_environment.sh.in
		${CMAKE_SOURCE_DIR}/CI/include/build_environment.sh
	)

	target_compile_options(${CMAKE_PROJECT_NAME}
		PRIVATE
			-Wall
			-Wextra)

	set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES PREFIX "")
endif()
# --- End of section ---

setup_plugin_target(${CMAKE_PROJECT_NAME})
