/***************************************************************************//**
 * @file    test.c
 *
 * @brief   Functions for testing.
 *
 * @copyright   Copyright (c) 2025 by Welzel-Online
 ******************************************************************************/


/****************************************************************** Includes **/
#include <cstdint>
#include <cstring>
#include <iostream>

#include "test.h"

// CP/M Tools
#include "config.h"
#include "cpmtools/cpmfs.h"


/******************************************************************* Defines **/

/********************************************************** Global Variables **/
extern struct cpmSuperBlock drive;


/******************************************************* Functions / Methods **/

const char cmd[]="libdsk-test";
static const char * const month[12]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec" };


/* namecmp -- compare two entries */ /*{{{*/
static int namecmp(const void *a, const void *b)
{
  if (**((const char * const *)a)=='[') return -1;
  return strcmp(*((const char * const *)a),*((const char * const *)b));
}


static void olddir( char **dirent, int entries )
{
  int i,j,k,l,user,announce,showuser,files;

  //showuser=!onlyuser0(dirent,entries);
  files=0;
  for (user=0; user<32; ++user)
  {
    announce=1;
    for (i=l=0; i<entries; ++i)
    {
      /* This selects real regular files implicitly, because only those have
       * the user in their name.  ".", ".." and the password file do not.
       */
      if (dirent[i][0]=='0'+user/10 && dirent[i][1]=='0'+user%10)
      {
        ++files;
        if (announce && showuser)
        {
          printf("User %d\n",user);
          announce=0;
        }
        if (l%4) printf(" : ");
        for (j=2; dirent[i][j] && dirent[i][j]!='.'; ++j) putchar(toupper(dirent[i][j]));
        k=j; while (k<11) { putchar(' '); ++k; }
        if (dirent[i][j]=='.') ++j;
        for (k=0; dirent[i][j]; ++j,++k) putchar(toupper(dirent[i][j]));
        for (; k<3; ++k) putchar(' ');
        ++l;
      }
      if (l && (l%4)==0)
      {
	l = 0;
	putchar('\n');
      }
    }
    if (l%4)
    {
      putchar('\n');
    }
  }

  if (files==0) printf("No file\n");
}


static void oldddir(char **dirent, int entries, struct cpmInode *ino)
{
  struct cpmStatFS buf;
  struct cpmStat statbuf;
  struct cpmInode file;

  if (entries>2)
  {
    int i,j,k,l,announce,user;

    qsort(dirent,entries,sizeof(char*),namecmp);
    cpmStatFS(ino,&buf);
    printf("     Name    Bytes   Recs  Attr     update             create\n");
    printf("------------ ------ ------ ---- -----------------  -----------------\n");
    announce=0;
    for (l=user=0; user<32; ++user)
    {
      for (i=0; i<entries; ++i)
      {
        struct tm *tmp;

        if (dirent[i][0]=='0'+user/10 && dirent[i][1]=='0'+user%10)
        {
          if (announce==1)
          {
            printf("\nUser %d:\n\n",user);
            printf("     Name    Bytes   Recs  Attr     update             create\n");
            printf("------------ ------ ------ ---- -----------------  -----------------\n");
          }
          announce=2;
          for (j=2; dirent[i][j] && dirent[i][j]!='.'; ++j) putchar(toupper(dirent[i][j]));
          k=j; while (k<10) { putchar(' '); ++k; }
          putchar('.');
          if (dirent[i][j]=='.') ++j;
          for (k=0; dirent[i][j]; ++j,++k) putchar(toupper(dirent[i][j]));
          for (; k<3; ++k) putchar(' ');

          cpmNamei(ino,dirent[i],&file);
          cpmStat(&file,&statbuf);
          printf(" %5.1ldK",(long) (statbuf.size+buf.f_bsize-1) /
			buf.f_bsize*(buf.f_bsize/1024));

          printf(" %6.1ld ",(long)(statbuf.size/128));
          putchar(statbuf.mode&0200 ? ' ' : 'R');
          putchar(statbuf.mode&01000 ? 'S' : ' ');
          putchar(' ');
          if (statbuf.mtime)
          {
            tmp=localtime(&statbuf.mtime);
            printf("  %02d-%s-%04d %02d:%02d",tmp->tm_mday,month[tmp->tm_mon],tmp->tm_year+1900,tmp->tm_hour,tmp->tm_min);
          }
          else if (statbuf.ctime) printf("                   ");
          if (statbuf.ctime)
          {
            tmp=localtime(&statbuf.ctime);
            printf("  %02d-%s-%04d %02d:%02d",tmp->tm_mday,month[tmp->tm_mon],tmp->tm_year+1900,tmp->tm_hour,tmp->tm_min);
          }
          putchar('\n');
          ++l;
        }
      }
      if (announce==2) announce=1;
    }
    printf("%5.1d Files occupying %6.1ldK",l,(buf.f_bused*buf.f_bsize)/1024);
    printf(", %7.1ldK Free.\n",(buf.f_bfree*buf.f_bsize)/1024);
  }
  else printf("No files found\n");
}


static void old3dir(char **dirent, int entries, struct cpmInode *ino)
{
  struct cpmStatFS buf;
  struct cpmStat statbuf;
  struct cpmInode file;

  if (entries>2)
  {
    int i,j,k,l,announce,user, attrib;
    int totalBytes=0,totalRecs=0;

    qsort(dirent,entries,sizeof(char*),namecmp);
    cpmStatFS(ino,&buf);
    announce=1;
    for (l=0,user=0; user<32; ++user)
    {
      for (i=0; i<entries; ++i)
      {
        struct tm *tmp;

        if (dirent[i][0]=='0'+user/10 && dirent[i][1]=='0'+user%10)
        {
          cpmNamei(ino,dirent[i],&file);
          cpmStat(&file,&statbuf);
	      cpmAttrGet(&file, &attrib);
          if (announce==1)
          {
            if (user) putchar('\n');
            printf("Directory For Drive A:  User %2.1d\n\n",user);
            printf("    Name     Bytes   Recs   Attributes   Prot      Update          %s\n",
		ino->sb->cnotatime ? "Create" : "Access");
            printf("------------ ------ ------ ------------ ------ --------------  --------------\n\n");
          }
          announce=2;
          for (j=2; dirent[i][j] && dirent[i][j]!='.'; ++j) putchar(toupper(dirent[i][j]));
          k=j; while (k<10) { putchar(' '); ++k; }
          putchar(' ');
          if (dirent[i][j]=='.') ++j;
          for (k=0; dirent[i][j]; ++j,++k) putchar(toupper(dirent[i][j]));
          for (; k<3; ++k) putchar(' ');

          totalBytes+=statbuf.size;
          totalRecs+=(statbuf.size+127)/128;
          printf(" %5.1ldk",(long) (statbuf.size+buf.f_bsize-1) /
			buf.f_bsize*(buf.f_bsize/1024));
          printf(" %6.1ld ",(long)((statbuf.size+127)/128));
          putchar((attrib & CPM_ATTR_F1)   ? '1' : ' ');
          putchar((attrib & CPM_ATTR_F2)   ? '2' : ' ');
          putchar((attrib & CPM_ATTR_F3)   ? '3' : ' ');
          putchar((attrib & CPM_ATTR_F4)   ? '4' : ' ');
          putchar((statbuf.mode&(S_IWUSR|S_IWGRP|S_IWOTH)) ? ' ' : 'R');
          putchar((attrib & CPM_ATTR_SYS)  ? 'S' : ' ');
          putchar((attrib & CPM_ATTR_ARCV) ? 'A' : ' ');
          printf("      ");
          if      (attrib & CPM_ATTR_PWREAD)  printf("Read   ");
          else if (attrib & CPM_ATTR_PWWRITE) printf("Write  ");
          else if (attrib & CPM_ATTR_PWDEL)   printf("Delete ");
          else printf("None   ");
          if (statbuf.mtime)
          {
            tmp=localtime(&statbuf.mtime);
            printf("%02d/%02d/%02d %02d:%02d  ",tmp->tm_mon+1,tmp->tm_mday,tmp->tm_year%100,tmp->tm_hour,tmp->tm_min);
          }
          else printf("                ");
          if (ino->sb->cnotatime && statbuf.ctime)
          {
            tmp=localtime(&statbuf.ctime);
            printf("%02d/%02d/%02d %02d:%02d",tmp->tm_mon+1,tmp->tm_mday,tmp->tm_year%100,tmp->tm_hour,tmp->tm_min);
          }
          else if (!ino->sb->cnotatime && statbuf.atime)
          {
            tmp=localtime(&statbuf.atime);
            printf("%02d/%02d/%02d %02d:%02d",tmp->tm_mon+1,tmp->tm_mday,tmp->tm_year%100,tmp->tm_hour,tmp->tm_min);
          }
          putchar('\n');
          ++l;
        }
      }
      if (announce==2) announce=1;
    }
    printf("\nTotal Bytes     = %6.1dk  ",(totalBytes+1023)/1024);
    printf("Total Records = %7.1d  ",totalRecs);
    printf("Files Found = %4.1d\n",l);
    printf("Total 1k Blocks = %6.1ld   ",(buf.f_bused*buf.f_bsize)/1024);
    printf("Used/Max Dir Entries For Drive A: %4.1ld/%4.1ld\n",buf.f_files-buf.f_ffree,buf.f_files);
  }
  else printf("No files found\n");
}


int get_stdformat_count()
{
	int n = 0;
	static int rv = -1;	// Cache the result so we only have to add them up once

	if (rv != -1)
	{
		return rv;
	}

    dsk_cchar_t formatName;
	while (dg_stdformat(NULL, (dsk_format_t)n, &formatName, NULL) == DSK_ERR_OK)
	{
        std::cout << "Standard-Format #" << n << " - " << formatName << std::endl;
		n++;
	}
	rv = n;
	return rv;
}


int getFormatID( dsk_cchar_t format )
{
    int retVal = -1;
   	int formatID = 0;
    dsk_cchar_t formatName;

	while( dg_stdformat( NULL, (dsk_format_t)formatID, &formatName, NULL ) == DSK_ERR_OK )
	{
        // std::cout << "Standard-Format #" << formatID << " - " << formatName << std::endl;

        if( strcmp( formatName, format ) == 0 )
        {
            retVal = formatID;
            break;
        }
		formatID++;
	}

	return retVal;
}


/***************************************************************************//**
 * @brief   The test function.
 ******************************************************************************/
int test( void )
{
#if 0
    const char *errStr = NULL;
    int         err = 0;
    struct cpmSuperBlock drive;
    const char *devopts=NULL;
    struct cpmInode root;

    static char starlit[2]="*";
    static char * const star[]={ starlit };

    // star[0] = pat;
    // std::string filename = "D:/Projekte/libdsk-test/testData/DS0N00.DSK";
    std::string filename = "D:/Projekte/libdsk-test/testData/disk/";
    std::string format = "z80mbc2-d0";

    std::cout << std::endl;

	errStr = Device_open( &drive.dev, filename.c_str(), O_RDONLY, devopts );
    if( errStr != NULL )
    {
        // Device_close( &drive.dev );
        std::cout << "Cannot open " << filename << "(" << errStr << ")" << std::endl;
    }

    err = cpmReadSuper( &drive, &root, format.c_str(), 0 );
    if( err == -1 )
    {
        // Device_close( &drive.dev );
        std::cout << "Cannot read superblock (" << err << ")" << std::endl;
    }

    int dir_count = 0;
    char **dir_name = NULL;

    cpmglob( 0, 1, star, &root, &dir_count, &dir_name );

    olddir( dir_name, dir_count );
    std::cout << std::endl << std::endl;
    oldddir( dir_name, dir_count, &root );
    std::cout << std::endl << std::endl;
    old3dir( dir_name, dir_count, &root );

    cpmglobfree( dir_name, dir_count );
    cpmUmount( &drive );

    //std::cout << err << std::endl;
#endif



#if 0
    // Öffnen Sie das Diskimage
    std::string filename = "D:/Projekte/WiFi-VirtDisk/WiFi-VirtDisk-Server/testData/DS0N00.DSK";
    // std::string filename = "D:/Projekte/libdsk-test/testData/disk/";

    DSK_PDRIVER driver;
    dsk_err_t err;
    DSK_GEOMETRY dg;
    unsigned char sector[512];

    std::cout << std::endl << "Disk Image: " << filename << std::endl;

    err = dsk_open( &driver, filename.c_str(), NULL, NULL );
    if (err) {
        fprintf( stderr, "Fehler beim Öffnen des Diskimages: %s\n", dsk_strerror(err) );
        return 1;
    }

    // Lesen Sie die Geometrie des Diskimages
    dsk_cchar_t format = "z80mbc2-d0";
    dsk_format_t customFormat = (dsk_format_t)getFormatID( format );
    err = dg_stdformat( &dg, customFormat , NULL, NULL );
    // err = dg_stdformat(&dg, (dsk_format_t)29, NULL, NULL);
    // err = dsk_getgeom(drive, &dg);
    if (err) {
        fprintf( stderr, "Fehler beim Lesen der Geometrie: %s\n", dsk_strerror(err) );
        dsk_close( &driver );
        return 1;
    }

    // Lesen Sie einen Sektor (z.B. Zylinder 0, Kopf 0, Sektor 1)
    for( dsk_pcyl_t cyl = 0; cyl < 3; cyl++ )
    {
        for( dsk_psect_t sec = 30; sec < 35; sec++ )
        {
            err = dsk_pread( driver, &dg, sector, cyl, 0, sec );
            // err = dsk_lread( driver, &dg, sector, sec );
            if (err) {
                fprintf(stderr, "Fehler beim Lesen des Sektors: %s\n", dsk_strerror(err));
                dsk_close( &driver );
                return 1;
            }

            // Verarbeiten Sie die gelesenen Daten hier
            // printf("Erster Byte des Sektors: 0x%02X\n", sector[0]);
            std::cout << std::endl << "Dump Cylinder #" << cyl << " - Sector #" << sec << std::endl;
            std::string ascii;
            for( uint16_t i = 0; i < 512; i++ )
            {
                printf( "0x%02X ", sector[i] );
                if( isprint( sector[i] ) ) { ascii += sector[i]; } else { ascii +='.'; }
                if( ( (i+1) % 16 ) == 0 ) { std::cout << " : " << ascii << std::endl; ascii = ""; }
            }
        }
    }

    // Schließen Sie das Diskimage
    dsk_close( &driver );

#else
    struct cpmSuperBlock drive;

    dsk_err_t err;
    const char *errStr = NULL;
    unsigned char sector[512];

    std::string format = "z80mbc2-d0";
    std::string devopts = "rcpmfs," + format;

    std::string filename = "D:/Projekte/WiFi-VirtDisk/WiFi-VirtDisk-Server/testData/disk/";

    std::cout << std::endl << "Disk Image: " << filename << std::endl;

    errStr = Device_open( &drive.dev, filename.c_str(), O_RDONLY, devopts.c_str() );
    if( errStr != NULL )
    {
        // Device_close( &drive.dev );
        std::cout << "Cannot open " << filename << "(" << errStr << ")" << std::endl;
    }

    struct cpmInode root;
    err = cpmReadSuper( &drive, &root, format.c_str(), 0 );
    if( err == -1 )
    {
        // Device_close( &drive.dev );
        std::cout << "Cannot read superblock (" << err << ")" << std::endl;
    }

    int dir_count = 0;
    char **dir_name = NULL;

    static char starlit[2]="*";
    static char * const star[]={ starlit };
    cpmglob( 0, 1, star, &root, &dir_count, &dir_name );

    olddir( dir_name, dir_count );
    std::cout << std::endl << std::endl;

    cpmglobfree( dir_name, dir_count );


    // Lesen Sie einen Sektor (z.B. Zylinder 0, Kopf 0, Sektor 1)
    for( dsk_pcyl_t cyl = 1; cyl < 2; cyl++ )
    {
        for( dsk_psect_t sec = 0; sec < 3; sec++ )
        {
            err = dsk_pread( drive.dev.dev, &drive.dev.geom, sector, cyl, 0, sec );
            // err = dsk_lread( drive.dev.dev, &dg, sector, sec );
            if (err) {
                fprintf(stderr, "Fehler beim Lesen des Sektors: %s\n", dsk_strerror(err));
                return 1;
            }

            // Verarbeiten Sie die gelesenen Daten hier
            // printf("Erster Byte des Sektors: 0x%02X\n", sector[0]);
            std::cout << std::endl << "Dump Cylinder #" << cyl << " - Sector #" << sec << std::endl;
            std::string ascii;
            for( uint16_t i = 0; i < 512; i++ )
            {
                printf( "0x%02X ", sector[i] );
                if( isprint( sector[i] ) ) { ascii += sector[i]; } else { ascii +='.'; }
                if( ( (i+1) % 16 ) == 0 ) { std::cout << " : " << ascii << std::endl; ascii = ""; }
            }
        }

        // for( dsk_psect_t sec = 39; sec < 43; sec++ )
        // {
        //     err = dsk_pread( drive.dev.dev, &drive.dev.geom, sector, cyl, 0, sec );
        //     // err = dsk_lread( drive.dev.dev, &dg, sector, sec );
        //     if (err) {
        //         fprintf(stderr, "Fehler beim Lesen des Sektors: %s\n", dsk_strerror(err));
        //         return 1;
        //     }

        //     // Verarbeiten Sie die gelesenen Daten hier
        //     // printf("Erster Byte des Sektors: 0x%02X\n", sector[0]);
        //     std::cout << std::endl << "Dump Cylinder #" << cyl << " - Sector #" << sec << std::endl;
        //     std::string ascii;
        //     for( uint16_t i = 0; i < 512; i++ )
        //     {
        //         printf( "0x%02X ", sector[i] );
        //         if( isprint( sector[i] ) ) { ascii += sector[i]; } else { ascii +='.'; }
        //         if( ( (i+1) % 16 ) == 0 ) { std::cout << " : " << ascii << std::endl; ascii = ""; }
        //     }
        // }
    }

    cpmUmount( &drive );
    Device_close( &drive.dev );
#endif

    return 0;
}
