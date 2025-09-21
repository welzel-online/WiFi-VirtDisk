/***************************************************************************//**
 * @file    virtDisk.hpp
 *
 * @brief   Handles the virtual disks.
 *
 * @copyright   Copyright (c) 2025 by Welzel-Online
 ******************************************************************************/

#ifndef VIRTDISK_HPP
#define VIRTDISK_HPP

/****************************************************************** Includes **/
#include <cstdint>
#include <string>
#include <fstream>


/******************************************************************* Defines **/
#pragma pack(1)
typedef struct
{
    uint8_t     cmd;            // Command
    // uint8_t     subCmd;         // Sub-Command (needed??)
    int8_t      status;         // Status information
    char        filename[13];   // 8.3\0 = 13
    uint32_t    fileOffset;     // Offset for seek command
    uint16_t    track;          // Track of the disk
    uint8_t     sector;         // Sector of the track
    uint8_t     data[512];      // Data buffer
    uint16_t    dataLen;        // Length of the valid data in buffer
} vdPacketInt_t;
#pragma pack()

#pragma pack(1)
typedef union
{
    vdPacketInt_t   packet;
    char            rawData[sizeof(vdPacketInt_t)];
} vdPacket_t;
#pragma pack()


enum vdCommands
{
    VD_CMD_NONE = 0,
    VD_CMD_STATUS,
    VD_CMD_SEL_FILE,
    VD_CMD_RD_FILE,
    VD_CMD_RD_NEXT,
    VD_CMD_WR_FILE,
    VD_CMD_WR_NEXT,
    VD_CMD_SEEK_FILE,
    VD_CMD_SEL_TR_SEC,
    VD_CMD_RD_SECTOR,
    VD_CMD_WR_SECTOR,
    VD_CMD_COUNT
};

enum vdStatus
{
    VD_STATUS_OK = 0,
    VD_STATUS_ERROR,
    VD_STATUS_FILE_NOT_FOUND,
    VD_STATUS_FILE_RD_ERROR,
    VD_STATUS_DISK_NOT_FOUND,
    VD_STATUS_TR_SEC_ERROR,
    VD_STATUS_SEC_RD_ERROR,
    VD_STATUS_SEC_WR_ERROR,
    VD_STATUS_COUNT
};

typedef struct
{
    std::fstream    fileStream;
    std::streampos  filePos;
    std::string     filename;
    int             track;
    int             sector;
} vdData_t;


/********************************************************** Global Variables **/

/******************************************************* Functions / Methods **/
int vdProcessCmd( char* buffer );

bool vdReloadDiskImage( void );


#endif
