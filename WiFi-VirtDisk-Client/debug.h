/***************************************************************************//**
 * @file    debug.h
 *
 * @brief   Debug helper functions.
 *
 * @copyright   Copyright (c) 2025 by Welzel-Online
 ******************************************************************************/

#ifndef DEBUG_H
#define DEBUG_H

/****************************************************************** Includes **/

/******************************************************************* Defines **/
#define VD_DEBUG
#undef ADDITIONAL_DEBUG
#undef SPI_DEBUG
#undef DEBUG_DEBUG



#ifdef VD_DEBUG
#define DBG_PRINT( msg ) Serial.print( F(msg) )
#define DBG_PRINTLN( msg ) Serial.println( F(msg) )
#define DBG_PRINTF( ... ) Serial.printf( __VA_ARGS__ )
#else
#define DBG_PRINT( msg )
#define DBG_PRINTLN( msg )
#define DBG_PRINTF( ... )
#endif

#ifdef ADDITIONAL_DEBUG
#define DBGA_PRINT( msg ) Serial.print( F(msg) )
#define DBGA_PRINTLN( msg ) Serial.println( F(msg) )
#define DBGA_PRINTF( ... ) Serial.printf( __VA_ARGS__ )
#else
#define DBGA_PRINT( msg )
#define DBGA_PRINTLN( msg )
#define DBGA_PRINTF( ... )
#endif

#ifdef SPI_DEBUG
#define DBGS_PRINT( msg ) Serial.print( F(msg) )
#define DBGS_PRINTLN( msg ) Serial.println( F(msg) )
#define DBGS_PRINTF( ... ) Serial.printf( __VA_ARGS__ )
#else
#define DBGS_PRINT( msg )
#define DBGS_PRINTLN( msg )
#define DBGS_PRINTF( ... )
#endif

#ifdef DEBUG_DEBUG
#define DBGD_PRINTLN( msg ) Serial.println( F(msg) )
#define DBGD_PRINTF( ... ) Serial.printf( __VA_ARGS__ )
#else
#define DBGD_PRINTLN( msg )
#define DBGD_PRINTF( ... )
#endif

#endif
