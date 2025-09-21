/***************************************************************************//**
 * @file    SPICallbacks.h
 *
 * @brief   Callback functions of the SPI Slave.
 *
 * @copyright   Copyright (c) 2025 by Welzel-Online
 ******************************************************************************/

#ifndef SPICALLBACKS
#define SPICALLBACKS

/****************************************************************** Includes **/
#include "Arduino.h"


/******************************************************************* Defines **/
// SPI Status
enum 
{
    SPISLAVE_READY,
    SPISLAVE_BUSY,
    SPISLAVE_CHKSUM_ERR
};

#define SPI_STATUS_RESET 0x00000000

enum 
{
    SPISLAVE_RX_READY,
    SPISLAVE_RX_BUSY,
    SPISLAVE_RX_ERROR
};

enum 
{
    SPISLAVE_TX_READY,
    SPISLAVE_TX_NODATA,
    SPISLAVE_TX_PREP_DATA,
    SPISLAVE_TX_WAIT_OF_CONFIRM
};


/********************************************************** Global Variables **/
extern uint8_t* spiDataBuf;


/******************************************************* Functions / Methods **/
// SPI Events
void SPIOnData( uint8_t* data, size_t len );
void SPIOnDataSent( void );
void SPIOnStatus( uint32_t data );
void SPIOnStatusSent( void );

void waitSPIReady( void );


#endif
