/***************************************************************************
 *                                                                         *
 *    LIBDSK: General floppy and diskimage access library                  *
 *    Copyright (C) 2001, 2024  John Elliott <seasip.webmaster@gmail.com>  *
 *                                                                         *
 *    This library is free software; you can redistribute it and/or        *
 *    modify it under the terms of the GNU Library General Public          *
 *    License as published by the Free Software Foundation; either         *
 *    version 2 of the License, or (at your option) any later version.     *
 *                                                                         *
 *    This library is distributed in the hope that it will be useful,      *
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU    *
 *    Library General Public License for more details.                     *
 *                                                                         *
 *    You should have received a copy of the GNU Library General Public    *
 *    License along with this library; if not, write to the Free           *
 *    Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,      *
 *    MA 02111-1307, USA                                                   *
 *                                                                         *
 ***************************************************************************/

/* Declarations for the D64 and D64CPM drivers */

typedef struct
{
        LDBSDISK_DSK_DRIVER d64_super;
	char *d64_filename;
} D64_DSK_DRIVER;

dsk_err_t d64_open(DSK_DRIVER *self, const char *filename,
	DSK_REPORTFUNC diagfunc);
dsk_err_t d64_creat(DSK_DRIVER *self, const char *filename);
dsk_err_t d64_close(DSK_DRIVER *self);


typedef struct
{
        LDBSDISK_DSK_DRIVER d64cpm_super;
	char *d64cpm_filename;
} D64CPM_DSK_DRIVER;

dsk_err_t d64cpm_open(DSK_DRIVER *self, const char *filename,
	DSK_REPORTFUNC diagfunc);
dsk_err_t d64cpm_creat(DSK_DRIVER *self, const char *filename);
dsk_err_t d64cpm_close(DSK_DRIVER *self);

