/***************************************************************************//**
 * @file    message.cpp
 *
 * @brief   Message output handling.
 *
 * @copyright   Copyright (c) 2024 by Welzel-Online
 ******************************************************************************/


/****************************************************************** Includes **/
#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <cstdlib>
#include <iostream>
#include <string>
#include <unistd.h>
#endif

#include <iostream>

#include "message.h"


/******************************************************************* Defines **/

/********************************************************** Global Variables **/
int verbosityLvl = 0;
bool _isError = false;


/******************************************************* Functions / Methods **/
#if defined(_WIN32)
/***************************************************************************//**
 * @brief   Returns true if the terminal supports color output.
 *
 * @return  true if the terminal supports color output, otherwise false.
 ******************************************************************************/
bool isColorTerm( void )
{
    DWORD consoleMode;
    HANDLE hConsole = GetStdHandle( STD_OUTPUT_HANDLE );


    if( hConsole == INVALID_HANDLE_VALUE ) { return false; }

    // Return false, if the console mode could not be read or the output is redirected
    if( !GetConsoleMode( hConsole, &consoleMode ) || ( GetFileType( hConsole ) != FILE_TYPE_CHAR ) )
    {
        return false;
    }

    return ( ( consoleMode & ENABLE_VIRTUAL_TERMINAL_PROCESSING ) != 0 );
}

#elif defined(__linux__)
/***************************************************************************//**
 * @brief   Returns true if the terminal supports color output.
 *
 * @return  true if the terminal supports color output, otherwise false.
 ******************************************************************************/
bool isColorTerm( void )
{
    const char* term = std::getenv( "TERM" );

    // Return false, if the environment TERM could not be read or the output is redirected
    if( ( term == nullptr ) || !isatty( fileno( stdout ) ) ) { return false; }

    std::string termStr( term );

    return ( ( termStr.find( "color" ) != std::string::npos ) ||
             ( termStr.find( "xterm" ) != std::string::npos ) ||
             ( termStr == "linux" ) ||
             ( termStr == "screen" ) );
}
#else
bool isColorTerm( void )
{
    return false;
}
#endif


/***************************************************************************//**
 * @brief   Outputs a given string with a categorie of error, warning or info.
 *
 * @param   type    Message type: ERR, WARN, INFO
 * @param   msg     The string to putput.
 ******************************************************************************/
void message( MsgType type, std::string msg )
{
    switch( type )
    {
        case MsgType::ERR:
            if( isColorTerm() ) { std::cout << COLOR_RED; }       // Red
            std::cout << "ERROR: ";
            if( isColorTerm() ) { std::cout << COLOR_NORM; }      // Normal
            std::cout << msg << std::endl;
            _isError = true;
        break;

        case MsgType::WARN:
            if( isColorTerm() ) { std::cout << COLOR_ORANGE; }    // Orange
            std::cout << "WARNING: ";
            if( isColorTerm() ) { std::cout << COLOR_NORM; }      // Normal
            std::cout << msg << std::endl;
        break;

        case MsgType::INFO:
            if( isColorTerm() ) { std::cout << COLOR_YELLOW; }    // Yellow
            std::cout << "INFO: ";
            if( isColorTerm() ) { std::cout << COLOR_NORM; }      // Normal
            std::cout << msg << std::endl;
        break;

        default:
        break;
    }
}


/***************************************************************************//**
 * @brief   Returns true if an error occurred.
 *
 * @return  true if an error occurred, otherwise false.
 ******************************************************************************/
bool isError( void )
{
    return _isError;
}
