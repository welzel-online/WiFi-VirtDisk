/***************************************************************************//**
 * @file    input.cpp
 *
 * @brief   Handles the keyboard input for Windows and Linux.
 *
 * @copyright   Copyright (c) 2025 by Welzel-Online
 ******************************************************************************/


/****************************************************************** Includes **/
#if defined(_WIN32)
#include <windows.h>
#include <conio.h>
#else
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#endif

#include "input.h"


/******************************************************************* Defines **/

/********************************************************** Global Variables **/

/******************************************************* Functions / Methods **/
#if defined(_WIN32)
/***************************************************************************//**
 * @brief   Checks if a key is pressed.
 *
 * @param   key         Pointer to the variable to store the key code.
 * @param   isSpecial   Pointer to the variable to store if the key is a
 *                      special key.
 *
 * @return  true if a key is pressed, otherwise false.
 ******************************************************************************/
bool isKeyPressed( int* key, bool* isSpecial )
{
    bool keyPressed = false;
    int ch;


    HANDLE hInput = GetStdHandle( STD_INPUT_HANDLE );
    DWORD  waitResult = WaitForSingleObject( hInput, 100 );

    if( waitResult == WAIT_OBJECT_0 )
    {
        if( kbhit() )
        {
            ch = _getch();

            // Special key pressed?
            if( ch == 224 )
            {
                ch = _getch();  // Read the key code
                *isSpecial = true;
            }
            else { *isSpecial = false; }

            *key = ch;
            keyPressed = true;
        }
    }

    return keyPressed;
}

#else

/***************************************************************************//**
 * @brief   Checks if a key is pressed.
 *
 * @param   key         Pointer to the variable to store the key code.
 * @param   isSpecial   Pointer to the variable to store if the key is a
 *                      special key.
 *
 * @return  true if a key is pressed, otherwise false.
 ******************************************************************************/
bool isKeyPressed( int* key, bool* isSpecial )
{
    struct termios oldt;
    struct termios newt;
    struct pollfd pfd;
    bool keyPressed = false;
    int ch;


    // Set terminal to non-canonical mode
    tcgetattr( STDIN_FILENO, &oldt );   // Save current settings
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);   // Disable canonical mode and echo
    tcsetattr( STDIN_FILENO, TCSANOW, &newt );

    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;
    int waitResult = poll( &pfd, 1, 100 ); // 100 ms Timeout

    if( ( waitResult > 0 ) && ( pfd.revents & POLLIN ) )
    {
        ch = getchar();

        if( ch != EOF )
        {
            *key = ch;
            keyPressed = true;
        }
    }

    tcsetattr( STDIN_FILENO, TCSANOW, &oldt ); // Restore old settings

    return keyPressed;
}
#endif
