/***************************************************************************//**
 * @file    virtDisk.cpp
 *
 * @brief   Handles the virtual disks.
 *
 * @copyright   Copyright (c) 2025 by Welzel-Online
 ******************************************************************************/


/****************************************************************** Includes **/
#include <iostream>
#include <iomanip>
#include <cstring>

// CP/M Tools
#include "config.h"
#include "cpmtools/cpmfs.h"

// LibDSK
#include <stddef.h>      // Needed for libdisk.h
#include <libdsk.h>

#include "virtDisk.hpp"
#include "message.h"


/******************************************************************* Defines **/

/********************************************************** Global Variables **/
vdData_t vdData;

extern std::string filePath;
extern std::string diskPath;

extern std::string diskEmuFilename;

// Variables for rcpmfs
struct cpmSuperBlock drive;

dsk_err_t     err;
const char   *errStr = NULL;
unsigned char sector[512];

std::string format = "z80mbc2-d0";
std::string devopts = "rcpmfs," + format;


/******************************************************* Functions / Methods **/


// void dumpPacket( void )
// {
//     // Dump packet
//     std::string asciiStr;
//     std::cout << "Packet dump rcpmfs" << std::endl;
//     for( uint16_t addr = 0; addr < sizeof(vd.packet.data); addr++ )
//     {
//         std::cout << std::setfill('0') << std::setw(2) << std::uppercase << std::hex << (int)((vdPacket_t*)buffer)->packet.data[addr] << " ";
//         if( isprint( ((vdPacket_t*)buffer)->packet.data[addr] ) )
//         {
//             asciiStr += (char)((vdPacket_t*)buffer)->packet.data[addr];
//         }
//         else
//         {
//             asciiStr += '.';
//         }

//         if( ( ( (addr + 1) % 16 ) == 0 ) && ( addr != sizeof(vd.packet.data) ) )
//         {
//             std::cout << " : " << asciiStr << std::endl;
//             // std::cout << std::endl;
//             asciiStr = "";
//         }
//     }
//     std::cout << std::dec << std::endl;
// }


/***************************************************************************//**
 * @brief   Process the client command.
 *
 * @param   buffer  Pointer to the receive buffer.
 ******************************************************************************/
int vdProcessCmd( char* buffer )
{
    int         retVal = -1;
    vdPacket_t  vd;
    std::string tempFilename;


    // Copy the packet for command processing
    memcpy( vd.rawData, buffer, sizeof(vd.rawData) );

    switch( vd.packet.cmd )
    {
        case VD_CMD_NONE:
        break;

        case VD_CMD_STATUS:
            message( MsgType::INFO, "VirtDisk Command: Get Status" );

            vd.packet.cmd = VD_CMD_NONE;
        break;

        case VD_CMD_SEL_FILE:
            vdData.filename.assign( vd.packet.filename );

            if( vdData.filename == diskEmuFilename )
            {
                // Check for previous open file
                if( drive.dev.opened == 1 )
                {
                    errStr = Device_close( &drive.dev );
                    if( errStr != NULL )
                    {
                        // Device_close failed
                        message( MsgType::ERR, "Cannot close rcpmfs: " + diskPath + "(" + std::string(errStr) + ")" );
                    }
                    else
                    {
                        message( MsgType::INFO, "VirtDisk Command: Select Emulated File: Previous file closed" );
                    }
                }

                message( MsgType::INFO, "VirtDisk Command: Select Emulated File: " + vdData.filename );
                // std::cout << "Disk path: " << diskPath << std::endl;

                // Open the disk image
                errStr = Device_open( &drive.dev, diskPath.c_str(), O_RDWR, devopts.c_str() );
                if( ( drive.dev.opened == 0 ) || ( errStr != NULL ) )
                {
                    // Device_open failed
                    message( MsgType::ERR, "Cannot open rcpmfs: " + diskPath + "(" + std::string(errStr) + ")" );
                }

                ((vdPacket_t*)buffer)->packet.status = VD_STATUS_OK;

                retVal = 0;
            }
            else
            {
                // Check for previous open file
                if( vdData.fileStream.is_open() == true )
                {
                    vdData.fileStream.close();
                    message( MsgType::INFO, "VirtDisk Command: Select File: Previous file closed" );
                }
                message( MsgType::INFO, "VirtDisk Command: Select File: " + vdData.filename );

                vdData.fileStream.open( filePath + vdData.filename, std::ios::in | std::ios::out | std::ios::binary );
                if( vdData.fileStream.is_open() == true )
                {
                    vdData.fileStream.clear();                      // Clear status of file
                    vdData.fileStream.seekg( 0, std::ios::beg );    // Seek to the begin of the file (read position)
                    vdData.fileStream.seekp( 0, std::ios::beg );    // Seek to the begin of the file (write position)
                    vdData.filePos = vdData.fileStream.tellg();     // Save the current file position

                    ((vdPacket_t*)buffer)->packet.status = VD_STATUS_OK;

                    retVal = 0;
                }
                else
                {
                    /* ERROR */
                    message( MsgType::ERR, "File not found: " + vdData.filename );

                    ((vdPacket_t*)buffer)->packet.status = VD_STATUS_DISK_NOT_FOUND;

                    retVal = 0;
                }
            }

            vd.packet.cmd = VD_CMD_NONE;
        break;

        case VD_CMD_RD_FILE:
            tempFilename.assign( vd.packet.filename );

            if( vdData.filename == tempFilename )
            {
                message( MsgType::INFO, "VirtDisk Command: Read File: " + tempFilename );

                if( vdData.filename == diskEmuFilename )
                {
                    dsk_lsect_t secNum = (vdData.filePos / 512);
                    err = dsk_lread( drive.dev.dev, &drive.dev.geom, sector, secNum );
                    if( err )
                    {
                        message( MsgType::ERR, "Error reading sector: " + std::string(dsk_strerror(err)) );
                        retVal = 0;
                    }

                    memcpy( (char*)((vdPacket_t*)buffer)->packet.data, sector, sizeof(vd.packet.data) );
                    ((vdPacket_t*)buffer)->packet.dataLen = 512;
                    ((vdPacket_t*)buffer)->packet.status  = VD_STATUS_OK;

                    vdData.filePos += 512;

                    retVal = 0;
                }
                else
                {
                    memset( (char*)((vdPacket_t*)buffer)->packet.data, 0x00, sizeof(vd.packet.data) );

                    std::streamsize rdCount;

                    vdData.fileStream.read( (char*)((vdPacket_t*)buffer)->packet.data, sizeof(vd.packet.data) );
                    rdCount = vdData.fileStream.gcount();
                    ((vdPacket_t*)buffer)->packet.dataLen = rdCount;
                    ((vdPacket_t*)buffer)->packet.status  = VD_STATUS_OK;

                    if( rdCount == sizeof(vd.packet.data) )
                    {
                        vdData.filePos = vdData.fileStream.tellg();     // Save the current file position
                    }

                    retVal = 0;
                }
            }
            else
            {
                /* ERROR */
                message( MsgType::ERR, "VirtDisk Command: Read File: Wrong filename" );

                ((vdPacket_t*)buffer)->packet.status = VD_STATUS_FILE_RD_ERROR;

                retVal = 0;
            }

            vd.packet.cmd = VD_CMD_NONE;
        break;

        case VD_CMD_WR_FILE:
            tempFilename.assign( vd.packet.filename );

            message( MsgType::INFO, "VirtDisk Command: Write File: " + tempFilename );

            tempFilename.assign( vd.packet.filename );

            if( vdData.filename == tempFilename )
            {
                if( vdData.filename == diskEmuFilename )
                {
                    dsk_lsect_t secNum = (vdData.filePos / 512);
                    memcpy( sector, (char*)((vdPacket_t*)buffer)->packet.data, sizeof(vd.packet.data) );
                    err = dsk_lwrite( drive.dev.dev, &drive.dev.geom, sector, secNum );
                    if( err )
                    {
                        message( MsgType::ERR, "Error writing sector: " + std::string(dsk_strerror( err )) );
                        retVal = 0;
                    }

                    vdData.filePos += 512;

                    retVal = 0;
                }
                else
                {
                    // Write the data to file
                    if( vdData.fileStream.is_open() == true )
                    {
                        vdData.fileStream.write( (char*)((vdPacket_t*)buffer)->packet.data, sizeof(vd.packet.data) );
                        vdData.fileStream.flush();

                        retVal = 0;
                    }
                }
            }

            vd.packet.cmd = VD_CMD_NONE;
        break;

        case VD_CMD_SEEK_FILE:
            tempFilename.assign( vd.packet.filename );

            uint32_t fileOffset;
            fileOffset = ((vdPacket_t*)buffer)->packet.fileOffset;

            message( MsgType::INFO, "VirtDisk Command: Seek File - Offset: " + std::to_string(fileOffset) );

            if( vdData.filename == tempFilename )
            {
                if( vdData.filename == diskEmuFilename )
                {
                    vdData.filePos = fileOffset;    // Save the current file position

                    ((vdPacket_t*)buffer)->packet.status = VD_STATUS_OK;

                    retVal = 0;
                }
                else
                {
                    if( vdData.fileStream.is_open() == true )
                    {
                        vdData.fileStream.clear();                              // Clear status of file
                        vdData.fileStream.seekg( (std::streampos)fileOffset, std::ios::beg );   // Seek to the given offset in the file (read position)
                        vdData.fileStream.seekp( (std::streampos)fileOffset, std::ios::beg );   // Seek to the given offset in the file (write position)
                        vdData.filePos = vdData.fileStream.tellg();             // Save the current file position

                        ((vdPacket_t*)buffer)->packet.status = VD_STATUS_OK;

                        retVal = 0;
                    }
                    else
                    {
                        /* ERROR */
                        ((vdPacket_t*)buffer)->packet.status = VD_STATUS_DISK_NOT_FOUND;
                    }
                }
            }

            vd.packet.cmd = VD_CMD_NONE;
        break;

        case VD_CMD_SEL_TR_SEC:
            message( MsgType::INFO, "VirtDisk Command: Select Track/Sector" );

            vd.packet.cmd = VD_CMD_NONE;
        break;

        case VD_CMD_RD_SECTOR:
            message( MsgType::INFO, "VirtDisk Command: Read Sector" );

            vd.packet.cmd = VD_CMD_NONE;
        break;

        case VD_CMD_WR_SECTOR:
            message( MsgType::INFO, "VirtDisk Command: Write Sector" );

            vd.packet.cmd = VD_CMD_NONE;
        break;

        default:
            vd.packet.cmd = VD_CMD_NONE;
        break;
    }

    return retVal;
}


/***************************************************************************//**
 * @brief   Reload the virtual disk image
 *
 * @return  0 on success, -1 on failure
 *****************************************************************************/
bool vdReloadDiskImage( void )
{
    bool retVal = true;


    // Check for previous open file
    if( drive.dev.opened == 1 )
    {
        errStr = Device_close( &drive.dev );
        if( errStr != NULL )
        {
            // Device_close failed
            message( MsgType::ERR, "Cannot close rcpmfs: " + diskPath + "(" + std::string(errStr) + ")" );
            retVal = false;
        }
    }

    // Open the disk image
    errStr = Device_open( &drive.dev, diskPath.c_str(), O_RDWR, devopts.c_str() );
    if( ( drive.dev.opened == 0 ) || ( errStr != NULL ) )
    {
        // Device_open failed
        message( MsgType::ERR, "Cannot open rcpmfs: " + diskPath + "(" + std::string(errStr) + ")" );
        retVal = false;
    }

    return retVal;
}
