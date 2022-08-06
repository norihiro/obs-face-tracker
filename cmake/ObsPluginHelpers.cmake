if(POLICY CMP0087)
  cmake_policy(SET CMP0087 NEW)
endif()

set(OBS_STANDALONE_PLUGIN_DIR ${CMAKE_SOURCE_DIR}/release)
set(INCLUDED_LIBOBS_CMAKE_MODULES ON)

include(GNUInstallDirs)
if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
	set(OS_MACOS ON)
	set(OS_POSIX ON)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux|FreeBSD|OpenBSD")
	set(OS_POSIX ON)
	string(TOUPPER "${CMAKE_SYSTEM_NAME}" _SYSTEM_NAME_U)
	set(OS_${_SYSTEM_NAME_U} ON)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	set(OS_WINDOWS ON)
	set(OS_POSIX OFF)
endif()

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT AND (OS_WINDOWS OR OS_MACOS))
	set(CMAKE_INSTALL_PREFIX
		${OBS_STANDALONE_PLUGIN_DIR}
		CACHE STRING "Directory to install OBS plugin after building" FORCE)
endif()

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE
		"RelWithDebInfo"
		CACHE STRING
		"OBS build type [Release, RelWithDebInfo, Debug, MinSizeRel]" FORCE)
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Release RelWithDebInfo
		Debug MinSizeRel)
endif()

if(NOT QT_VERSION)
	set(QT_VERSION
		"5"
		CACHE STRING "OBS Qt version [5, 6]" FORCE)
	set_property(CACHE QT_VERSION PROPERTY STRINGS 5 6)
endif()

macro(find_qt)
	set(oneValueArgs VERSION)
	set(multiValueArgs COMPONENTS COMPONENTS_WIN COMPONENTS_MAC COMPONENTS_LINUX)
	cmake_parse_arguments(FIND_QT "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

	if(OS_WINDOWS)
		find_package(
			Qt${FIND_QT_VERSION}
			COMPONENTS ${FIND_QT_COMPONENTS} ${FIND_QT_COMPONENTS_WIN}
			REQUIRED)
	elseif(OS_MACOS)
		find_package(
			Qt${FIND_QT_VERSION}
			COMPONENTS ${FIND_QT_COMPONENTS} ${FIND_QT_COMPONENTS_MAC}
			REQUIRED)
	else()
		find_package(
			Qt${FIND_QT_VERSION}
			COMPONENTS ${FIND_QT_COMPONENTS} ${FIND_QT_COMPONENTS_LINUX}
			REQUIRED)
	endif()

	if("Gui" IN_LIST FIND_QT_COMPONENTS)
		list(APPEND FIND_QT_COMPONENTS "GuiPrivate")
	endif()

	foreach(_COMPONENT IN LISTS FIND_QT_COMPONENTS FIND_QT_COMPONENTS_WIN
			FIND_QT_COMPONENTS_MAC FIND_QT_COMPONENTS_LINUX)
		if(NOT TARGET Qt::${_COMPONENT} AND TARGET
				Qt${FIND_QT_VERSION}::${_COMPONENT})

			add_library(Qt::${_COMPONENT} INTERFACE IMPORTED)
			set_target_properties(
				Qt::${_COMPONENT} PROPERTIES INTERFACE_LINK_LIBRARIES
				"Qt${FIND_QT_VERSION}::${_COMPONENT}")
		endif()
	endforeach()
endmacro()

file(RELATIVE_PATH RELATIVE_INSTALL_PATH ${CMAKE_SOURCE_DIR} ${CMAKE_INSTALL_PREFIX})
file(RELATIVE_PATH RELATIVE_BUILD_PATH ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})

if(OS_MACOS)
	set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "OBS build architecture for macOS - x86_64 required at least")
	set_property(CACHE CMAKE_OSX_ARCHITECTURES PROPERTY STRINGS x86_64 arm64 "x86_64;arm64")

	set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "OBS deployment target for macOS - 10.13+ required")
	set_property(CACHE CMAKE_OSX_DEPLOYMENT_TARGET PROPERTY STRINGS 10.15 11 12)

	set(OBS_BUNDLE_CODESIGN_IDENTITY "-" CACHE STRING "OBS code signing identity for macOS")
	set(OBS_CODESIGN_LINKER ON
		CACHE BOOL "Enable linker code-signing on macOS (macOS 11+ required)")

	if(XCODE)
		# Tell Xcode to pretend the linker signed binaries so that editing with
		# install_name_tool preserves ad-hoc signatures. This option is supported by
		# codesign on macOS 11 or higher. See CMake Issue 21854:
		# https://gitlab.kitware.com/cmake/cmake/-/issues/21854

		set(CMAKE_XCODE_GENERATE_SCHEME ON)
	endif()

	# Set default options for bundling on macOS
	set(CMAKE_MACOSX_RPATH ON)
	set(CMAKE_SKIP_BUILD_RPATH OFF)
	set(CMAKE_BUILD_WITH_INSTALL_RPATH OFF)
	set(CMAKE_INSTALL_RPATH "@executable_path/../Frameworks/")
	set(CMAKE_INSTALL_RPATH_USE_LINK_PATH OFF)

	function(setup_plugin_target target)

		install(
			TARGETS ${target}
			LIBRARY DESTINATION "${target}/bin/"
			COMPONENT obs_plugins
			NAMELINK_COMPONENT ${target}_Development)

		if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/data)
			install(
				DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data/
				DESTINATION "${target}/data/"
				USE_SOURCE_PERMISSIONS
				COMPONENT obs_plugins)
		endif()

	endfunction()

else()
	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(_ARCH_SUFFIX 64)
	else()
		set(_ARCH_SUFFIX 32)
	endif()
	set(OBS_OUTPUT_DIR ${CMAKE_BINARY_DIR}/rundir)

	if(OS_POSIX)
		option(LINUX_PORTABLE "Build portable version (Linux)" ON)
		if(NOT LINUX_PORTABLE)
			set(OBS_LIBRARY_DESTINATION ${CMAKE_INSTALL_LIBDIR})
			set(OBS_PLUGIN_DESTINATION ${OBS_LIBRARY_DESTINATION}/obs-plugins)
			set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)
			set(OBS_DATA_DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/obs)
		else()
			set(OBS_LIBRARY_DESTINATION bin/${_ARCH_SUFFIX}bit)
			set(OBS_PLUGIN_DESTINATION obs-plugins/${_ARCH_SUFFIX}bit)
			set(CMAKE_INSTALL_RPATH
				"$ORIGIN/" "${CMAKE_INSTALL_PREFIX}/${OBS_LIBRARY_DESTINATION}")
			set(OBS_DATA_DESTINATION "data")
		endif()

		if(OS_LINUX)
			set(CPACK_PACKAGE_NAME "${CMAKE_PROJECT_NAME}")
			set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${LINUX_MAINTAINER_EMAIL}")
			set(CPACK_PACKAGE_VERSION "${CMAKE_PROJECT_VERSION}")
			option(PKG_SUFFIX "Suffix of package name" "-linux-x86_64")
			set(CPACK_PACKAGE_FILE_NAME
				"${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}${PKG_SUFFIX}")

			set(CPACK_GENERATOR "DEB")

			if(NOT LINUX_PORTABLE)
				set(CPACK_SET_DESTDIR ON)
			endif()
			include(CPack)
		endif()
	else()
		set(OBS_LIBRARY_DESTINATION "bin/${_ARCH_SUFFIX}bit")
		set(OBS_LIBRARY32_DESTINATION "bin/32bit")
		set(OBS_LIBRARY64_DESTINATION "bin/64bit")
		set(OBS_PLUGIN_DESTINATION "obs-plugins/${_ARCH_SUFFIX}bit")
		set(OBS_PLUGIN32_DESTINATION "obs-plugins/32bit")
		set(OBS_PLUGIN64_DESTINATION "obs-plugins/64bit")

		set(OBS_DATA_DESTINATION "data")
	endif()

	function(setup_plugin_target target)
		set_target_properties(${target} PROPERTIES PREFIX "")

		install(
			TARGETS ${target}
			RUNTIME DESTINATION "${OBS_PLUGIN_DESTINATION}"
			COMPONENT ${target}_Runtime
			LIBRARY DESTINATION "${OBS_PLUGIN_DESTINATION}"
			COMPONENT ${target}_Runtime
			NAMELINK_COMPONENT ${target}_Development)

		install(
			FILES $<TARGET_FILE:${target}>
			DESTINATION $<CONFIG>/${OBS_PLUGIN_DESTINATION}
			COMPONENT obs_rundir
			EXCLUDE_FROM_ALL)

		if(OS_WINDOWS)
			install(
				FILES $<TARGET_PDB_FILE:${target}>
				CONFIGURATIONS "RelWithDebInfo" "Debug"
				DESTINATION ${OBS_PLUGIN_DESTINATION}
				COMPONENT ${target}_Runtime
				OPTIONAL)

			install(
				FILES $<TARGET_PDB_FILE:${target}>
				CONFIGURATIONS "RelWithDebInfo" "Debug"
				DESTINATION $<CONFIG>/${OBS_PLUGIN_DESTINATION}
				COMPONENT obs_rundir
				OPTIONAL EXCLUDE_FROM_ALL)
		endif()

		if(MSVC)
			target_link_options(
				${target}
				PRIVATE
				"LINKER:/OPT:REF"
				"$<$<NOT:$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>>:LINKER\:/SAFESEH\:NO>"
				"$<$<CONFIG:DEBUG>:LINKER\:/INCREMENTAL:NO>"
				"$<$<CONFIG:RELWITHDEBINFO>:LINKER\:/INCREMENTAL:NO>")
		endif()

		setup_target_resources(${target} obs-plugins/${target})

		if(OS_WINDOWS)
			add_custom_command(
				TARGET ${target}
				POST_BUILD
				COMMAND
				"${CMAKE_COMMAND}" -DCMAKE_INSTALL_PREFIX=${OBS_OUTPUT_DIR}
				-DCMAKE_INSTALL_COMPONENT=obs_rundir
				-DCMAKE_INSTALL_CONFIG_NAME=$<CONFIG> -P
				${CMAKE_CURRENT_BINARY_DIR}/cmake_install.cmake
				COMMENT "Installing to plugin rundir"
				VERBATIM)
		endif()
	endfunction()

	function(setup_target_resources target destination)
		if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/data)
			install(
				DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data/
				DESTINATION ${OBS_DATA_DESTINATION}/${destination}
				USE_SOURCE_PERMISSIONS
				COMPONENT obs_plugins)

			install(
				DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data
				DESTINATION $<CONFIG>/${OBS_DATA_DESTINATION}/${destination}
				USE_SOURCE_PERMISSIONS
				COMPONENT obs_rundir
				EXCLUDE_FROM_ALL)
		endif()
	endfunction()
endif()
