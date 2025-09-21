cmake_minimum_required( VERSION 3.0...3.31 )

project( WiFi-VirtDisk-Server LANGUAGES C CXX )


# Activated the optimization on level 3 (speed)
#SET(CMAKE_CXX_FLAGS "-O3")

# Activated C++17 standard
# set( CMAKE_CXX_STANDARD 17 )

# Link gcc libraries and windows libraries static
# set(CMAKE_EXE_LINKER_FLAGS "-static -static-libgcc -static-libstdc++ -Wl,-Bstatic -lstdc++ -lpthread")

add_subdirectory( src )

