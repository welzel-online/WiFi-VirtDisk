/***************************************************************************//**
 * @file    virtDisk.cpp
 *
 * @brief   Handles the virtual disks.
 *
 * @copyright   Copyright (c) 2025 by Welzel-Online
 ******************************************************************************/


/****************************************************************** Includes **/
#include <ESP8266WiFi.h>
#include "virtDisk.hpp"
#include "SPISlave.h"
#include "SPICallbacks.h"


/******************************************************************* Defines **/

/********************************************************** Global Variables **/
vdData_t   vdData;
vdPacket_t vd;
vdStatus_t vdStatus;

extern WiFiClient tcpClient;   // WiFi Client for communication
extern uint8_t    wifiStatus;
extern uint8_t    tcpSrvStatus;

extern bool     spiDataRcvd;
extern bool     spiDataSent;
uint8_t         sendBuf[32];

// uint32_t lastSeek;  // Debug


/******************************************************* Functions / Methods **/

void dumpSpiPacket( uint8_t* data )
{
#ifdef SPI_DEBUG
  for( uint8_t adr = 0; adr < 32; adr++ )
  {
    Serial.printf( "%02X ", data[adr] );
  }
  Serial.println();
#endif
}

/***************************************************************************//**
 * @brief   Wait for complete TCP packet.
 ******************************************************************************/
bool waitForTcpData( void )
{
  bool retVal = false;
  int  dataCnt = 0;
  uint16_t loop = 0;

  while( loop < 250 )
  {
    dataCnt = tcpClient.available();
    if( dataCnt == 536 )
    {
      retVal = true;
      break;
    }

    loop++;
    // delay(1);
    delayMicroseconds(500);
  }

  // Serial.printf( "TCP data count: %u - loop: %u - retVal: %u\n\r", dataCnt, loop, retVal );

  return retVal;
}



/***************************************************************************//**
 * @brief   Process the client command.
 ******************************************************************************/
void vdProcessCmd( void )
{
  static uint8_t  dataLen;    // Data length for write operation
  uint32_t        savedPS;    // Interrupt status
  uint8_t         cmd;        // Current received command
  uint8_t         checksum;   // Checksum of SPI packet
  static uint8_t  prevCmd = VD_CMD_NONE;  // Previous received command
  uint8_t         dataBuf[32];
  uint8_t         numOfBytes;
  uint8_t         offset;
  uint8_t         i;


  if( spiDataRcvd )
  {
    // Copy data from spi input buffer to local buffer
    memcpy( dataBuf, spiDataBuf, 32 );

    // Get command
    cmd = (vdCommands)(dataBuf[0]);

    // DBG_PRINTF( "cmd: %u - vdData.dataLen: %u\n", cmd, vdData.dataLen );

    switch( cmd )
    {
      case VD_CMD_STATUS:
        DBG_PRINTLN( "VD_CMD_STATUS" );

        vdStatus.rawStatus = SPI_STATUS_RESET;

        DBGS_PRINT( "Rec. SPI data: " );
        dumpSpiPacket( dataBuf );

        // Init checksum
        checksum = cmd;

        // Calculate checksum
        checksum += dataBuf[1];

        if( checksum == 0 )
        {
          DBGA_PRINTF( "wifiStatus: %d, srvStatus: %d\r\n", wifiStatus, tcpSrvStatus );

          vdStatus.cmd_status = wifiStatus;
          vdStatus.cmd_data   = tcpSrvStatus;

          // if( ( ( wifiStatus != 3 ) || ( tcpSrvStatus != 0) ) )
          // {
          //   vdStatus.cmd_status = 1;  // Error
          // }
          // else
          // {
          //   vdStatus.cmd_status = 0;
          // }
        }
        else
        {
          vdStatus.status = SPISLAVE_CHKSUM_ERR;  // Checksum error

          DBGA_PRINTF( "Checksum Error: %02X\n\r", checksum );
        }

        prevCmd = VD_CMD_NONE;

        // Set status and command status
        // vdStatus.status = SPISLAVE_READY;
        SPISlave.setStatus( vdStatus.rawStatus );
      break;

      case VD_CMD_SEL_FILE:
        DBG_PRINTLN( "VD_CMD_SEL_FILE" );

        vdStatus.rawStatus = SPI_STATUS_RESET;

        DBGS_PRINT( "Rec. SPI data: " );
        dumpSpiPacket( dataBuf );

        // Init checksum
        checksum = cmd;

        // Calculate checksum
        for( i = 0; i < sizeof(vdData.filename); i++ )
        {
          if( dataBuf[i+1] != 0 ) { checksum += dataBuf[i+1]; }
          else { break; }
        }
        checksum += dataBuf[i+2];

        if( checksum == 0 )
        {
          // Save filename and reset position and length
          memcpy( vdData.filename, dataBuf + 1, sizeof(vdData.filename) );
          vdData.filePos = 0;
          vdData.dataLen = 0;

          // Fill packet for server
          vd.packet.cmd = VD_CMD_SEL_FILE;
          memcpy( vd.packet.filename, vdData.filename, sizeof(vd.packet.filename) );

          // Send data to server
          tcpClient.write( vd.rawData, sizeof(vd.rawData) );
          tcpClient.flush();

          // Receive data from server
          if( waitForTcpData() )
          {
            DBGA_PRINTLN( "WifiClient select file" );

            tcpClient.read( vd.rawData, sizeof(vd.rawData) );

            // Serial.printf( "Status from Server: %i\n", vd.packet.status );

            vdStatus.cmd_status = 0;
          }
          else
          {
            vdStatus.cmd_status = 1;  // Error
          }
        }
        else
        {
          vdStatus.status = SPISLAVE_CHKSUM_ERR;  // Checksum error

          DBGA_PRINTF( "Checksum Error: %02X\n\r", checksum );
        }

        prevCmd = VD_CMD_NONE;

        // Set status and command status
        // vdStatus.status = SPISLAVE_READY;
        SPISlave.setStatus( vdStatus.rawStatus );
      break;

      case VD_CMD_RD_FILE:
        // DBG_PRINTLN( "VD_CMD_RD_FILE" );

        vdStatus.rawStatus = SPI_STATUS_RESET;

        // Debug
        DBGS_PRINT( "Rec. SPI data: " );
        dumpSpiPacket( dataBuf );

        offset     = dataBuf[1];
        numOfBytes = dataBuf[2];

        // Init checksum
        checksum = cmd;

        // Calculate checksum
        checksum += offset;
        checksum += numOfBytes;
        checksum += dataBuf[3];

        if( checksum == 0 )
        {
          // Do we have data?
          if( vdData.dataLen == 0 )
          {
            DBG_PRINTLN( "VD_CMD_RD_FILE" );

            DBGA_PRINTLN( "Get data from server" );

            // No data - Fill packet for server
            vd.packet.cmd = VD_CMD_RD_FILE;
            memcpy( vd.packet.filename, vdData.filename, sizeof(vd.packet.filename) );

            // Send data to server
            tcpClient.write( vd.rawData, sizeof(vd.rawData) );
            tcpClient.flush();

            // Receive data from server
            if( waitForTcpData() )
            {
              DBGA_PRINTLN( "WifiClient read data" );

              tcpClient.read( vd.rawData, sizeof(vd.rawData) );

              // Serial.printf( "Status from Server: %i\n", vd.packet.status );

              if( vd.packet.status == VD_STATUS_OK )
              {
                DBGA_PRINTLN( "WifiClient read data - Status OK" );

                // Copy data to local buffer
                memcpy( vdData.data, vd.packet.data, sizeof(vdData.data) );
                vdData.dataLen = vd.packet.dataLen;
                vdData.filePos = 0;
              }
              else
              {
                DBGA_PRINTLN( "WifiClient Error" );
              }
            }
          }

          // Clear spi send buffer
          memset( sendBuf, 0, sizeof(sendBuf) );

          // Init checksum
          checksum = 0xFF;

          // Calculate checksum
          sendBuf[0] = cmd;
          checksum += cmd;

          // Prepare data
          for( i = 0; ( i < numOfBytes ) && ( i < sizeof(sendBuf) ); i++ )
          {
            if( ( ( vdData.filePos + (offset * 16) + i ) < sizeof(vdData.data) ) && ( ( vdData.filePos + (offset * 16) + i ) < vdData.dataLen ) )
            {
              sendBuf[i+1] = vdData.data[vdData.filePos+(offset * 16)+i];

              checksum += sendBuf[i+1];   // Calculate checksum
            }
            else
            {
              break;
            }
          }

          sendBuf[i+1] = ~checksum;

          DBGS_PRINT( "Sent SPI data: " );
          dumpSpiPacket( sendBuf );   // Debug

          if( i < numOfBytes ) { numOfBytes = i; }

          // vdData.filePos += i;   // Implemented in new command VD_CMD_RD_NEXT
          // Check for buffer end
          // if( ( vdData.filePos == vdData.dataLen ) || ( vdData.filePos == sizeof(vdData.data) ) )
          // {                                           //    |- Ist diese Prüfung notwenig?
          //   vdData.dataLen = 0;
          // }

          DBGA_PRINTF( "READ - vdData.filePos: %i, dataLen: %i, numOfBytes: %i\n", vdData.filePos, vdData.dataLen, numOfBytes );

          DBGA_PRINTLN( "READ - Set data buffer" );

          // Set data buffer
          savedPS = noInterrupts();             // cli();
          SPISlave.setData( sendBuf, numOfBytes + 2 );
          xt_wsr_ps(savedPS);                   // sei();

          vdStatus.cmd_status = 0;
        }
        else
        {
          vdStatus.status = SPISLAVE_CHKSUM_ERR;  // Checksum error

          DBGA_PRINTF( "Checksum Error: %02X\n\r", checksum );
        }

        DBGA_PRINTLN( "READ - Set status" );

        prevCmd = VD_CMD_NONE;

        // Set status and command status
        // vdStatus.cmd_status = 0;            // Set status \todo Fill in correct status
        vdStatus.cmd_data   = numOfBytes;   // Set number of bytes
        // vdStatus.status     = SPISLAVE_READY;
        SPISlave.setStatus( vdStatus.rawStatus );
      break;

      case VD_CMD_RD_NEXT:
        DBGA_PRINTLN( "VD_CMD_RD_NEXT" );

        vdStatus.rawStatus = SPI_STATUS_RESET;

        // Debug
        DBGS_PRINT( "Rec. SPI data: " );
        dumpSpiPacket( dataBuf );

        numOfBytes = dataBuf[1];

        // Init checksum
        checksum = cmd;

        // Calculate checksum
        checksum += numOfBytes;
        checksum += dataBuf[2];

        if( checksum == 0 )
        {
          vdData.filePos += numOfBytes;
          // Check for buffer end
          if( ( vdData.filePos == vdData.dataLen ) || ( vdData.filePos == sizeof(vdData.data) ) )
          {                                           //    |- Ist diese Prüfung notwenig?
            vdData.dataLen = 0;
          }

          vdStatus.cmd_status = 0;
        }
        else
        {
          vdStatus.status = SPISLAVE_CHKSUM_ERR;  // Checksum error

          DBGA_PRINTF( "Checksum Error: %02X\n\r", checksum );
        }

        prevCmd = VD_CMD_NONE;

        // Set status and command status
        vdStatus.cmd_data   = numOfBytes;   // Set number of bytes
        // vdStatus.status     = SPISLAVE_READY;
        SPISlave.setStatus( vdStatus.rawStatus );
      break;

      case VD_CMD_WR_FILE:
        // DBG_PRINTLN( "VD_CMD_WR_FILE" );

        vdStatus.rawStatus = SPI_STATUS_RESET;

        // Debug
        DBGS_PRINT( "Rec. SPI data: " );
        dumpSpiPacket( dataBuf );

        offset     = dataBuf[1];
        numOfBytes = dataBuf[2];

        // Init checksum
        checksum = cmd;

        // Calculate checksum
        checksum += offset;
        checksum += numOfBytes;

        // Do we have previous data?
        if( vdData.dataLen == 0 )
        {
          DBG_PRINTLN( "VD_CMD_WR_FILE" );

          // No data - Fill packet header for server
          vd.packet.cmd = VD_CMD_WR_FILE;
          memcpy( vd.packet.filename, vdData.filename, sizeof(vd.packet.filename) );

          // memset( vd.packet.data, 0x00, sizeof(vd.packet.data) );   // Debug

          vdData.filePos = 0;
        }

        // Prepare data
        for( i = 0; i < numOfBytes; i++ )
        {
          if( ( vdData.filePos + (offset * 16) + i ) < sizeof(vdData.data) )
          {
            vdData.data[vdData.filePos+(offset*16)+i] = dataBuf[i+3];

            checksum += vdData.data[vdData.filePos+(offset*16)+i];

            // Serial.printf( "\n\r%i:%i - 0x%02X - 0x%02X", offset, i, vdData.data[vdData.filePos+(offset*16)+i], checksum );
            // // Dump SPI packet
            // Serial.printf( "%02X ", vdData.data[vdData.filePos+i] );
            // if( i == 15 ) { Serial.println(); }
          }
          else
          {
            break;
          }
        }

        checksum += dataBuf[i+3];;

        if( checksum == 0 )
        {
          vdStatus.cmd_status = 0;
        }
        else
        {
          vdStatus.status = SPISLAVE_CHKSUM_ERR;  // Checksum error

          DBGA_PRINTF( "Checksum Error: %02X\n\r", checksum );
        }

        // vdData.filePos += i;
        // if( i < dataLen ) { dataLen = i; }

        // vdData.dataLen += dataLen;

        // DBGA_PRINTF( "WRITE - vdData.filePos: %i, dataLen: %i, numOfBytes: %i\n", vdData.filePos, vdData.dataLen, dataLen );

        // // Check for buffer end
        // if( vdData.filePos == sizeof(vdData.data) )
        // {
        //   // Serial.println( "Data to write sent to server" );

        //   // Copy data to packet buffer
        //   memcpy( vd.packet.data, vdData.data, sizeof(vd.packet.data) );
        //   vd.packet.dataLen = vdData.dataLen;
        //   vdData.filePos = 0;

        //   // // Dump packet
        //   // Serial.println( "Packet dump\n" );
        //   // for( uint16_t addr = 0; addr < sizeof(vd.packet.data); addr++ )
        //   // {
        //   //   Serial.printf( "%02X ", vd.packet.data[addr] );

        //   //   if( ( ( (addr + 1) % 16 ) == 0 ) && ( addr != sizeof(vd.packet.data) ) )
        //   //   {
        //   //     Serial.println();
        //   //   }
        //   // }
        //   // Serial.println();

        //   DBGA_PRINTLN( "WifiClient write data" );

        //   // Send data to server
        //   tcpClient.write( vd.rawData, sizeof(vd.rawData) );
        //   tcpClient.flush();

        //   // Receive data from server - dummy read
        //   if( waitForTcpData() )
        //   {
        //     tcpClient.read( vd.rawData, sizeof(vd.rawData) );

        //     if( vd.packet.status == VD_STATUS_OK )
        //     {
        //       DBGA_PRINTLN( "WifiClient write data - Status OK" );
        //     }
        //   }


        //   vdData.dataLen = 0;
        // }

        DBGA_PRINTLN( "WRITE - Set status" );

        // Set status and command status
        // vdStatus.cmd_status = 0;        // Set status \todo Fill in correct status
        // vdStatus.cmd_data   = dataLen;  // Set number of bytes
        // vdStatus.status     = SPISLAVE_READY;
        SPISlave.setStatus( vdStatus.rawStatus );
      break;

      case VD_CMD_WR_NEXT:
        DBGA_PRINTLN( "VD_CMD_WR_NEXT" );

        vdStatus.rawStatus = SPI_STATUS_RESET;

        // Debug
        DBGS_PRINT( "Rec. SPI data: " );
        dumpSpiPacket( dataBuf );

        numOfBytes = dataBuf[1];

        // Init checksum
        checksum = cmd;

        // Calculate checksum
        checksum += numOfBytes;
        checksum += dataBuf[2];

        if( checksum == 0 )
        {
          vdData.filePos += numOfBytes;
          vdData.dataLen += numOfBytes;

          DBGA_PRINTF( "WRITE - vdData.filePos: %i, dataLen: %i, numOfBytes: %i\n", vdData.filePos, vdData.dataLen, dataLen );

          // Check for buffer end
          if( vdData.filePos == sizeof(vdData.data) )
          {
            // Serial.println( "Data to write sent to server" );

            // Copy data to packet buffer
            memcpy( vd.packet.data, vdData.data, sizeof(vd.packet.data) );
            vd.packet.dataLen = vdData.dataLen;
            vdData.filePos = 0;

            // // Dump packet
            // Serial.println( "Packet dump\n" );
            // for( uint16_t addr = 0; addr < sizeof(vd.packet.data); addr++ )
            // {
            //   Serial.printf( "%02X ", vd.packet.data[addr] );

            //   if( ( ( (addr + 1) % 16 ) == 0 ) && ( addr != sizeof(vd.packet.data) ) )
            //   {
            //     Serial.println();
            //   }
            // }
            // Serial.println();

            DBGA_PRINTLN( "WifiClient write data" );

            // Send data to server
            tcpClient.write( vd.rawData, sizeof(vd.rawData) );
            tcpClient.flush();

            // Receive data from server - dummy read
            if( waitForTcpData() )
            {
              tcpClient.read( vd.rawData, sizeof(vd.rawData) );

              if( vd.packet.status == VD_STATUS_OK )
              {
                DBGA_PRINTLN( "WifiClient write data - Status OK" );
              }
            }

            vdData.dataLen = 0;

            vdStatus.cmd_status = 0;
          }
        }
        else
        {
          vdStatus.status = SPISLAVE_CHKSUM_ERR;  // Checksum error

          DBGA_PRINTF( "Checksum Error: %02X\n\r", checksum );
        }

        // Set status and command status
        vdStatus.cmd_data   = numOfBytes;   // Set number of bytes
        // vdStatus.status     = SPISLAVE_READY;
        SPISlave.setStatus( vdStatus.rawStatus );
      break;

      case VD_CMD_SEEK_FILE:
        DBGS_PRINTLN( "VD_CMD_SEEK_FILE" );

        vdStatus.rawStatus = SPI_STATUS_RESET;

        DBGS_PRINT( "Rec. SPI data: " );
        dumpSpiPacket( dataBuf );

        // Init checksum
        checksum = cmd;

        uint32_t fileOffset;
        fileOffset = ((uint32_t)dataBuf[1] << 24) | ((uint32_t)dataBuf[2] << 16) | ((uint32_t)dataBuf[3] << 8) | dataBuf[4];

        // Calculate checksum (data + checksum)
        for( i = 1; i <= 5; i++ )
        {
          checksum += dataBuf[i];
        }

        if( checksum == 0 )
        {
          DBG_PRINTF( "VD_CMD_SEEK_FILE - Offset: %lu\n\r", fileOffset );

          // lastSeek = fileOffset;  // Debug

          // Fill packet for server
          vd.packet.cmd = VD_CMD_SEEK_FILE;
          memcpy( vd.packet.filename, vdData.filename, sizeof(vd.packet.filename) );
          vd.packet.fileOffset = fileOffset;

          // Send data to server
          tcpClient.write( vd.rawData, sizeof(vd.rawData) );
          tcpClient.flush();

          // Receive data from server
          // if( tcpClient.available() )
          if( waitForTcpData() )
          {
            DBGA_PRINTLN( "WifiClient seek file" );

            tcpClient.read( vd.rawData, sizeof(vd.rawData) );

            // Serial.printf( "Status from Server: %i\n", vd.packet.status );

            vdStatus.cmd_status = 0;
          }
          else
          {
            DBGA_PRINTLN( "WifiClient seek file - ERROR" );

            vdStatus.cmd_status = 1;  // Error
          }
        }
        else
        {
          vdStatus.status = SPISLAVE_CHKSUM_ERR;  // Checksum error

          DBGA_PRINTF( "Checksum Error: %02X\n\r", checksum );
        }

        vdData.dataLen = 0;   // New for VD_CMD_WR_FILE
        prevCmd = VD_CMD_NONE;

        // Set status and command status
        // vdStatus.status = SPISLAVE_READY;
        SPISlave.setStatus( vdStatus.rawStatus );
      break;

      default:
        DBG_PRINTLN( "default" );

        // Dump packet
        Serial.println( "SPI Packet dump" );
        for( uint16_t addr = 0; addr < 32; addr++ )
        {
          Serial.printf( "%02X ", dataBuf[addr] );

          // if( ( ( (addr + 1) % 16 ) == 0 ) && ( addr != 32 ) )
          // {
          //   Serial.println();
          // }
        }
        Serial.println();

        prevCmd = VD_CMD_NONE;
      break;
    }

    spiDataRcvd = false;
  }
}
