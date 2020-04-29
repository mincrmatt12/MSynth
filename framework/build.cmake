# MAIN CMAKE FILE
cmake_policy(SET CMP0057 NEW)
cmake_policy(SET CMP0069 NEW)
set(FRAMEWORK_DIR ${CMAKE_CURRENT_LIST_DIR})

# FLAGS SETUP
set(COMMON_FLAGS " -Os --specs=nano.specs -nostdlib -mthumb -mcpu=cortex-m4 -ggdb3")
# check if the user specified they wanted the FPU enabled
if (${MSYNTH_USE_FPU})
	set(COMMON_FLAGS "${COMMON_FLAGS} -mfloat-abi=hard -mfpu=fpv4-sp-d16")
	message("FPU is enabled")
endif()

add_definitions(-DSTM32F429xx -DF_CPU=168000000L -DUSE_FULL_LL_DRIVER)

if (DEFINED MSYNTH_DISABLE_LTO)
	if (NOT ${MSYNTH_DISABLE_LTO})
		set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
	else()
		set(CMAKE_INTERPROCEDURAL_OPTIMIZATION FALSE)
		message(STATUS "LTO is disabled")
	endif()
else()
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()
#set(COMMON_FLAGS "${COMMON_FLAGS} -Wl,--gc-sections,--relax -ffunction-sections")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_FLAGS} -fno-exceptions -fno-rtti -Wno-register -Wno-pmf-conversions")
set(CMAKE_EXE_LINKER_FLAGS) # disable nosys
set(CMAKE_CXX_STANDARD 17)

# FIND OPENOCD
set(OPENOCD_DIR $ENV{HOME}/.platformio/packages/tool-openocd)
if (NOT EXISTS ${OPENOCD_DIR})
	message(WARNING OPENOCD was not detected, so you can't upload)
else()
	set(OPENOCD_FOUND 1)
endif()


# HELPER FUNCTIONS
macro(setflag FLAGNAME FLAGL FLAGV VARS)
	if (NOT "${FLAGNAME}" IN_LIST ${FLAGL})
		math(EXPR ${VARS} "${${VARS}} & ((1 << ${FLAGV}) ^ 65535)") # unary ~ doesn't work in cmake. i know
	endif()
endmacro()

# SETUP LIBRARIES
set(PFRAMEWORK_DIR $ENV{HOME}/.platformio/packages/framework-stm32cube)
if (NOT EXISTS ${PFRAMEWORK_DIR})
	message(FATAL_ERROR Could not find stm32cube framework)
endif()

file(GLOB ll_srcs ${PFRAMEWORK_DIR}/f4/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_*.c)
file(GLOB mslib_srcs ${FRAMEWORK_DIR}/lib/src/*.c ${FRAMEWORK_DIR}/lib/src/*.cpp)

add_library(cmsis_ll STATIC ${ll_srcs})
add_library(mslib STATIC ${mslib_srcs})

target_include_directories(mslib PRIVATE ${FRAMEWORK_DIR}/lib/include/msynth)

include_directories(
	${PFRAMEWORK_DIR}/f4/Drivers/STM32F4xx_HAL_Driver/Inc
	${PFRAMEWORK_DIR}/f4/Drivers/CMSIS/Device/ST/STM32F4xx/Include
	${PFRAMEWORK_DIR}/f4/Drivers/CMSIS/Include
)

# PUBLIC API
function(add_app 
	target_name app_name app_id app_size)
	
	# FLAGS are ARGN
	
	# Each app target needs: a linker script that sets up for partial linking
	# (i.e. it generates a script that _only_ puts symbols in the correct sections)
	#
	# A set of source files

	# This function will create a target called app_<target_name>
	# which will generate app_<target_name>.pre.elf
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
	setflag(PRIMARY ARGN 0 app_flags)
	setflag(TEST ARGN 1 app_flags)
	setflag(EEPROM ARGN 4 app_flags)
	setflag(FS ARGN 9 app_flags)
	setflag(LOWSCREEN ARGN 10 app_flags)
	setflag(APPCMD ARGN 12 app_flags)

	message(STATUS "Creating app ${target_name} with name \"${app_name}\", ID ${app_id} and size ${app_size}")

	# Configure file data
	message(STATUS "Generating ${app_name}_startup.s")
	configure_file(${FRAMEWORK_DIR}/startup/app_startup.s.in ${target_name}_startup.s)

	# Find sources
	file(GLOB_RECURSE app_srcs "${app_dir}/src/*.cpp" "${app_dir}/src/*.cc" "${app_dir}/src/*.c")

	# Add the startup file
	list(INSERT app_srcs 0 ${CMAKE_CURRENT_BINARY_DIR}/${target_name}_startup.s)
	
	# Setup linker scripts
	if (${app_size} EQUAL 16 OR ${app_size} EQUAL 32 OR ${app_size} EQUAL 64 OR ${app_size} EQUAL 128)
		set(sectors
			0x08004000
			0x08008000
			0x0800C000
			0x08010000
			0x08020000
			0x08040000
			0x08060000
			0x08080000
			0x080A0000
			0x080E0000
		)
	elseif (${app_size} EQUAL 256)
		set(sectors
			0x08004000
			0x08008000
			0x0800C000
			0x08010000
			0x08020000
			0x08040000
			0x08060000
			0x08080000
			0x080A0000
		)
	elseif (${app_size} EQUAL 384)
		set(sectors
			0x08004000
			0x08008000
			0x0800C000
			0x08010000
			0x08020000
			0x08040000
			0x08060000
			0x08080000
		)
	elseif (${app_size} EQUAL 512)
		set(sectors
			0x08004000
			0x08008000
			0x0800C000
			0x08010000
			0x08020000
			0x08040000
			0x08060000
		)
	endif()

	# Create object library of sources (to avoid recompiling)
	add_library(app_${target_name} OBJECT ${app_srcs})
	target_include_directories(app_${target_name} PRIVATE ${FRAMEWORK_DIR}/lib/include)

	set(sector_number 1)
	foreach(sector IN LISTS sectors)
		# Create linker script
		configure_file(${FRAMEWORK_DIR}/ld/app.ld.in ${CMAKE_CURRENT_BINARY_DIR}/app_${sector}_${app_size}.ld)
		# Create executable target for sector N
		add_executable(app_${target_name}_${sector_number} $<TARGET_OBJECTS:app_${target_name}>)
		set_target_properties(app_${target_name}_${sector_number} PROPERTIES
			LINK_FLAGS "-Wl,-T${CMAKE_CURRENT_BINARY_DIR}/app_${sector}_${app_size}.ld -Wl,--gc-sections,--relax -Wl,--nmagic"
			OUTPUT_NAME app_${target_name}.${sector_number}.elf
		)
		# Add libs
		target_link_libraries(app_${target_name}_${sector_number} mslib cmsis_ll c m gcc)
		# Increment count
		math(EXPR sector_number "${sector_number} + 1")
	endforeach()
endfunction()

function(create_uploader
	target_name app_name sector_no)
	
	if (NOT OPENOCD_FOUND)
		return()
	endif()

	message(STATUS "Creating uploader with target ${target_name}")

	add_custom_target(${target_name}
		COMMAND ${OPENOCD_DIR}/bin/openocd -f ${OPENOCD_DIR}/scripts/interface/stlink.cfg -f ${OPENOCD_DIR}/scripts/target/stm32f4x.cfg
		-c "program ${CMAKE_CURRENT_BINARY_DIR}/app_${app_name}.${sector_no}.elf verify reset exit"
	)

	add_dependencies(${target_name} app_${app_name}_${sector_no})
endfunction()
