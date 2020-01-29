cmake_minimum_required(VERSION 3.3)
project(factory C CXX ASM)

include(${CMAKE_CURRENT_LIST_DIR}/../framework/build.cmake)  # point at the right dir

# setup an APP
add_app(
	# folder /target name
	factory_app
	# name
	"FactoryTest"
	# appID
	"FTsc"
	# appSIZE (kib)
	32
	# flags
	TEST
	LOWSCREEN
	FS
	# valid options are: PRIMARY TEST EEPROM FS LOWSCREEN APPCMD
)

# setup an upload target
create_uploader(
	# name of taregt (to use with make)
	upload_factory
	# name of app
	factory_app
	# sector
	1
)
