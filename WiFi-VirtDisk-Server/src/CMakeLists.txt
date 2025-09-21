cmake_minimum_required( VERSION 3.0...3.31 )


# ${version_h_fake} is used here,
# because we rely on that file being detected as missing
# every build so that the real header "version.h" is updated.
#
# Keep this until we find a better way to resolve!

set(version_h_real "${CMAKE_SOURCE_DIR}/version.h")
set(version_h_fake "${CMAKE_SOURCE_DIR}/version.h_fake")

if(EXISTS ${version_h_fake})
        message(FATAL_ERROR "File \"${version_h_fake}\" found, this should never be created, remove!")
endif()

# a custom target that is always built
add_custom_target(versioninfo ALL
        DEPENDS ${version_h_fake})

# creates version.h using cmake script
add_custom_command(
        OUTPUT
                ${version_h_fake}  # ensure we always run
                ${version_h_real}
        COMMAND SubWCRev ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src/version.in ${CMAKE_SOURCE_DIR}/src/version.h)

# version.h is a generated file
set_source_files_properties(
        ${version_h_real}
        PROPERTIES GENERATED TRUE
        HEADER_FILE_ONLY TRUE)

unset(version_h_real)
unset(version_h_fake)



#SET(CMAKE_CXX_FLAGS "-O3")
set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_EXE_LINKER_FLAGS "-static -static-libgcc -static-libstdc++ -Wl,-Bstatic -lstdc++ -lpthread" )

include_directories( ${CMAKE_SOURCE_DIR}/src/cpmtools
                     ${CMAKE_SOURCE_DIR}/src/libdsk
                     ${CMAKE_SOURCE_DIR}/src/socket-cpp
                     ${CMAKE_SOURCE_DIR}/src )


# libdsk library sources
file( GLOB_RECURSE LIBDSK_SOURCES ${CMAKE_SOURCE_DIR}/src/libdsk/*.c )
add_library( libdsk STATIC ${LIBDSK_SOURCES} )
target_compile_definitions( libdsk PUBLIC NOTWINDLL )

# socket-cpp library sources
if( WIN32 )
    add_definitions( -DWINDOWS )
else()
    add_definitions( -DLINUX )
endif()

option( SOCKET_CPP_BUILD_WITHOUT_SECURE_CLASSES "Disable OpenSSL" ON )    # Disable OpenSSL
if( NOT SOCKET_CPP_BUILD_WITHOUT_SECURE_CLASSES )
    add_definitions( -DOPENSSL )
	IF( NOT MSVC )
	find_package( OpenSSL )
	ELSE()
	find_package( OpenSSL REQUIRED )
	include_directories( "${OPENSSL_INCLUDE_DIR}" )
	ENDIF()
endif()

file( GLOB_RECURSE SOCKET_SOURCES ${CMAKE_SOURCE_DIR}/src/socket-cpp/*.cpp )
add_library( socket-cpp STATIC ${SOCKET_SOURCES} )


add_executable( WiFi-VirtDisk-Server
                cpmtools/device_libdsk.c
                cpmtools/cpmfs.c
                test.cpp
                helper.cpp
                message.cpp
                input.c
                virtDisk.cpp
                version.rc
                WiFi-VirtDisk-Server.cpp
            )

if(WIN32)
    target_link_libraries( WiFi-VirtDisk-Server libdsk socket-cpp ws2_32 pthread )
else()
    target_link_libraries( WiFi-VirtDisk-Server libdsk socket-cpp pthread )
endif()

add_custom_command( TARGET WiFi-VirtDisk-Server
                    POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E echo "Copy the compiled binary to the ${CMAKE_BUILD_TYPE} folder."
                    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/${CMAKE_BUILD_TYPE}
                    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:WiFi-VirtDisk-Server> ${CMAKE_SOURCE_DIR}/${CMAKE_BUILD_TYPE}/.
                )
