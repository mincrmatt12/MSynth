# MAIN CMAKE FILE
cmake_policy(SET CMP0057 NEW)

# TOOLCHAIN SETUP
set(TOOLCHAIN_DIR $ENV{HOME}/.platformio/packages/toolchain-gccarmnoneeabi)
set(FRAMEWORK_DIR ${CMAKE_CURRENT_LIST_DIR})
if (NOT EXISTS ${TOOLCHAIN_DIR})
	message(FATAL_ERROR "Please build the bootloader first with platformio, as we steal it's dependencies")
else()
	message(STATUS "Found toolchain at ${TOOLCHAIN_DIR}")
endif()

enable_language(C CXX ASM)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER ${TOOLCHAIN_DIR}/bin/arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_DIR}/bin/arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_DIR}/bin/arm-none-eabi-as)
set(CMAKE_EXE_LINKER_FLAGS "")
SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS)
SET(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS)

set(COMMON_FLAGS " -fdata-sections -ffunction-sections -Os --specs=nosys.specs -nostdlib")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_FLAGS} -fno-exceptions -fno-rtti")

# HELPER FUNCTIONS
macro(setflag FLAGNAME FLAGL FLAGV VARS)
	if (NOT FLAGNAME IN_LIST FLAGL)
		math(EXPR VARS "${${VARS}} & ((1 << ${FLAGV}) ^ 65535)") # unary ~ doesn't work in cmake. i know
	endif()
endmacro()

# PUBLIC API
function(add_app 
	target_name app_name app_id)
	
	# FLAGS are ARGN
	
	# Each app target needs: a linker script that sets up for partial linking
	# (i.e. it generates a script that _only_ puts symbols in the correct sections)
	#
	# A set of source files

	# This function will create a target called app_<target_name>
	# which will generate app_<target_name>.a
	set(app_dir ${PROJECT_SOURCE_DIR}/${target_name})

	if (NOT EXISTS ${app_dir})
		message(FATAL_ERROR "App dir ${target_name} not found.")
	elseif (NOT EXISTS ${app_dir}/src)
		message(FATAL_ERROR "Source dir ${target_name}/src not found.")
	endif ()

	# The libraries for MSynth and vendor are generated in the add_bucket call, so we don't explicitly define /
	# link them here.

	# We do, however, need to generate the required assembler files.

	# Generate the appflags
	set(app_flags 65535) # 0xffff (unused bits should be kept at 1)
	# Carve out the nonspecified bits
	setflag("PRIMARY" "${ARGN}" 0 app_flags)
	setflag("TEST" "${ARGN}" 1 app_flags)
	setflag("EEPROM" "${ARGN}" 4 app_flags)
	setflag("FS" "${ARGN}" 9 app_flags)
	setflag("LOWSCREEN" "${ARGN}" 10 app_flags)
	setflag("APPCMD" "${ARGN}" 12 app_flags)

	# Configure file data
	message(STATUS "Generating ${app_name}_startup.s")
	configure_file(${FRAMEWORK_DIR}/startup/app_startup.s.in ${target_name}_startup.s)

	# Find sources
	file(GLOB_RECURSE app_srcs "${app_dir}/src/*.cpp")
	file(GLOB_RECURSE app_srcs2 "${app_dir}/src/*.c")

	# Add the startup file
	list(APPEND app_srcs ${CMAKE_CURRENT_BINARY_DIR}/${target_name}_startup.s)

	# Create executable target (so we can run ld)
	add_executable(app_${target_name} ${app_srcs} ${app_srcs2})
	set_target_properties(app_${target_name} PROPERTIES
		LINK_FLAGS "-Wl,-T${FRAMEWORK_DIR}/ld/app.ld")
endfunction()
