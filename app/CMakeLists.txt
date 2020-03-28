cmake_minimum_required(VERSION 3.3)
project(app C CXX ASM)

include(${CMAKE_CURRENT_LIST_DIR}/../framework/build.cmake)  # point at the right dir

# setup an APP
add_app(
	# folder /target name
	stream_player
	# name
	"SD Player"
	# appID
	"Sply"
	# appSIZE (kib)
	128
	# flags
	PRIMARY
	LOWSCREEN
	FS
	EEPROM
	# valid options are: PRIMARY TEST EEPROM FS LOWSCREEN APPCMD
)

# setup an upload target
create_uploader(
	# name of taregt (to use with make)
	upload_streamer
	# name of app
	stream_player
	# sector
	3
)
