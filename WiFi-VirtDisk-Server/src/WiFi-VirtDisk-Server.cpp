/***************************************************************************//**
 * @file    WiFi-VirtDisk-Server.cpp
 *
 * @brief   Main file of the WiFi-VirtDisk-Server.
 *
 * @copyright   Copyright (c) 2025 by Welzel-Online
 ******************************************************************************/


/****************************************************************** Includes **/
#include <cstdint>
#include <string>

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <filesystem>

#if defined(__linux__)
#include <unistd.h>
#include <poll.h>
#include <termios.h>
#endif

// Socket-CPP
#include "TCPServer.h"

// CP/M Tools
#include "config.h"
#include "cpmtools/cpmfs.h"

// LibDsk
#include <stddef.h>
#include <libdsk.h>

// SimpleIni
#include "SimpleIni/SimpleIni.h"
// ArgParse
#include "argparse.h"

#include "helper.h"
#include "message.h"
#include "input.h"
#include "virtDisk.hpp"
#include "version.h"


/******************************************************************* Defines **/
// Log-Printers for socket-cpp
// #define LOG_PRINTER = [](const std::string& strLogMsg) { std::cout << strLogMsg << std::endl; }
auto LogPrinter = [](const std::string& strLogMsg) { std::cout << strLogMsg << std::endl; };


/********************************************************** Global Variables **/
bool gSrvRunning = true;
ASocket::Socket gCloseTcpSocket = INVALID_SOCKET;
ASocket::Socket gCloseDbgSocket = INVALID_SOCKET;

std::mutex gMutex;
std::condition_variable gCv;
char gDbgCmd = ' ';
bool gDbgDataReady = false;


// Configuration data
std::string serverPort    = "12345";    // WiFi-VirtDisk Portnummer
std::string dbgServerPort = "12346";    // Debug Server Portnummer
std::string filePath      = "D:/Projekte/WiFi-VirtDisk/WiFi-VirtDisk-Server/testData/files/";

std::vector<std::string> diskEmuPath;
std::vector<std::string> diskEmuFilename;
std::vector<std::string> diskEmuFormat;

std::string defaultDiskEmuPath      = "D:/Projekte/WiFi-VirtDisk/WiFi-VirtDisk-Server/testData/disk/";
std::string defaultDiskEmuFilename  = "DS0N00.DSK";
std::string defaultDiskEmuFormat    = "z80mbc2-d0";

extern vdData_t vdData;
extern struct cpmSuperBlock drive;


/******************************************************* Functions / Methods **/
/***************************************************************************//**
 * @brief   Reads the configuration file and sets the global variables.
 *
 * @return  true    if the configuration file was read successfully,
 *                  otherwise false.
 ******************************************************************************/
bool readConfig( void )
{
    bool retVal = true;
    CSimpleIniA vdIni;
    std::string exeDir = getExeDir();

	vdIni.SetUnicode();


    if( std::filesystem::exists( exeDir + "/.WiFi-VirtDisk" ) )
    {
        message( MsgType::INFO, "Using configuration file: .WiFi-VirtDisk" );

        // Load configuration file
        SI_Error rc = vdIni.LoadFile( std::string(exeDir + "/.WiFi-VirtDisk").c_str() );
        if( rc < 0 )
        {
            message( MsgType::ERR, "Error loading configuration file" );
            return 1;
        }

        // Get server port from configuration file
        const char* serverPortIni = vdIni.GetValue( "WiFi-VirtDisk", "serverPort", serverPort.c_str() );
        if( serverPortIni != nullptr )
        {
            serverPort = serverPortIni;
            dbgServerPort = std::to_string(std::stoi(serverPort) + 1);  // Use the next port number
            message( MsgType::INFO, "Server port: " + serverPort + ", Debug Server port: " + dbgServerPort );
        }

        // Get file path from configuration file
        const char* filePathIni = vdIni.GetValue( "WiFi-VirtDisk", "filePath", filePath.c_str() );
        if( filePathIni != nullptr )
        {
            filePath = filePathIni;
            message( MsgType::INFO, "File path: " + filePath );
        }

        // Get number of emulated disks and parameters from configuration file
        int diskNum = 0;
        do
        {
            // Get section name
            std::string section = "EmuDisk" + std::to_string(diskNum);

            // Get the disk emulation file name from configuration file
            const char* diskEmuFilenameIni = vdIni.GetValue( section.c_str(), "diskEmuFilename", nullptr );
            // Get disk path from configuration file
            const char* diskEmuPathIni = vdIni.GetValue( section.c_str(), "diskEmuPath", nullptr );
            // Get disk format from configuration file
            const char* diskEmuFormatIni = vdIni.GetValue( section.c_str(), "diskEmuFormat", nullptr );

            if( diskEmuFilenameIni != nullptr && diskEmuPathIni != nullptr && diskEmuFormatIni != nullptr )
            {
                diskEmuFilename.push_back( std::string(diskEmuFilenameIni) );
                diskEmuPath.push_back( std::string(diskEmuPathIni) );
                diskEmuFormat.push_back( std::string(diskEmuFormatIni) );

                message( MsgType::INFO, "Emulated disk " + std::to_string(diskNum) + ": " + diskEmuFilename.back() + " (" + diskEmuFormat.back() + ")\r\n" +
                                        "                       " + diskEmuPath.back() );
                // message( MsgType::INFO, "           Path: " + diskEmuPath.back() );
            }
            // else
            // {
            //     message( MsgType::ERR, "Incomplete configuration for emulated disk " + std::to_string(diskNum) + ", skipping" );
            // }
        } while( ++diskNum < 4 ); // Currently only 4 emulated disks are supported
    }
    else
    {
        message( MsgType::INFO, "No configuration file found, using default settings" );
        retVal = false;
    }
    std::cout << std::endl;

    return retVal;
}


/***************************************************************************//**
 * @brief   Returns the client IP address of the given socket.
 *
 * @param   clientSocket    The current client socket.
 * @param   clientIP        The character array for the IP address.
 * @param   clientPort      The integer for the client port.
 *
 * @return  true if the IP address could be resolved, otherwise false.
 ******************************************************************************/
bool getClientIP( ASocket::Socket clientSocket, char* clientIP, int* clientPort )
{
    struct sockaddr_in clientAddress;
    socklen_t          clientAddressLength = sizeof(clientAddress);
    int                nativeSocket        = static_cast<int>(clientSocket);  // Cast ASocket::Socket to a native socket descriptor
    bool               retVal              = false;


    // Get client address
    if( getpeername( nativeSocket, (struct sockaddr*)&clientAddress, &clientAddressLength ) == 0 )
    {
        inet_ntop( AF_INET, &clientAddress.sin_addr, clientIP, INET_ADDRSTRLEN) ;
        *clientPort = ntohs( clientAddress.sin_port );
        retVal = true;
    }

    return retVal;
}


/***************************************************************************//**
 * @brief   Process the debug client request.
 *
 * @param   clientSocket    The current client socket.
 * @param   server          The TCPServer for communication.
 * @param   clientInfo      IP address and port of the connected client.
 ******************************************************************************/
void handleDbgClient( ASocket::Socket clientSocket, CTCPServer* server, std::string clientInfo )
{
    const int BUFFER_SIZE = 10;
    char buffer[BUFFER_SIZE] = {};
    int bytesReceived;
    int ret;


    int flag = 1;
    setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));

    server->SetRcvTimeout( clientSocket, 100 );

    while( gSrvRunning )
    {
        std::unique_lock<std::mutex> lock( gMutex);

        // The thread sleeps here until gDbgDataReady is true.
        // gCv.wait releases the lock while it sleeps,
        // and locks it again before waking up.
        gCv.wait(lock, [] { return gDbgDataReady; });

        if( ( gCloseDbgSocket != INVALID_SOCKET ) && ( gCloseDbgSocket == clientSocket ) )
        {
            message( MsgType::INFO, "Old Debug connection closed (" + clientInfo + ")" );
            gCloseDbgSocket = INVALID_SOCKET;   // Reset the close socket
            break;
        }

        switch( gDbgCmd )
        {
            case 'R':
                // Reset the Z80-MBC2
                message( MsgType::INFO, "Resetting the Z80-MBC2" );
                buffer[0] = 'R';
                buffer[1] = '\0';
                server->Send( clientSocket, buffer, sizeof(buffer) );
                gDbgCmd = ' ';
            break;

            case 'U':
                // Press the user button and reset of the Z80-MBC2
                message( MsgType::INFO, "Press user button and reset of the Z80-MBC2" );
                buffer[0] = 'U';
                buffer[1] = '\0';
                server->Send( clientSocket, buffer, sizeof(buffer) );
                gDbgCmd = ' ';
            break;

            default:
            break;
        }

        // Reset flag to wait for the next key
        gDbgDataReady = false;
    }

    // Close connection to client
    server->Disconnect( clientSocket );
}


/***************************************************************************//**
 * @brief   Process the client request.
 *
 * @param   clientSocket    The current client socket.
 * @param   server          The TCPServer for communication.
 * @param   clientInfo      IP address and port of the connected client.
 ******************************************************************************/
void handleTcpClient( ASocket::Socket clientSocket, CTCPServer* server, std::string clientInfo )
{
    const int BUFFER_SIZE = 1024;
    char buffer[BUFFER_SIZE] = {};
    int bytesReceived;
    int ret;

    int flag = 1;
    setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));

    server->SetRcvTimeout( clientSocket, 100 );

    while( gSrvRunning )
    {
        if( ( gCloseTcpSocket != INVALID_SOCKET ) && ( gCloseTcpSocket == clientSocket ) )
        {
            message( MsgType::INFO, "Old connection closed (" + clientInfo + ")" );
            gCloseTcpSocket = INVALID_SOCKET;   // Reset the close socket
            break;
        }

        bytesReceived = server->Receive( clientSocket, buffer, BUFFER_SIZE, false );

        if( bytesReceived > 0 )
        {
            //std::cout << "Bytes received from ESP8266: " << bytesReceived << std::endl;

            ret = vdProcessCmd( buffer );
            if( ret == 0 )
            {
                //std::cout << "Sending response to ESP8266: " << sizeof(vdPacket_t) << std::endl;

                server->Send( clientSocket, buffer, sizeof(vdPacket_t) );
            }
            else
            {
                //std::cout << "Error processing command from ESP8266" << std::endl;
            }
        }
        else if( bytesReceived == 0 )
        {
            // Client closed the connection
            message( MsgType::INFO, "Client disconnected (" + clientInfo + ")" );
            break;
        }

        genSleep( 10 ); // Sleep for 10ms to avoid busy waiting
    }

    // Close connection to client
    server->Disconnect( clientSocket );
}


/***************************************************************************//**
 * @brief   The main function of WiFi-VirtDisk-Server.
 *          This function takes the command line arguments and the configuration
 *          file and serves the virtual disks via TCP.
 *
 * @param   argc The number of arguments contained in argv[].
 * @param   argv The passing parameters. Description in printUsage()
 *
 * @return  0 if everything is okay. -1 in case of general error.
 ******************************************************************************/
int main( int argc, char* argv[] )
{
    std::vector<std::thread> tcpClientThreads;
    std::vector<std::thread> dbgClientThreads;
    ASocket::Socket tcpClient;
    ASocket::Socket oldTcpClient = INVALID_SOCKET;
    ASocket::Socket dbgClient;
    ASocket::Socket oldDbgClient = INVALID_SOCKET;

    int    key;
    bool   isSpecial;


    // Print status message
    std::cout << std::endl;
    if( isColorTerm() ) { std::cout << COLOR_YELLOW; }
    std::cout << "WiFi-VirtDisk Server v" << WIFI_VIRTDISK_SERVER_REVISION;
    std::cout << " - Copyright (c) 2025 by Welzel-Online" << std::endl;
    std::cout << "Using LibDsk v" << LIBDSK_VERSION << ", CP/M Tools v" << CPMTOOLS_VERSION << " and SimpleIni v" << SIMPLEINI_VERSION << std::endl << std::endl;
    if( isColorTerm() ) { std::cout << COLOR_GREEN; }
    std::cout << "'H' for help, 'Q' for quit" << std::endl << std::endl;
    if( isColorTerm() ) { std::cout << COLOR_NORM; }

    // Read configuration file
    readConfig();

    // // Test function
    // test();
    // return 0;


    // Create WiFi-VirtDisk Server
    CTCPServer tcpServer( LogPrinter, serverPort, (ASocket::SettingsFlag)0 );
    try
    {
        message( MsgType::INFO, "WiFi-VirtDisk Server started, listening on port " + serverPort );
    }
    catch( const std::exception& e)
    {
        message( MsgType::ERR, "Error creating WiFi-VirtDisk server: " + std::string(e.what()) );
        return 1;
    }


    // Create Debug Server
    CTCPServer dbgServer( LogPrinter, dbgServerPort, (ASocket::SettingsFlag)0 );
    try
    {
        message( MsgType::INFO, "Debug Server started, listening on port " + dbgServerPort );
        if( isColorTerm() ) { std::cout << COLOR_GREEN; }
        std::cout << "'R' for reset the Z80-MBC2, 'U' for user button and reset" << std::endl << std::endl;
        if( isColorTerm() ) { std::cout << COLOR_NORM; }
    }
    catch( const std::exception& e)
    {
        message( MsgType::ERR, "Error creating Debug server: " + std::string(e.what()) );
        return 1;
    }


    // Main loop of server
    while( gSrvRunning )
    {
        // WiFi-VirtDisk - Wait for incomming connection, timeout 250ms
        if( tcpServer.Listen( tcpClient, 250 ) )
        {
            if( oldTcpClient == INVALID_SOCKET )
            {
                oldTcpClient = tcpClient;       // Save the first client socket
            }
            else if( oldTcpClient != tcpClient )
            {
                gCloseTcpSocket = oldTcpClient; // Set the socket to close
                oldTcpClient    = tcpClient;    // Update the old client socket
            }

            std::string clientInfo = "IP could not be resolved";
            MsgType ipMsg = MsgType::WARN;
            char clientIP[INET_ADDRSTRLEN] = {0};
            int  clientPort = 0;
            if( getClientIP( tcpClient, clientIP, &clientPort ) )
            {
                clientInfo = std::string(clientIP) + ":" + std::to_string(clientPort);
                ipMsg = MsgType::INFO;
            }
            message( ipMsg, "Client connected (" + clientInfo + ")" );

            // Start new thread for client connection handling
            message( MsgType::INFO, "Client thread created" );
            tcpClientThreads.emplace_back( handleTcpClient, tcpClient, &tcpServer, clientInfo );
        }


        // Debug Server - Wait for incomming connection, timeout 250ms
        if( dbgServer.Listen( dbgClient, 250 ) )
        {
            if( oldDbgClient == INVALID_SOCKET )
            {
                oldDbgClient = dbgClient;       // Save the first client socket
            }
            else if( oldDbgClient != dbgClient )
            {
                gCloseDbgSocket = oldDbgClient; // Set the socket to close
                gDbgDataReady   = true;         // Set the data ready flag to wake up the worker thread
                gCv.notify_one();               // Wake up the sleeping worker thread
                oldDbgClient    = dbgClient;    // Update the old client socket
            }

            std::string clientInfo = "IP could not be resolved";
            MsgType ipMsg = MsgType::WARN;
            char clientIP[INET_ADDRSTRLEN] = {0};
            int  clientPort = 0;
            if( getClientIP( dbgClient, clientIP, &clientPort ) )
            {
                clientInfo = std::string(clientIP) + ":" + std::to_string(clientPort);
                ipMsg = MsgType::INFO;
            }
            message( ipMsg, "Client connected to Debug Server (" + clientInfo + ")" );

            // Start new thread for client connection handling
            message( MsgType::INFO, "Debug Client thread created" );
            dbgClientThreads.emplace_back( handleDbgClient, dbgClient, &dbgServer, clientInfo );
        }


        // Keyboard handling
        if( isKeyPressed( &key, &isSpecial ) )
        {
            if( isSpecial )
            {
                std::cout << "Special-Key: #" << key << " - " << (char)toupper(key) << std::endl;
            }
            else
            {
                switch( toupper(key) )
                {
                    case 'H':
                        if( isColorTerm() ) { std::cout << COLOR_GREEN; }
                        std::cout << "'H' for help, 'Q' for quit" << std::endl;
                        std::cout << "'L' for re-load the disk image" << std::endl;
                        std::cout << "'R' for reset the Z80-MBC2, 'U' for user button and reset" << std::endl << std::endl;
                        if( isColorTerm() ) { std::cout << COLOR_NORM; }
                    break;

                    case 'Q':
                        // Stop the server
                        message( MsgType::INFO, "Key stroke: 'Q'" );
                        message( MsgType::INFO, "Stopping server..." );
                        gSrvRunning = false;
                    break;

                    case 'R':
                        // Reset the Z80-MBC2
                        message( MsgType::INFO, "Key stroke: 'R'" );

                        // Scope for the lock so that it is quickly released again
                        {
                            std::lock_guard<std::mutex> lock( gMutex);
                            gDbgCmd = 'R';
                            gDbgDataReady = true;
                        } // Mutex is automatically released here

                        // Wake up the sleeping worker thread
                        gCv.notify_one();
                    break;

                    case 'U':
                        // Press the user button and reset the Z80-MBC2
                        message( MsgType::INFO, "Key stroke: 'U'" );

                        // Scope for the lock so that it is quickly released again
                        {
                            std::lock_guard<std::mutex> lock( gMutex);
                            gDbgCmd = 'U';
                            gDbgDataReady = true;
                        } // Mutex is automatically released here

                        // Wake up the sleeping worker thread
                        gCv.notify_one();
                    break;

                    case 'L':
                        // Re-load the disk image
                        message( MsgType::INFO, "Key stroke: 'L'" );

                        if( vdReloadDiskImage() == true )
                        {
                            message( MsgType::INFO, "Disk image re-loaded successfully" );
                        }
                        else
                        {
                            message( MsgType::ERR, "Failed to re-load disk image" );
                        }
                    break;

                    default:
                        std::cout << (char)key << std::endl;
                    break;
                }
            }
        }
    }


    // Check for open file
    if( vdData.fileStream.is_open() )
    {
        vdData.fileStream.close();
        message( MsgType::INFO, "File closed" );
    }

    // Close eumlated disk drive
    Device_close( &drive.dev );


    // Wait for all client threads are terminated.
    message( MsgType::INFO, "Waiting for all client threads to stop." );
    for( auto& thread : tcpClientThreads )
    {
        thread.join();
    }


    // Close debug connection to client
    message( MsgType::INFO, "Waiting for all debug client threads to stop." );
    for( auto& thread : dbgClientThreads )
    {
        gDbgDataReady = true;
        gCv.notify_one();
        thread.join();
    }


    message( MsgType::INFO, "Server shutdown" );
    std::cout << std::endl;

    return 0;
}
