/***************************************************************************//**
 * @file    SPICallbacks.cpp
 *
 * @brief   Callback functions of the SPI Slave.
 *
 * @copyright   Copyright (c) 2025 by Welzel-Online
 ******************************************************************************/


/****************************************************************** Includes **/
#include "SPICallbacks.h"
#include "SPISlave.h"


/******************************************************************* Defines **/

/********************************************************** Global Variables **/
volatile bool spiDataRcvd = false;
volatile bool spiDataSent = true;
uint8_t*      spiDataBuf;

// volatile uint32_t spiSlaveStatus;
volatile uint32_t spiMasterStatus;


/******************************************************* Functions / Methods **/

// status has been received from the master.
// The status register is a special register that bot the slave and the master can write to and read from.
// Can be used to exchange small data or status information
void IRAM_ATTR SPIOnStatus( uint32_t data )
{
  // Serial.printf( "Status: %u\n", data );
  spiMasterStatus = data;
}


// The master has read the status register
void IRAM_ATTR SPIOnStatusSent( void )
{
  // Serial.printf( "Status Sent: 0x%08X\n", SPISlave.getStatus() );
  // Serial.println( "Status sent" );
}


/***************************************************************************//**
 * @brief   Data received from the master.
 *
 * @param   data  Pointer to the data buffer.
 * @param   len   Length of the data in buffer.
 ******************************************************************************/
void IRAM_ATTR SPIOnData( uint8_t* data, size_t len )
{
  SPISlave.setStatus( SPISLAVE_BUSY );
  
  uint32_t savedPS = noInterrupts();  // cli();

  spiDataBuf = data;
  spiDataRcvd = true;

  xt_wsr_ps(savedPS);  // sei();

  // Serial.println( "SPIOnData" );
}


/***************************************************************************//**
 * @brief   Data was complete sent to the master.
 ******************************************************************************/
void IRAM_ATTR SPIOnDataSent( void ) 
{
    // uint32_t savedPS = noInterrupts();  // cli();
    
    spiDataSent = true;

    // Serial.println( "SPIOnDataSent" );

    // xt_wsr_ps(savedPS);  // sei();
}


/***************************************************************************//**
 * @brief   Wait for SPI ready (Needs flag from interrupt function).
 ******************************************************************************/
void IRAM_ATTR waitSPIReady( void )
{
  while( !spiDataSent ) {}
  spiDataSent = false;
}
