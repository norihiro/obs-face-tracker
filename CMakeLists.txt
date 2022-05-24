cmake_minimum_required(VERSION 3.10)

project(obs-face-tracker)
set(CMAKE_PROJECT_VERSION 0.5.4) # TODO: update cmake to 3.12 or later and put to 'project'
set(PLUGIN_AUTHOR "Norihiro Kamae")
set(MACOS_BUNDLEID "net.nagater.obs-face-tracker")
set(LINUX_MAINTAINER_EMAIL "norihiro@nagater.net")

option(WITH_PTZ_TCP "Enable to connect PTZ camera through TCP socket" ON)
option(ENABLE_MONITOR_USER "Enable monitor source for user" OFF)
option(ENABLE_DEBUG_DATA "Enable property to save error and control data" OFF)
option(WITH_DOCK "Enable dock" ON)

set(CMAKE_PREFIX_PATH "${QTDIR}")
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (WIN32 OR APPLE)
include(external/FindLibObs.cmake)
endif()

find_package(LibObs REQUIRED)
find_package(Qt5 REQUIRED COMPONENTS Core Widgets Gui)
set(plugin_additional_libs)

if (WITH_PTZ_TCP)
	set(CMAKE_POSITION_INDEPENDENT_CODE true)
	set(WITH_VISCA_SERIAL OFF)
	add_definitions("-DVISCA_API= ")
	add_subdirectory(libvisca/visca)
	set(plugin_additional_libs ${plugin_additional_libs} visca)
	set(plugin_additional_incs ${plugin_additional_incs} libvisca/visca)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE True)
set(DLIB_NO_GUI_SUPPORT ON)
set(DLIB_PNG_SUPPORT OFF)
set(DLIB_GIF_SUPPORT OFF)
set(DLIB_JPEG_SUPPORT OFF)
set(DLIB_LINK_WITH_SQLITE3 OFF)
add_subdirectory(dlib)

configure_file(
    src/plugin-macros.h.in
    ../src/plugin-macros.generated.h
)
configure_file(
    installer/installer-Windows.iss.in
    ../installer/installer-Windows.generated.iss
)

configure_file(
    ci/ci_includes.sh.in
    ../ci/ci_includes.generated.sh
)
configure_file(
    ci/ci_includes.cmd.in
    ../ci/ci_includes.generated.cmd
)

set(PLUGIN_SOURCES
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
	src/helper.cpp
	src/ptz-backend.cpp
	src/obsptz-backend.cpp
	src/dummy-backend.cpp
)

set(PLUGIN_HEADERS
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

if (WITH_PTZ_TCP)
	set(PLUGIN_SOURCES ${PLUGIN_SOURCES} src/libvisca-thread.cpp)
	set(PLUGIN_HEADERS ${PLUGIN_HEADERS} src/libvisca-thread.hpp)
endif()

if (WITH_DOCK)
	set(PLUGIN_SOURCES
		${PLUGIN_SOURCES}
		ui/face-tracker-dock.cpp
		ui/face-tracker-widget.cpp
	)
	set(PLUGIN_HEADERS
		${PLUGIN_HEADERS}
		ui/face-tracker-dock.hpp
		ui/face-tracker-widget.hpp
	)
	set(plugin_additional_incs ${plugin_additional_incs} src)
	set(plugin_additional_libs ${plugin_additional_libs} Qt5::Gui Qt5::GuiPrivate)
	set(plugin_additional_incs ${plugin_additional_incs} ${Qt5Gui_PRIVATE_INCLUDE_DIRS})
endif()

# --- Platform-independent build settings ---
add_library(${CMAKE_PROJECT_NAME} MODULE ${PLUGIN_SOURCES} ${PLUGIN_HEADERS})

include_directories(
	"${LIBOBS_INCLUDE_DIR}/../UI/obs-frontend-api"
	${Qt5Core_INCLUDES}
	${Qt5Widgets_INCLUDES}
	${CMAKE_CURRENT_SOURCE_DIR}/dlib
	${plugin_additional_incs}
)

target_link_libraries(${CMAKE_PROJECT_NAME}
	libobs
	Qt5::Core
	Qt5::Widgets
	dlib
	${plugin_additional_libs}
)

# --- End of section ---

# --- Windows-specific build settings and tasks ---
if(WIN32)
	if(NOT DEFINED OBS_FRONTEND_LIB)
		set(OBS_FRONTEND_LIB "OBS_FRONTEND_LIB-NOTFOUND" CACHE FILEPATH "OBS frontend library")
		message(FATAL_ERROR "Could not find OBS Frontend API\'s library !")
	endif()

    # Enable Multicore Builds and disable FH4 (to not depend on VCRUNTIME140_1.DLL when building with VS2019)
    if (MSVC)
        add_definitions(/MP /d2FH4-)
        add_definitions("-D_USE_MATH_DEFINES")
        add_definitions("-D_CRT_SECURE_NO_WARNINGS") # to avoid a warning for `fopen`
    endif()

	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(ARCH_NAME "64bit")
		set(OBS_BUILDDIR_ARCH "build64")
	else()
		set(ARCH_NAME "32bit")
		set(OBS_BUILDDIR_ARCH "build32")
	endif()

	include_directories(
		"${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/UI"
	)

	target_link_libraries(${CMAKE_PROJECT_NAME}
		${PTHREAD_LIBS}
		"${OBS_FRONTEND_LIB}"
    )

	# --- Release package helper ---
	# The "release" folder has a structure similar OBS' one on Windows
	set(RELEASE_DIR "${PROJECT_SOURCE_DIR}/release")

    add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
        # If config is Release or RelWithDebInfo, package release files
        COMMAND if $<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>==1 (
            "${CMAKE_COMMAND}" -E make_directory
                "${RELEASE_DIR}/data/obs-plugins/${CMAKE_PROJECT_NAME}"
                "${RELEASE_DIR}/obs-plugins/${ARCH_NAME}"
        )

        COMMAND if $<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>==1 (
            "${CMAKE_COMMAND}" -E copy_directory
                "${PROJECT_SOURCE_DIR}/data"
                "${RELEASE_DIR}/data/obs-plugins/${CMAKE_PROJECT_NAME}"
        )

        COMMAND if $<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>==1 (
            "${CMAKE_COMMAND}" -E copy
                "$<TARGET_FILE:${CMAKE_PROJECT_NAME}>"
                "${RELEASE_DIR}/obs-plugins/${ARCH_NAME}"
        )

        # If config is RelWithDebInfo, copy the pdb file
        COMMAND if $<CONFIG:RelWithDebInfo>==1 (
            "${CMAKE_COMMAND}" -E copy
                "$<TARGET_PDB_FILE:${CMAKE_PROJECT_NAME}>"
                "${RELEASE_DIR}/obs-plugins/${ARCH_NAME}"
        )

        # Copy to obs-studio dev environment for immediate testing
        COMMAND if $<CONFIG:Debug>==1 (
            "${CMAKE_COMMAND}" -E copy
                "$<TARGET_FILE:${CMAKE_PROJECT_NAME}>"
                "${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/obs-plugins/${ARCH_NAME}"
        )

        COMMAND if $<CONFIG:Debug>==1 (
            "${CMAKE_COMMAND}" -E copy
                "$<TARGET_PDB_FILE:${CMAKE_PROJECT_NAME}>"
                "${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/obs-plugins/${ARCH_NAME}"
        )

        COMMAND if $<CONFIG:Debug>==1 (
            "${CMAKE_COMMAND}" -E make_directory
                "${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/data/obs-plugins/${CMAKE_PROJECT_NAME}"
        )

        COMMAND if $<CONFIG:Debug>==1 (
            "${CMAKE_COMMAND}" -E copy_directory
                "${PROJECT_SOURCE_DIR}/data"
                "${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/data/obs-plugins/${CMAKE_PROJECT_NAME}"
        )
    )
	# --- End of sub-section ---

endif()
# --- End of section ---

# --- Linux-specific build settings and tasks ---
if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)

	set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES PREFIX "")
	if(DEFINED LIBOBS_FRONTEND_API)
		target_link_libraries(${CMAKE_PROJECT_NAME} ${LIBOBS_FRONTEND_API})
	else()
		target_link_libraries(${CMAKE_PROJECT_NAME} obs-frontend-api)
	endif()

	install(TARGETS ${CMAKE_PROJECT_NAME}
		LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}/obs-plugins")

	install(DIRECTORY data/
		DESTINATION "${CMAKE_INSTALL_FULL_DATAROOTDIR}/obs/obs-plugins/${CMAKE_PROJECT_NAME}")
endif()
# --- End of section ---

# -- OS X specific build settings and tasks --
if(APPLE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -fvisibility=default")

	set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES PREFIX "")
	target_link_libraries(${CMAKE_PROJECT_NAME} "${OBS_FRONTEND_LIB}")
endif()
# -- End of section --
