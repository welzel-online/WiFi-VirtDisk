#******************************************************************************
# version.cmake
#
# Generates version.h from version.in using SubWCRev if available
# or using GitWCRev if available.
#
# ${version_h_fake} is used here,
# because we rely on that file being detected as missing
# every build so that the real header "version.h" is updated.
#
# Copyright (c) 2025 by Welzel-Online
#******************************************************************************

# Check for SubWCRev or GitWCRev
find_program( SubWCRev_found NAMES SubWCRev )
find_program( GitWCRev_found NAMES GitWCRev )

# Check if the repository is a subversion repository
if( SubWCRev_found )
    execute_process(
        COMMAND SubWCRev.exe "${CMAKE_SOURCE_DIR}" NUL NUL
        RESULT_VARIABLE SubWCRev_result
        OUTPUT_QUIET
        ERROR_QUIET
    )
endif()

# Check if the repository is a git repository
if( GitWCRev_found )
    execute_process(
        COMMAND GitWCRev.exe "${CMAKE_SOURCE_DIR}" NUL NUL
        RESULT_VARIABLE GitWCRev_result
        OUTPUT_QUIET
        ERROR_QUIET
    )
endif()

# If either SubWCRev or GitWCRev is available and the repository is valid
# use it to generate version.h
if( SubWCRev_result EQUAL 0 OR GitWCRev_result EQUAL 0 )

    set( version_h_real "${CMAKE_SOURCE_DIR}/src/version.h" )
    set( version_h_fake "${CMAKE_SOURCE_DIR}/src/version.h_fake" )
    if( SubWCRev_result EQUAL 0 )
        set( version_in "${CMAKE_SOURCE_DIR}/src/version-svn.in" )
        set( version_tool SubWCRev )
        message( STATUS "Using SubWCRev to generate version.h" )
    else()
        set( version_in "${CMAKE_SOURCE_DIR}/src/version-git.in" )
        set( version_tool GitWCRev )
        message( STATUS "Using GitWCRev to generate version.h" )
    endif()

    if( EXISTS ${version_h_fake} )
            message( FATAL_ERROR "File \"${version_h_fake}\" found, this should never be created, remove!" )
    endif()

    # A custom target that is always built
    add_custom_target( versioninfo ALL
            DEPENDS ${version_h_fake}
    )

    # Create version.h
    add_custom_command(
        OUTPUT
        ${version_h_fake}  # ensure we always run
        ${version_h_real}
        COMMAND ${version_tool} "${CMAKE_SOURCE_DIR}" "${version_in}" "${version_h_real}"
    )

    # version.h is a generated file
    set_source_files_properties(
            ${version_h_real}
            PROPERTIES GENERATED TRUE
            HEADER_FILE_ONLY TRUE
    )

    unset(version_h_real)
    unset(version_h_fake)
    unset(version_in)
    unset(version_tool)
endif()
