/***************************************************************************//**
 * @file    message.h
 *
 * @brief   Message output handling.
 *
 * @copyright   Copyright (c) 2024 by Welzel-Online
 ******************************************************************************/

#ifndef MESSAGE_H
#define MESSAGE_H


/****************************************************************** Includes **/
#include <string>


/******************************************************************* Defines **/
enum class MsgType {
    INFO,
    WARN,
    ERR
};


#define COLOR_RED       "\033[38;5;9m"
#define COLOR_GREEN     "\033[38;5;10m"
#define COLOR_YELLOW    "\033[38;5;11m"
#define COLOR_ORANGE    "\033[38;5;166m"
#define COLOR_NORM      "\033[0m"


/******************************************************* Functions / Methods **/
bool isColorTerm( void );
void message( MsgType type, std::string msg );
bool isError( void );


#endif
