# face-tracker-test-sources: the sources that tests/integration puts the
# face tracker's filters on. See tests/integration/README.md.
#
# A plugin of its own, and not the face tracker's, so that the plugin that is
# tested is the one that is released. It uses nothing of the face tracker's
# but libobs, which the top level finds.

add_library(face-tracker-test-sources MODULE
	test-sources.c
	test-video-source.c
)

target_link_libraries(face-tracker-test-sources PRIVATE OBS::libobs)

if(OS_WINDOWS)
	target_link_libraries(face-tracker-test-sources PRIVATE OBS::w32-pthreads)
endif()

if(OS_LINUX)
	target_compile_options(face-tracker-test-sources PRIVATE -Wall -Wextra -Werror)
	target_link_options(face-tracker-test-sources PRIVATE "-Wl,-z,defs")
endif()

# A plain library, on macOS as well: the tests put it in a bundle
set_target_properties(face-tracker-test-sources PROPERTIES PREFIX "" AUTOMOC OFF)
