/***************************************************************************//**
 * @file    virtDisk.cpp
 *
 * @brief   Handles the virtual disks.
 *
 * @copyright   Copyright (c) 2025 by Welzel-Online
 ******************************************************************************/

/****************************************************************** Includes **/
#include <stdint.h>
#include "virtDisk.h"
#include "Arduino.h"
#include "pffArduino.h"


/******************************************************************* Defines **/
#define DBG_SPI_READY false

#define MAX_ATTEMPTS (uint8_t)4

typedef enum
{
  SRV_CONNECTED = 0x01,
  SRV_DISCONNECTED = 0x02,
  DBG_SRV_CONNECTED = 0x04,
  DBG_SRV_DISCONNECTED = 0x08
} srvStatus_t;


/********************************************************** Global Variables **/
vdStatus_t vdStatus;
uint8_t    vdChecksum;
// uint8_t bytesSent;


/******************************************************* Functions / Methods **/

/***************************************************************************//**
 * @brief   Read the SPI status registers set from SPI client.
 ******************************************************************************/
uint8_t rdStatus_spi( void )
{
  SELECT();                         // Enable CS
  xmit_spi( SPI_RD_STATUS );
  vdStatus.status     = rcv_spi();  // Read SPI status
  vdStatus.cmd_status = rcv_spi();  // Read command status
  vdStatus.cmd_data   = rcv_spi();  // Read command data
  vdStatus.free       = rcv_spi();  // Free
  DESELECT();                       // Disable CS

  return vdStatus.status;
}


/***************************************************************************//**
 * @brief   Checks the SPI status, if the Client is ready.
 *
 * @param   timeout If true, this function has a timeout, otherwise not.
 * @param   dbgInfo Debug information to print in case of an error.
 *
 * @return  0 if SPI is ready, 1 otherwise
 ******************************************************************************/
int8_t waitReady_spi( bool timeout, const char* dbgInfo )
{
  uint32_t delayTime = 1;   // 8
  int8_t   retVal;


  // Todo: Check if we need such a long wait time
  delayMicroseconds(500);
  // delay(1);   // 6

  retVal = rdStatus_spi();
  while( retVal == SPISLAVE_BUSY )
  { 
    // if( ( dbgInfo != "" ) && ( DBG_SPI_READY == true ) ) { Serial.print( dbgInfo ); }

    delay(delayTime); 
    delayTime = delayTime * 2;

    if( ( timeout == true ) && ( delayTime > 512 ) )
    {
      retVal = 1;
      break;
    }

    retVal = rdStatus_spi();
  }

  // delay(10);
  if( DBG_SPI_READY == true )
  {
    if( ( dbgInfo != "" ) && ( retVal == 1 ) ) 
    { 
      Serial.print( dbgInfo ); 
    }

    if( retVal == 2 )
    {
      Serial.print( dbgInfo );
      Serial.println( F(" - Checksum error!") );
    }
  }

  return retVal;
}



FRESULT vd_status( uint8_t* wifiStatus, uint8_t* srvStatus )
{
  uint8_t status = FR_OK;
  uint8_t attempts = 0;


  // Initialize SPI communication.
  init_spi();   // Init hardware
  // Set speed of SPI transfer.
  SPCR = (1 << SPE) | (1 << MSTR) | 1;  // Clk/16 = 1MHz
  SPSR = 0;                             
  DESELECT();   // Disable CS
  delay(2);

  // VD_CMD_STATUS
  // Should check the Status of the ESP8266: WiFi and Server connection.
  do 
  {
    attempts++;

    SELECT();                   // Enable CS
    xmit_spi( SPI_WR_DATA );    // ESP8266 Prefix-Bytes - Write Data
    xmit_spi( 0x00 );

    vdChecksum = 0xFF;
    xmit_spi( VD_CMD_STATUS );  // Byte 1: Send command
    vdChecksum += VD_CMD_STATUS;
    xmit_spi( ~vdChecksum );    // Byte 2: Send checksum
    DESELECT();                 // Disable CS

    int8_t spiStat = waitReady_spi( true, "ST " );
    if( spiStat == SPISLAVE_CHKSUM_ERR )
    {
      attempts++;   // Checksum error, try again
      status = FR_NOT_READY;
    }
    else
    {
      if( spiStat == 0 )
      {
        *wifiStatus = vdStatus.cmd_status;
        *srvStatus  = vdStatus.cmd_data;

        // WiFi-Status: WL_CONNECTED = 3
        if( ( vdStatus.cmd_status != WL_CONNECTED ) && ( ( vdStatus.cmd_data & SRV_CONNECTED ) != SRV_CONNECTED ) )
        {
          status = FR_NOT_READY;
        }
      }
      else
      {
        status = FR_NOT_READY;

        *wifiStatus = 0xFF;
        *srvStatus  = 0x00;
      }
      break;
    }

  } while( attempts <= MAX_ATTEMPTS );


  return (FRESULT)status;
}


FRESULT vd_mount (
	FATFS *fs		/* Pointer to new file system object */
)
{
  uint8_t status = FR_OK;
  // uint8_t attempts = 0;
  uint8_t wifiStatus;
  uint8_t srvStatus;


  // // Initialize SPI communication.
  // init_spi();   // Init hardware
  // // Set speed of SPI transfer.
  // SPCR = (1 << SPE) | (1 << MSTR) | 1;  // Clk/16 = 1MHz
  // SPSR = 0;                             
  // DESELECT();   // Disable CS
  // delay(2);

  // VD_CMD_STATUS
  // Should check the Status of the ESP8266: WiFi and Server connection.
  status = vd_status( &wifiStatus, &srvStatus );


  // do 
  // {
  //   attempts++;

  //   SELECT();                   // Enable CS
  //   xmit_spi( SPI_WR_DATA );
  //   xmit_spi( 0x00 );

  //   vdChecksum = 0xFF;
  //   xmit_spi( VD_CMD_STATUS );  // Send command
  //   vdChecksum += VD_CMD_STATUS;
  //   xmit_spi( ~vdChecksum );    // Send checksum
  //   DESELECT();                 // Disable CS

  //   // int8_t spiStat = waitReady_spi( true, "M " );
  //   // if( ( vdStatus.cmd_status != 0 ) || ( spiStat != 0 ) ) { status = FR_NOT_READY; }
  
  //   int8_t spiStat = waitReady_spi( true, "M " );
  //   if( spiStat == SPISLAVE_CHKSUM_ERR )
  //   {
  //     attempts++;   // Checksum error, try again
  //     status = FR_NOT_READY;
  //   }
  //   else
  //   {
  //     if( spiStat == 0 )
  //     {
  //       // WiFi-Status: WL_CONNECTED = 3
  //       if( ( vdStatus.cmd_status == 3 ) && ( ( vdStatus.cmd_data & SRV_CONNECTED ) == SRV_CONNECTED ) )
  //       {
  //         status = FR_OK;
  //       }
  //     }
  //     else
  //     {
  //       status = FR_NOT_READY;
  //     }
  //     break;
  //   }

  // } while( attempts <= MAX_ATTEMPTS );


  return (FRESULT)status;
}


FRESULT vd_open (
	const char *path	/* Pointer to the file name */
)
{
  uint8_t status   = FR_OK;
  uint8_t attempts = 0;


  // VD_CMD_SEL_FILE
  do 
  {
    attempts++;

    SELECT();                     // Enable CS
    xmit_spi( SPI_WR_DATA );      // ESP8266 Prefix-Bytes - Write Data
    xmit_spi( 0x00 );

    vdChecksum = 0xFF;
    xmit_spi( VD_CMD_SEL_FILE );  // Byte 1: Send command
    vdChecksum += VD_CMD_SEL_FILE;
    
    while( *path != '\0' )        // Byte 2+n: Send filename
    {                             // n = length of filename
      xmit_spi( *path );
      vdChecksum += *path;
      path++;
    }
    xmit_spi( 0x00 );             // Byte 3+n: Terminating Null
    xmit_spi( ~vdChecksum );      // Byte 4+n: Send checksum
    DESELECT();                   // Disable CS

    // int8_t spiStat = waitReady_spi( true, "O " );
    // if( ( vdStatus.cmd_status != 0 ) || ( spiStat != 0 ) ) { status = FR_NOT_OPENED; }

    int8_t spiStat = waitReady_spi( true, "O " );
    if( spiStat == SPISLAVE_CHKSUM_ERR )
    {
      attempts++;   // Checksum error, try again
      status = FR_NOT_OPENED;
    }
    else
    {
      if( ( vdStatus.cmd_status != 0 ) || ( spiStat != 0 ) )
      { 
        status = FR_NOT_OPENED; 
      }
      else 
      {
        status = FR_OK;
      }
      break;
    }

  } while( attempts <= MAX_ATTEMPTS );


  return (FRESULT)status;
}


FRESULT vd_read (
	void* buff, /* Pointer to the read buffer (NULL:Forward data to the stream)*/
	UINT btr,		/* Number of bytes to read */
	UINT* br		/* Pointer to number of bytes read */
)
{
  uint8_t status   = FR_OK;
  uint8_t offset   = 0;
  uint8_t attempts = 0;

  byte dataLen = 0;
  byte *buf = (byte*)buff;

  // VD_CMD_RD_FILE (32 Bytes)
  // 2x 16 Bytes
  do 
  {
    attempts++;

    SELECT();                   // Enable CS
    xmit_spi( SPI_WR_DATA );    // ESP8266 Prefix-Bytes - Write Data
    xmit_spi( 0x00 );

    vdChecksum = 0xFF;
    xmit_spi( VD_CMD_RD_FILE ); // Byte 1: Send command
    vdChecksum += VD_CMD_RD_FILE;
    xmit_spi( offset );         // Byte 2: Send offset
    vdChecksum += offset;
    xmit_spi( 16 );             // Byte 3: Send number of bytes to read
    vdChecksum += 16;
    xmit_spi( ~vdChecksum );    // Byte 4: Send checksum
    DESELECT();                 // Disable CS

    int8_t spiStat = waitReady_spi( true, "R " );
    if( ( vdStatus.cmd_status == 0 ) && ( spiStat == 0 ) )
    {
      // dataLen += vdStatus.cmd_data;  // Read data length

      // Serial.printf( " Status: %d ", vdStatus.cmd_status );
      // Serial.printf( "dataLen: %d\n\r", vdStatus.cmd_data );
      // Serial.printf( "dataLen: %d - Data:\n\r", vdStatus.cmd_data );

      SELECT();                   // Enable CS
      xmit_spi( SPI_RD_DATA );    // ESP8266 Prefix-Bytes - Read Data
      xmit_spi( 0x00 );

      vdChecksum = 0;
      vdChecksum += rcv_spi();    // Byte 1: Receive command

      byte i;
      for( i = 0; ( (i < vdStatus.cmd_data) && (i < btr) ); i++ )
      {
        buf[(offset*16)+i] = rcv_spi();       // Byte 2+n: Read data (n=vdStatus.cmd_data)
        vdChecksum += buf[(offset*16)+i];

        // Serial.printf( "\n\r%i:%i - 0x%02X - 0x%02X", offset, i, buf[(offset*16)+i], vdChecksum );
      }
      vdChecksum += rcv_spi();    // Byte 3+n: Read checksum
      // Serial.printf( "\n\rChecksum 0x%02X", vdChecksum );
      DESELECT();                 // Disable CS

      if( vdChecksum == 0 )
      {
        dataLen += vdStatus.cmd_data;  // Read data length
        
        offset++;
        if( offset > 1 )
        {
          *br = dataLen;

          // Serial.println();
          status = vdStatus.cmd_status;
        }
      }
      // else
      // {
      //   Serial.printf( "\n\rRead Checksum Error - %02X - %i\n\r", vdChecksum, offset );
      // }
    }
    else 
    {
      if( attempts > (2*MAX_ATTEMPTS) )
      {
        status = FR_DISK_ERR;   // FR_NO_FILE;
        break;
      }
    }

    // if( attempts > 2 ) { Serial.printf( "Read attempt #%i, Offset %i\n\r", attempts, offset ); }

  } while( offset < 2 );

  if( status == FR_OK )
  {
    attempts = 0;
    do
    {
      // Finalize the read and set the read pointer
      SELECT();                   // Enable CS
      xmit_spi( SPI_WR_DATA );    // ESP8266 Prefix-Bytes - Write Data
      xmit_spi( 0x00 );

      vdChecksum = 0xFF;
      xmit_spi( VD_CMD_RD_NEXT ); // Byte 1: Send command
      vdChecksum += VD_CMD_RD_NEXT;
      xmit_spi( dataLen );        // Byte 2: Send data length
      vdChecksum += dataLen;
      xmit_spi( ~vdChecksum );    // Byte 3: Send checksum
      DESELECT();                 // Disable CS

      int8_t spiStat = waitReady_spi( true, "RN " );
      if( ( vdStatus.cmd_status == 0 ) && ( spiStat == 0 ) )
      {
        // Error handling??

        break;
      }
      else
      {
        attempts++;
      }
    } while( attempts <= MAX_ATTEMPTS );

    if( attempts > MAX_ATTEMPTS ) { status = FR_DISK_ERR; }
  }

  return (FRESULT)status;
}


FRESULT vd_write (
	const void* buff, /* Pointer to the data to be written */
	UINT btw,			    /* Number of bytes to write (0:Finalize the current write operation) */
	UINT* bw			    /* Pointer to number of bytes written */
)
{
  uint8_t status   = FR_OK;
  uint8_t offset   = 0;
  uint8_t attempts = 0;

  byte dataLen = 0;
  byte *buf = (byte*)buff;


  if( !btw )  /* Finalize request */
  {		
    // Serial.println( "Finalize" );
    *bw = 0;
  }
  else 
  {
    // VD_CMD_WR_FILE (32 Bytes)
    // 2x 16 Bytes
    do 
    {
      attempts++;

      SELECT();                   // Enable CS
      xmit_spi( SPI_WR_DATA );    // ESP8266 Prefix-Bytes - Write Data
      xmit_spi( 0x00 );

      vdChecksum = 0xFF;
      xmit_spi( VD_CMD_WR_FILE ); // Byte 1: Send command
      vdChecksum += VD_CMD_WR_FILE;
      xmit_spi( offset );         // Byte 2: Send offset
      vdChecksum += offset;
      xmit_spi( 16 );             // Byte 3: Send number of bytes to write
      vdChecksum += 16;

      // Send data
      byte i;
      for( byte i = 0; i < 16; i++ )
      {
        xmit_spi( buf[(offset*16)+i] );       // Byte 4+n: Write data (n=16)
        vdChecksum += buf[(offset*16)+i];
        
        // Serial.printf( "\n\r%i:%i - 0x%02X - 0x%02X", offset, i, buf[(offset*16)+i], vdChecksum );
      }

      xmit_spi( ~vdChecksum );    // Byte 5+n: Send checksum
      DESELECT();                 // Disable CS
    
      int8_t spiStat = waitReady_spi( true, "W " );
      if( ( vdStatus.cmd_status == 0 ) && ( spiStat == 0 ) )
      {
        // Serial.printf( " Status: %d ", vdStatus.cmd_status );
        // Serial.printf( "dataLen: %d\n\r", vdStatus.cmd_data );
        // Serial.printf( "dataLen: %d - Data:\n\r", vdStatus.cmd_data );
        
        dataLen += 16;

        offset++;
        if( offset > 1 )
        {
          *bw = dataLen;
        }
      }
      else
      {
        if( attempts > (2*MAX_ATTEMPTS) )
        {
          status = FR_DISK_ERR;   // FR_NO_FILE;
          break;
        }
      }

      if( attempts > 2 ) { Serial.printf( "Write attempt #%i, Offset %i\n\r", attempts, offset ); }

    } while( offset < 2 );
  }

  // Finalize here
  if( status == FR_OK )
  {
    attempts = 0;
    do
    {
      // Finalize the read and set the read pointer
      SELECT();                   // Enable CS
      xmit_spi( SPI_WR_DATA );    // ESP8266 Prefix-Bytes - Write Data
      xmit_spi( 0x00 );

      vdChecksum = 0xFF;
      xmit_spi( VD_CMD_WR_NEXT ); // Byte 1: Send command
      vdChecksum += VD_CMD_WR_NEXT;
      xmit_spi( dataLen );        // Byte 2: Send data length
      vdChecksum += dataLen;
      xmit_spi( ~vdChecksum );    // Byte 3: Send checksum
      DESELECT();                 // Disable CS

      int8_t spiStat = waitReady_spi( true, "WN " );
      if( ( vdStatus.cmd_status == 0 ) && ( spiStat == 0 ) )
      {
        // Error handling??

        break;
      }
      else
      {
        attempts++;
      }
    } while( attempts <= MAX_ATTEMPTS );

    if( attempts > MAX_ATTEMPTS ) { status = FR_DISK_ERR; }
  }

  return (FRESULT)status;
}


FRESULT vd_lseek (
	DWORD ofs	  /* File pointer from top of file */
)
{
  uint8_t status = FR_OK;
  uint8_t attempts = 0;


  // VD_CMD_SEEK_FILE
  do 
  {
    attempts++;

    SELECT();                         // Enable CS
    xmit_spi( SPI_WR_DATA );          // ESP8266 Prefix-Bytes - Write Data
    xmit_spi( 0x00 );

    vdChecksum = 0xFF;
    xmit_spi( VD_CMD_SEEK_FILE );     // Byte 1: Send command
    vdChecksum += VD_CMD_SEEK_FILE;

    xmit_spi( ( ofs >> 24 ) & 0xFF ); // Byte 2: Send high byte (4th)
    xmit_spi( ( ofs >> 16 ) & 0xFF ); // Byte 3: Send mid-high byte (3rd)
    xmit_spi( ( ofs >> 8 ) & 0xFF );  // Byte 4: Send mid-low byte (2nd)
    xmit_spi( ofs & 0xFF );           // Byte 5: Send low byte (1st)
    vdChecksum += ( ( ofs >> 24 ) & 0xFF );
    vdChecksum += ( ( ofs >> 16 ) & 0xFF );
    vdChecksum += ( ( ofs >> 8 ) & 0xFF );
    vdChecksum += ( ofs & 0xFF );

    xmit_spi( ~vdChecksum );          // Byte 6: Send checksum
    DESELECT();                       // Disable CS

    // int8_t spiStat = waitReady_spi( true, "LS " );
    // if( ( vdStatus.cmd_status != 0 ) || ( spiStat != 0 ) ) { status = FR_NOT_OPENED; }

    int8_t spiStat = waitReady_spi( true, "LS " );
    if( spiStat == SPISLAVE_CHKSUM_ERR )
    {
      attempts++;   // Checksum error, try again
      status = FR_NOT_OPENED;
    }
    else
    {
      if( ( vdStatus.cmd_status != 0 ) || ( spiStat != 0 ) )
      { 
        status = FR_NOT_OPENED; 
      }
      else 
      {
        status = FR_OK;
      }
      break;
    }

  } while( attempts <= MAX_ATTEMPTS );

  
  return (FRESULT)status;
}
