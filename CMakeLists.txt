cmake_minimum_required(VERSION 3.10)

project(i960SxChipset_rpi400)

add_executable(chipset400
	main.cc)

target_link_libraries(chipset400
		      wiringpi)
