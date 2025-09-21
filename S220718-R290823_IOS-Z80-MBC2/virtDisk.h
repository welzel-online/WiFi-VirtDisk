/***************************************************************************//**
 * @file    virtDisk.h
 *
 * @brief   Handles the virtual disks.
 *
 * @copyright   Copyright (c) 2025 by Welzel-Online
 ******************************************************************************/

#ifndef VIRTDISK_H
#define VIRTDISK_H

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************** Includes **/
#include <stdint.h>
#include "pff.h"


/******************************************************************* Defines **/
#define SPI_WR_DATA   0x02
#define SPI_RD_DATA   0x03
#define SPI_RD_STATUS 0x04
#define SPI_WR_STATUS 0x01

// SPI Status
enum 
{
    SPISLAVE_READY,
    SPISLAVE_BUSY,
    SPISLAVE_CHKSUM_ERR
};

#pragma pack(1)
typedef struct
{
  union
  {
    struct
    {
      uint8_t status;
      uint8_t cmd_status;
      uint8_t cmd_data;
      uint8_t free;
    };
    uint32_t rawStatus;
  };
} vdStatus_t;
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


/********************************************************** Global Variables **/

/******************************************************* Functions / Methods **/
uint8_t rdStatus_spi( void );
int8_t  waitReady_spi( bool timeout = true, const char* dbgInfo = "" );

FRESULT vd_status( uint8_t* wifiStatus, uint8_t* srvStatus );

FRESULT vd_mount(FATFS* fs);								              /* Mount/Unmount a logical drive */
FRESULT vd_open(const char* path);							          /* Open a file */
FRESULT vd_read(void* buff, UINT btr, UINT* br);			    /* Read data from the open file */
FRESULT vd_write(const void* buff, UINT btw, UINT* bw);   /* Write data to the open file */
FRESULT vd_lseek(DWORD ofs);								              /* Move file pointer of the open file */
FRESULT vd_opendir(DIR* dj, const char* path);				    /* Open a directory */
FRESULT vd_readdir(DIR* dj, FILINFO* fno);					      /* Read a directory item from the open directory */

#ifdef __cplusplus
}
#endif

#endif
