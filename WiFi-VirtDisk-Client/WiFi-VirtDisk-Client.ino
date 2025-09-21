/***************************************************************************//**
 * @file    WiFi-VirtDisk-Client.cpp
 *
 * @brief   The WiFi Virtual Disk Client for ESP8266 with SPI Slave.
 *
 * @note    Connect the SPI Master device to the following pins:
 *
 *                    ESP8266
 *            GPIO    NodeMCU   Name
 *          ===========================
 *             15       D8       SS
 *             13       D7      MOSI
 *             12       D6      MISO
 *             14       D5      SCK
 *
 * @note    If D8 is high at boot up, the ESP8266 didn't boot from flash.
 *          (https://github.com/JiriBilek/WiFiSpiESP/issues/6)
 *
 * @copyright   Copyright (c) 2025 by Welzel-Online
 ******************************************************************************/


/****************************************************************** Includes **/
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <Ticker.h>
#include <DoubleResetDetector.h>
#include <EEPROM.h>

#include "SPISlave.h"
#include "SPICallbacks.h"
#include "virtDisk.hpp"
#include "debug.h"


/******************************************************************* Defines **/
const char* VD_VERSION = "0.9.0";

const uint8_t SS_ENABLE_PIN = D1;   // PIN for circuit blocking SS to GPIO15 on reset 
const uint8_t Z80_RESET_PIN = D3;
const uint8_t Z80_USER_PIN  = D4;

#define EEPROM_SIZE       512
#define EEPROM_CONFIG_ADR 0

#define VD_CONFIG_VERSION 1

struct vdConfig_t
{
  uint8_t version      = 0;   // Config version
  char    vdServer[64] = "";  // VirtDisk Server Hostname or IP-Address
  char    vdPort[5]    = "";  // VirtDisk Server Port
};

const char* VD_DEFAULT_PORT = "12345";
bool vdSave = false;          // Flag for saving the configuration

extern vdStatus_t vdStatus;

// Number of seconds after reset during which a 
// subseqent reset will be considered a double reset.
#define DRD_TIMEOUT 10

// RTC Memory Address for the DoubleResetDetector to use
#define DRD_ADDRESS 0

typedef enum
{
  SRV_CONNECTED = 0x01,
  SRV_DISCONNECTED = 0x02,
  DBG_SRV_CONNECTED = 0x04,
  DBG_SRV_DISCONNECTED = 0x08
} srvStatus_t;


/********************************************************** Global Variables **/
WiFiManager wifiManager;  // The WiFi Manager
WiFiClient  tcpClient;    // WiFi Client for communication
WiFiClient  dbgClient;    // WiFi Client for debugging
vdConfig_t  vdConfig;     // VirtDisk configuration
Ticker      timer;        // Timer for status LED (Startup: 250ms, Config: 100ms, Up: On)
DoubleResetDetector drd( DRD_TIMEOUT, DRD_ADDRESS );

uint8_t wifiStatus;
uint8_t tcpSrvStatus = SRV_DISCONNECTED;
// uint8_t dbgSrvStatus = SRV_DISCONNECTED;


/******************************************************* Functions / Methods **/

/***************************************************************************//**
 * @brief   Periodic tick to toggle the status LED.
 ******************************************************************************/
void onTick( void )
{
    int state = digitalRead( LED_BUILTIN ); // get the current state of LED
    digitalWrite( LED_BUILTIN, !state );    // set pin to the opposite state
}


/***************************************************************************//**
 * @brief   WiFiManager Callback: Configuration mode enter.
 *
 * @param   WiFiManager Pointer to the WiFiManager instance.
 ******************************************************************************/
void wm_configModeCB( WiFiManager *myWiFiManager )
{
  DBG_PRINTLN( "Entered WiFiManager config mode" );

  timer.attach_ms( 100, onTick );
}


/***************************************************************************//**
 * @brief   WiFiManager Callback: Save configuration.
 *
 * @param   WiFiManager Pointer to the WiFiManager instance.
 ******************************************************************************/
void wm_saveConfigCB( void )
{
  DBG_PRINTLN( "WiFi credentials saved." );

  timer.attach_ms( 250, onTick );

  vdSave = true;
}


/***************************************************************************//**
 * @brief   This function initializes all the stuff.
 ******************************************************************************/
void setup( void )
{
  // Initalize the LED
  pinMode( LED_BUILTIN, OUTPUT );
  digitalWrite( LED_BUILTIN, HIGH );  // LED off

  // Enable SS signal to D5 (GPIO15) (https://github.com/JiriBilek/WiFiSpiESP/issues/6)
  pinMode( SS_ENABLE_PIN, OUTPUT );
  digitalWrite( SS_ENABLE_PIN, HIGH );  


  // --- Setting callbacks for SPI protocol

  // --- onData
  // Data has been received from the master. Beware that len is always 32
  // and the buffer is autofilled with zeroes if data is less than 32 bytes long
  SPISlave.onData( SPIOnData );

  // --- onDataSent
  // The master has read out outgoing data buffer
  // that buffer can be set with SPISlave.setData
  // SPISlave.onDataSent( SPIOnDataSent );

  // --- onStatus
  // Status has been received from the master.
  // The status register is a special register that both the slave and the master can write to and read from.
  // Can be used to exchange small data or status information
  SPISlave.onStatus( SPIOnStatus );

  // --- onStatusSent
  // The master has read the status register
  SPISlave.onStatusSent( SPIOnStatusSent );

  // Setup SPI Slave registers and pins
  SPISlave.begin();

  // Receiver and transmitter state
  vdStatus.rawStatus = SPI_STATUS_RESET;
  vdStatus.status = SPISLAVE_BUSY;
  SPISlave.setStatus( vdStatus.rawStatus );
  // SPISlave.setStatus( SPISLAVE_BUSY );


  // Start the LED timer
  timer.attach_ms( 250, onTick );

  // Needed for Debug Server
  pinMode( Z80_RESET_PIN, OUTPUT );
  digitalWrite( Z80_RESET_PIN, LOW );
  pinMode( Z80_USER_PIN, OUTPUT );
  digitalWrite( Z80_USER_PIN, LOW );

  // Read VirtDisk configuration
  EEPROM.begin( EEPROM_SIZE );
  EEPROM.get( EEPROM_CONFIG_ADR, vdConfig );

  // Serial line for debugging
  Serial.begin( 115200 );

  // delay( 5000 );

  Serial.println();
  Serial.print( "WiFi-VirtDisk Client v" );
  Serial.println( VD_VERSION );

  // The Wifi is started by the WifiManager
  WiFi.mode( WIFI_OFF );
  WiFi.persistent( true );
  
  // WiFiManager configuration
  wifiManager.setDebugOutput( false );
  wifiManager.setTitle( "VirtDisk Configuration" );
  wifiManager.setHostname( "WiFi-VirtDisk-Client" );
  wifiManager.setAPCallback( wm_configModeCB );
  wifiManager.setSaveConfigCallback( wm_saveConfigCB );
  wifiManager.setShowInfoErase( true );
  // wifiManager.setCaptivePortalEnable( true );
  // wifiManager.setParamsPage( true );

  // Additional parameters for the VirtDisk-Server
  WiFiManagerParameter custom_vdServer( "vdserver", "WiFi-VirtDisk Server", vdConfig.vdServer, sizeof(vdConfig.vdServer) );
  wifiManager.addParameter( &custom_vdServer );
  WiFiManagerParameter custom_vdPort( "vdport", "WiFi-VirtDisk Port", vdConfig.vdPort, sizeof(vdConfig.vdPort) );
  wifiManager.addParameter( &custom_vdPort );

  // Debug
  // wifiManager.resetSettings();

  #define START_CONFIG 0
  if( drd.detectDoubleReset() || START_CONFIG )
  {
    DBG_PRINTLN( "WiFi-Manager Config Portal" );

    // Start WiFiManager config portal
    wifiManager.startConfigPortal( "WiFi-VirtDisk Client AP" );
  }

  // Check, if we should save the VirtDisk parameters
  if( vdSave )
  {
    // vdConfig.version = VD_CONFIG_VERSION;
    // strncpy( vdConfig.vdServer, custom_vdServer.getValue(), sizeof(vdConfig.vdServer) );
    // // Check for valid port number
    // if( atoi( custom_vdPort.getValue() ) != 0 )
    // {
    //   strncpy( vdConfig.vdPort, custom_vdPort.getValue(), sizeof(vdConfig.vdPort) );
    // }
    // else
    // {
    //   strncpy( vdConfig.vdPort, VD_DEFAULT_PORT, sizeof(vdConfig.vdPort) );
    // }
    

    // EEPROM.put( EEPROM_CONFIG_ADR, vdConfig );
    // EEPROM.commit();

    vdSave = false;

    timer.attach_ms( 250, onTick );
  }

  DBG_PRINTLN( "WiFi-Manager Auto Connect" );

  // Try to auto connect
  wifiManager.autoConnect( "WiFi-VirtDisk Client AP" );

  // WiFi connected successfully
  timer.detach();
  digitalWrite( LED_BUILTIN, LOW ); // turn LED on

  if( tcpClient.connect( vdConfig.vdServer, atoi(vdConfig.vdPort) ) )
  {
    Serial.println( F("Connected to WiFi-VirtDisk Server") );
  }
  else
  {
    DBG_PRINTLN( "Connection to WiFi-VirtDisk Server failed" );
  }

  if( dbgClient.connect( vdConfig.vdServer, atoi(vdConfig.vdPort)+1 ) )
  {
    Serial.println( F("Connected to WiFi-VirtDisk Debug Server.") );
  }
  else
  {
    DBG_PRINTLN( "Connection to WiFi-VirtDisk Debug Server failed" );
  }


  // SPI Receiver and transmitter state
  vdStatus.rawStatus = SPI_STATUS_RESET;
  vdStatus.status = SPISLAVE_READY;
  SPISlave.setStatus( vdStatus.rawStatus );
}


/***************************************************************************//**
 * @brief   This is the main loop of the Arduino SDK.
 ******************************************************************************/
void loop( void )
{
  static uint8_t  oldWifiStatus = WL_NO_SHIELD; 
  static uint16_t vdPort = atoi(vdConfig.vdPort);


  // Check WiFi status
  wifiStatus = WiFi.status();

  if( wifiStatus == WL_CONNECTED )
  {
    // Handle SPI and WiFi-VirtDisk communication
    if( tcpClient.connected() )
    {
      if( ( tcpSrvStatus & SRV_DISCONNECTED ) == SRV_DISCONNECTED )
      {
        DBG_PRINTLN( "Connected to WiFi-VirtDisk Server"); 
      }

      tcpSrvStatus &= ~SRV_DISCONNECTED;
      tcpSrvStatus |= SRV_CONNECTED;

      vdProcessCmd();
    }
    else
    {
      tcpSrvStatus &= ~SRV_CONNECTED;
      tcpSrvStatus |= SRV_DISCONNECTED;

      // Try to reconnect to the WiFi-VirtDisk Server
      DBG_PRINTLN( "Lost connection to WiFi-VirtDisk Server. Try to reconnect..." );
      if( tcpClient.connect( vdConfig.vdServer, vdPort ) )
      {
        DBG_PRINTLN( "Reconnected to Server" );
      } 
      else 
      {
        DBG_PRINTLN( "Reconnection to WiFi-VirtDisk Server failed" );
        delay(2000);  // Wait 2 seconds before trying again.
      }
    }  
    

    // Handle Debug communication
    if( dbgClient.connected() )
    {
      if( ( tcpSrvStatus & DBG_SRV_DISCONNECTED ) == DBG_SRV_DISCONNECTED )
      {
        DBG_PRINTLN( "Connected to WiFi-VirtDisk Debug Server"); 
      }
      
      tcpSrvStatus &= ~DBG_SRV_DISCONNECTED;
      tcpSrvStatus |= DBG_SRV_CONNECTED;

      // Receive data
      if( dbgClient.available() ) 
      {
        // Serial.println( F("DebugClient read data") );

        char dbgBuffer[10];
        dbgClient.read( dbgBuffer, sizeof(dbgBuffer) );
        
        Serial.printf( "Debug command from Server: %c\n\r", dbgBuffer[0] );

        switch( dbgBuffer[0] )
        {
          case 'R':
            // Reset the Z80-MBC2
            digitalWrite( Z80_RESET_PIN, HIGH );
            delay(200);
            digitalWrite( Z80_RESET_PIN, LOW );
          break;

          case 'U':
            // Press the user button and reset the Z80-MBC2
            digitalWrite( Z80_USER_PIN, HIGH );
            delay(200);
            digitalWrite( Z80_RESET_PIN, HIGH );
            delay(200);
            digitalWrite( Z80_RESET_PIN, LOW );
            delay(2000);
            digitalWrite( Z80_USER_PIN, LOW );
          break;

          default:
          break;
        }
      }
    }
    else
    {
      tcpSrvStatus &= ~DBG_SRV_CONNECTED;
      tcpSrvStatus |= DBG_SRV_DISCONNECTED;

      // Try to reconnect to the WiFi-VirtDisk Server
      DBG_PRINTLN( "Lost connection to WiFi-VirtDisk Debug Server. Try to reconnect..." );
      if( dbgClient.connect( vdConfig.vdServer, vdPort+1 ) )
      {
        DBG_PRINTLN( "Reconnected to Debug Server" );
      } 
      else 
      {
        DBG_PRINTLN( "Reconnection to WiFi-VirtDisk Debug Server failed" );
        delay(2000);  // Wait 2 seconds before trying again.
      }
    }  
  }  
  else
  {
    // Try to auto re-connect
    wifiManager.autoConnect( "WiFi-VirtDisk Client AP" );
  }


  if( oldWifiStatus != wifiStatus )
  {
    oldWifiStatus = wifiStatus;

    switch( wifiStatus )
    {
      case WL_CONNECTED:
        Serial.print( F("WiFi: Connected to: ") );
        Serial.println( WiFi.SSID().c_str() );
      break;
      
      case WL_NO_SSID_AVAIL:
        Serial.println( F("WiFi: Configured SSID not found") );
      break;
      
      case WL_CONNECT_FAILED:
        Serial.println( F("WiFi: Connection failed") );
      break;
      
      case WL_IDLE_STATUS:
        Serial.println( F("WiFi: Module in idle mode") );
      break;
      
      case WL_DISCONNECTED:
        Serial.println( F("WiFi: Not connected") );
      break;
      
      default:
        Serial.println( F("WiFi: Unknown status") );
      break;
    }
  }

  // Call the double reset detector loop method every so often,
  // so that it can recognise when the timeout expires.
  // You can also call drd.stop() when you wish to no longer
  // consider the next reset as a double reset.
  drd.loop();
}
