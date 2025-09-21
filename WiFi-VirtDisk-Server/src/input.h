/***************************************************************************//**
 * @file    input.h
 *
 * @brief   Handles the keyboard input for Windows and Linux.
 *
 * @copyright   Copyright (c) 2025 by Welzel-Online
 ******************************************************************************/

 #ifndef INPUT_H
 #define INPUT_H

/****************************************************************** Includes **/
#include "stdbool.h"

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************* Defines **/

/********************************************************** Global Variables **/

/******************************************************* Functions / Methods **/
bool isKeyPressed( int* key, bool* isSpecial );


#ifdef __cplusplus
}
#endif

#endif
