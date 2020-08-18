cmake_minimum_required(VERSION 3.3)
project(app C CXX ASM)

set(MSYNTH_USE_FPU ON)

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
	64
	# flags
	PRIMARY
	LOWSCREEN
	FS
	# valid options are: PRIMARY TEST EEPROM FS LOWSCREEN APPCMD
)

add_app(
	main
	"MSynth"
	"MSmn"
	256

	PRIMARY
	LOWSCREEN
	FS
	EEPROM
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

create_uploader(
	# name of taregt (to use with make)
	upload
	# name of app
	main
	# sector
	3
)
