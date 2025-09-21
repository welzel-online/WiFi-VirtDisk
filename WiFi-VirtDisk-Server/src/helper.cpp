/***************************************************************************//**
 * @file    helper.cpp
 *
 * @brief   Helper functions.
 *
 * @copyright   Copyright (c) 2025 by Welzel-Online
 ******************************************************************************/


/****************************************************************** Includes **/
#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#include <limits.h>
#endif

#include "helper.h"


/******************************************************************* Defines **/

/********************************************************** Global Variables **/

/******************************************************* Functions / Methods **/
#if defined(_WIN32)
/***************************************************************************//**
 * @brief   Returns the directory of the executable.
 *
 * @return  The directory of the executable.
 ******************************************************************************/
std::string getExeDir( void )
{
    char buffer[MAX_PATH];


    GetModuleFileNameA( NULL, buffer, MAX_PATH );
    std::string path( buffer );
    size_t pos = path.find_last_of( "\\/" );

    return ( std::string::npos == pos ) ? "" : path.substr( 0, pos );
}


/***************************************************************************//**
 * @brief   Sleeps for the specified duration.
 *
 * @param   milliseconds   The duration to sleep in milliseconds.
 ******************************************************************************/
void genSleep( unsigned long milliseconds )
{
    Sleep( milliseconds );
}

#elif defined(__linux__)
/***************************************************************************//**
 * @brief   Returns the directory of the executable.
 *
 * @return  The directory of the executable.
 ******************************************************************************/
std::string getExeDir( void )
{
    char buffer[PATH_MAX];


    ssize_t len = readlink( "/proc/self/exe", buffer, sizeof(buffer)-1 );
    if( len != -1 )
    {
        buffer[len] = '\0';
        std::string path( buffer );
        size_t pos = path.find_last_of( "/" );

        return ( pos == std::string::npos ) ? "" : path.substr( 0, pos );
    }

    return "";
}


/***************************************************************************//**
 * @brief   Sleeps for the specified duration.
 *
 * @param   milliseconds   The duration to sleep in milliseconds.
 ******************************************************************************/
void genSleep( unsigned long milliseconds )
{
    usleep( milliseconds * 1000 );
}

#endif
