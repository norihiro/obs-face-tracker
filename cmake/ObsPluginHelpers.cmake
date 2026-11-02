if(POLICY CMP0087)
  cmake_policy(SET CMP0087 NEW)
endif()

set(OBS_STANDALONE_PLUGIN_DIR ${CMAKE_SOURCE_DIR}/release)

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
		CACHE STRING "OBS build type [Release, RelWithDebInfo, Debug, MinSizeRel]" FORCE)
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Release RelWithDebInfo Debug MinSizeRel)
endif()

file(RELATIVE_PATH RELATIVE_INSTALL_PATH ${CMAKE_SOURCE_DIR} ${CMAKE_INSTALL_PREFIX})
file(RELATIVE_PATH RELATIVE_BUILD_PATH ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_ARCH_SUFFIX 64)
else()
	set(_ARCH_SUFFIX 32)
endif()

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
		if(NOT DEFINED MACOSX_PLUGIN_GUI_IDENTIFIER)
			message(
				FATAL_ERROR
				"No 'MACOSX_PLUGIN_GUI_IDENTIFIER' set, but is required to build plugin bundles on macOS - example: 'com.yourname.pluginname'"
				)
		endif()

		if(NOT DEFINED MACOSX_PLUGIN_BUNDLE_VERSION)
			message(
				FATAL_ERROR
				"No 'MACOSX_PLUGIN_BUNDLE_VERSION' set, but is required to build plugin bundles on macOS - example: '25'"
				)
		endif()

		if(NOT DEFINED MACOSX_PLUGIN_SHORT_VERSION_STRING)
			message(
				FATAL_ERROR
				"No 'MACOSX_PLUGIN_SHORT_VERSION_STRING' set, but is required to build plugin bundles on macOS - example: '1.0.2'"
				)
		endif()

		set(MACOSX_PLUGIN_BUNDLE_NAME "${target}" PARENT_SCOPE)
		set(MACOSX_PLUGIN_BUNDLE_VERSION "${MACOSX_BUNDLE_BUNDLE_VERSION}" PARENT_SCOPE)
		set(MACOSX_PLUGIN_SHORT_VERSION_STRING "${MACOSX_BUNDLE_SHORT_VERSION_STRING}" PARENT_SCOPE)
		set(MACOSX_PLUGIN_EXECUTABLE_NAME "${target}" PARENT_SCOPE)

		install(
			TARGETS ${target}
			LIBRARY DESTINATION "."
			COMPONENT obs_plugins
			NAMELINK_COMPONENT ${target}_Development)

		set_target_properties(
			${target}
			PROPERTIES
			BUNDLE ON
			BUNDLE_EXTENSION "plugin"
			OUTPUT_NAME ${target}
			MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/bundle/macOS/Plugin-Info.plist.in"
			XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${MACOSX_PLUGIN_GUI_IDENTIFIER}"
			XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "${OBS_BUNDLE_CODESIGN_IDENTITY}"
			XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/bundle/macOS/entitlements.plist")

		install_bundle_resources(${target})

		set(FIRST_DIR_SUFFIX ".plugin" PARENT_SCOPE)
	endfunction()

	function(install_bundle_resources target)
		if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/data)
			file(GLOB_RECURSE _DATA_FILES "${CMAKE_CURRENT_SOURCE_DIR}/data/*")
			foreach(_DATA_FILE IN LISTS _DATA_FILES)
				file(RELATIVE_PATH _RELATIVE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/data/
					${_DATA_FILE})
				get_filename_component(_RELATIVE_PATH ${_RELATIVE_PATH} PATH)
				target_sources(${target} PRIVATE ${_DATA_FILE})
				set_source_files_properties(
					${_DATA_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION
					Resources/${_RELATIVE_PATH})
				string(REPLACE "\\" "\\\\" _GROUP_NAME "${_RELATIVE_PATH}")
				source_group("Resources\\${_GROUP_NAME}" FILES ${_DATA_FILE})
			endforeach()
		endif()
	endfunction()

elseif(OS_POSIX)
	option(LINUX_PORTABLE "Build portable version (Linux)" OFF)

	if(NOT CCACHE_SET)
		find_program(CCACHE_PROGRAM "ccache")
		set(CCACHE_SUPPORT
			ON
			CACHE BOOL "Enable ccache support")
		mark_as_advanced(CCACHE_PROGRAM)
		if(CCACHE_PROGRAM AND CCACHE_SUPPORT)
			set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM} CACHE INTERNAL "")
			set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM} CACHE INTERNAL "")
			set(CMAKE_OBJC_COMPILER_LAUNCHER ${CCACHE_PROGRAM} CACHE INTERNAL "")
			set(CMAKE_OBJCXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM} CACHE INTERNAL "")
			set(CMAKE_CUDA_COMPILER_LAUNCHER ${CCACHE_PROGRAM} CACHE INTERNAL "") # CMake 3.9+
			set(CCACHE_SET ON CACHE INTERNAL "")
		endif()
	endif()

	if(OS_LINUX AND NOT LINUX_PORTABLE)
		set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
		set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${LINUX_MAINTAINER_EMAIL}")
		set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
		set(PKG_SUFFIX "-linux-x86_64" CACHE STRING "Suffix of package name")
		set(CPACK_PACKAGE_FILE_NAME
			"${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}${PKG_SUFFIX}")

		set(CPACK_GENERATOR "DEB")

		set(CPACK_SET_DESTDIR ON)
		include(CPack)
	endif()

	function(setup_plugin_target target)
		if(LINUX_PORTABLE)
			set(PLUGIN_BIN ${target}/bin/${_ARCH_SUFFIX}bit)
			set(PLUGIN_DATA ${target}/data)
		elseif(libobs_VERSION VERSION_LESS "33.0.0")
			set(PLUGIN_BIN ${CMAKE_INSTALL_LIBDIR}/obs-plugins)
			set(PLUGIN_DATA ${CMAKE_INSTALL_DATAROOTDIR}/obs/obs-plugins/${target})
		else()
			set(PLUGIN_BIN ${CMAKE_INSTALL_LIBDIR}/obs-modules/plugins)
			set(PLUGIN_DATA ${CMAKE_INSTALL_DATAROOTDIR}/obs/obs-modules/plugins/${target})
		endif()

		set_target_properties(${target} PROPERTIES PREFIX "")

		install(
			TARGETS ${target}
			RUNTIME DESTINATION "${PLUGIN_BIN}"
			LIBRARY DESTINATION "${PLUGIN_BIN}"
		)

		install(
			DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data/
			DESTINATION ${PLUGIN_DATA}
			USE_SOURCE_PERMISSIONS
			OPTIONAL
		)
	endfunction()

elseif(OS_WINDOWS)
	function(setup_plugin_target target)
		set_target_properties(${target} PROPERTIES PREFIX "")

		if(libobs_VERSION VERSION_LESS "33.0.0")
			set(PLUGIN_BIN "${target}/bin/${_ARCH_SUFFIX}bit")
			set(PLUGIN_DATA "${target}/data")
		else()
			set(PLUGIN_BIN "${target}")
			set(PLUGIN_DATA "${target}/data")
		endif()

		install(
			TARGETS ${target}
			RUNTIME DESTINATION "${PLUGIN_BIN}"
			LIBRARY DESTINATION "${PLUGIN_BIN}"
		)

		install(
			FILES $<TARGET_PDB_FILE:${target}>
			CONFIGURATIONS "RelWithDebInfo" "Debug"
			DESTINATION ${PLUGIN_BIN}
			OPTIONAL)

		if(MSVC)
			target_link_options(
				${target}
				PRIVATE
				"LINKER:/OPT:REF"
				"$<$<NOT:$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>>:LINKER\:/SAFESEH\:NO>"
				"$<$<CONFIG:DEBUG>:LINKER\:/INCREMENTAL:NO>"
				"$<$<CONFIG:RELWITHDEBINFO>:LINKER\:/INCREMENTAL:NO>")
		endif()

		install(
			DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data/
			DESTINATION ${PLUGIN_DATA}/
			USE_SOURCE_PERMISSIONS
			OPTIONAL)
	endfunction()

else()
	message(FATAL_ERROR "Should not reach here.")
endif()
