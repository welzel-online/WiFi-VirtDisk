/***************************************************************************
 *                                                                         *
 *    LIBDSK: General floppy and diskimage access library                  *
 *    Copyright (C) 2001-2022  John Elliott <seasip.webmaster@gmail.com>   *
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

/* Support for the D88 format, as defined at 
 * <https://www.pc98.org/project/doc/d88.html> */

#define D88_USER_BLOCK "ud88"

#include <stdio.h>
#include "libdsk.h"
#include "drvi.h"
#include "drvldbs.h"
#include "drvd88.h"

/* This struct contains function pointers to the driver's functions, and the
 * size of its DSK_DRIVER subclass */
DRV_CLASS dc_d88 = 
{
	sizeof(D88_DSK_DRIVER),
	&dc_ldbsdisk,	/* superclass */
	"d88\0D88\0",
	"D88 disk image",
	d88_open,	/* open */
	d88_creat,	/* create new */
	d88_close,	/* close */
};

dsk_err_t d88_open(DSK_DRIVER *self, const char *filename, DSK_REPORTFUNC diagfunc)
{
	dsk_psect_t sector;
	size_t spt;
	unsigned char sech[16];
	D88_DSK_DRIVER *d88self;
	FILE *fp;
	unsigned long offset, pos, tracko, seco, last;
	dsk_err_t err;
	LDBS_TRACKHEAD *trkh;
	unsigned char *buffer = NULL;
	size_t buflen = 0;
	int c;
	/* Sanity check: Is this meant for our driver? */
	if (self->dr_class != &dc_d88) return DSK_ERR_BADPTR;
	d88self = (D88_DSK_DRIVER *)self;

	/* Save the filename, we'll want it when doing output */
	d88self->d88_filename = dsk_malloc_string(filename);
	if (!d88self->d88_filename) return DSK_ERR_NOMEM;

	fp = fopen(filename, "r+b");
	if (!fp)
	{
		d88self->d88_super.ld_readonly = 1;
		fp = fopen(filename, "rb");
	}
	if (!fp) 
	{
		dsk_free(d88self->d88_filename);
		return DSK_ERR_NOTME;
	}
	/* File is open. Load the header. */
	if (fread(d88self->d88_header, 1, 672, fp) < 128)
	{
/* File should be at least 672 bytes long */
		fclose(fp);
		dsk_free(d88self->d88_filename);
		return DSK_ERR_NOTME;
	}
	offset = ldbs_peek4(d88self->d88_header + 0x20);
/* Offset to first track needs to be 0x02A0 or 0x02B0. Media flag needs 
 * to be 0x00, 0x10, 0x20, 0x30 or 0x40 */
	if ((offset != 0x2A0 && offset != 0x2B0) 
	   || (d88self->d88_header[0x1B] & 0x0F) 
	   || ((d88self->d88_header[0x1B] >> 4) > 4))
	{
		fclose(fp);
		dsk_free(d88self->d88_filename);
		return DSK_ERR_NOTME;
		
	}
	if (offset == 0x2B0)
	{
/* Read in the extra 16 bytes of header */
		if (fread(d88self->d88_header + 672, 1, 16, fp) < 16)
		{
			fclose(fp);
			dsk_free(d88self->d88_filename);
			return DSK_ERR_NOTME;
		}
	}
	/* OK, we're ready to load the file. */
	err = ldbs_new(&d88self->d88_super.ld_store, NULL, LDBS_DSK_TYPE);
	if (err)
	{
		dsk_free(d88self->d88_filename);
		fclose(fp);
		return err;
	}
	diaghead(diagfunc, "D88 file header");
	diaghex(diagfunc, 0x00, d88self->d88_header + 0x00, 0x11, "Description");
	diaghex(diagfunc, 0x11, d88self->d88_header + 0x11, 0x09, "Reserved");
	diaghex(diagfunc, 0x1A, d88self->d88_header + 0x1A, 0x01, "Write protected");
	diaghex(diagfunc, 0x1B, d88self->d88_header + 0x1B, 0x01, "Media flag");
	diaghex(diagfunc, 0x1C, d88self->d88_header + 0x1C, 0x04, "Disk size");
	for (pos = 0x20; pos < offset; pos += 4)
	{
		diaghex(diagfunc, pos, d88self->d88_header + pos, 0x04, 
			"Track %ld offset", (pos - 0x20) / 4);
	}
	d88self->d88_header[0x10] = 0;	/* Ensure comment is terminated */

	err = ldbs_put_comment(d88self->d88_super.ld_store, (char *)d88self->d88_header);
	if (err)
	{
		dsk_free(d88self->d88_filename);
		ldbs_close(&d88self->d88_super.ld_store);
		fclose(fp);
		return err;
	}
/* XXX Create a custom 'ud88' block to hold the original media flag */
	if (d88self->d88_header[0x1A])
	{
		d88self->d88_super.ld_readonly = 1;
	}
	last = ldbs_peek4(d88self->d88_header + 0x1C);
	for (pos = 0x20; pos < offset; pos += 4)
	{
		tracko = ldbs_peek4(d88self->d88_header + pos);
		if (tracko >= last || 0 == tracko) continue;

		seco = tracko;
		if (fseek(fp, tracko, SEEK_SET))
		{
			dsk_free(d88self->d88_filename);
			ldbs_close(&d88self->d88_super.ld_store);
			fclose(fp);
			return DSK_ERR_CORRUPT;
		}
/* Read the first sector header */
		if (fread(sech, 1, 16, fp) < 16)
		{
			dsk_free(d88self->d88_filename);
			ldbs_close(&d88self->d88_super.ld_store);
			fclose(fp);
			return DSK_ERR_CORRUPT;
		}
		spt = ldbs_peek2(sech + 4);
		trkh = ldbs_trackhead_alloc(spt);
		if (!trkh)
		{
			dsk_free(d88self->d88_filename);
			ldbs_close(&d88self->d88_super.ld_store);
			fclose(fp);
			return DSK_ERR_NOMEM;
		}	
		diaghead(diagfunc, "Track %ld", (pos - 0x20) / 4);
		trkh->filler = 0xE5;
		trkh->recmode = (sech[6] & 0x40) ? 0x01: 0x02;	/* FM / MFM */
		for (sector = 0; sector < spt; sector++)
		{
/* Load the new sector header */
			if (sector > 0 && fread(sech, 1, 16, fp) < 16)
			{
				dsk_free(d88self->d88_filename);
				ldbs_close(&d88self->d88_super.ld_store);
				fclose(fp);
				return DSK_ERR_CORRUPT;
			}
			diaghead(diagfunc, "Sector entry %d", sector);
			diaghex(diagfunc, seco + 0x00, sech + 0x00, 1, "Cylinder ID");
			diaghex(diagfunc, seco + 0x01, sech + 0x01, 1, "Head ID");
			diaghex(diagfunc, seco + 0x02, sech + 0x02, 1, "Sector ID");
			diaghex(diagfunc, seco + 0x03, sech + 0x03, 1, 
				"Sector size=%d", 128 << sech[3]);
			diaghex(diagfunc, seco + 0x04, sech + 0x04, 2, "Sectors in track");
			diaghex(diagfunc, seco + 0x06, sech + 0x06, 1, "Recording mode");	
			diaghex(diagfunc, seco + 0x07, sech + 0x07, 1, "Deleted data");	
			diaghex(diagfunc, seco + 0x08, sech + 0x08, 1, "FDC status");	
			diaghex(diagfunc, seco + 0x09, sech + 0x09, 1, "Seek time");	
			diaghex(diagfunc, seco + 0x0A, sech + 0x0A, 3, "Reserved");	
			diaghex(diagfunc, seco + 0x0D, sech + 0x0D, 1, "RPM");	
			diaghex(diagfunc, seco + 0x0E, sech + 0x0E, 2, "Actual data length");	
			trkh->sector[sector].id_cyl  = sech[0];
			trkh->sector[sector].id_head = sech[1];
			trkh->sector[sector].id_sec  = sech[2];
			trkh->sector[sector].id_psh  = sech[3];
			trkh->sector[sector].st1 = sech[8];
			trkh->sector[sector].st2 = sech[7] ? 0x40 : 0;
			trkh->sector[sector].copies = 0;
			trkh->sector[sector].filler = 0xE5;

			if (buflen != (128 << sech[3]))
			{
				buffer = dsk_realloc(buffer, 128 << sech[3]);
				if (!buffer)
				{	
					dsk_free(d88self->d88_filename);
					ldbs_close(&d88self->d88_super.ld_store);
					fclose(fp);
					return DSK_ERR_NOMEM;
				}
				buflen = 128 << sech[3];
			}
			if (fread(buffer, 1, buflen, fp) < buflen)
			{
				dsk_free(d88self->d88_filename);
				ldbs_close(&d88self->d88_super.ld_store);
				fclose(fp);
				return DSK_ERR_CORRUPT;
			}
			diaghex(diagfunc, seco + 0x10, buffer, buflen, "Sector data");	
			for (c = 1; c < buflen; c++)
			{
				if (buffer[c] != buffer[0])
				{
					trkh->sector[sector].copies = 1;
					break;
				}
			}
			if (!trkh->sector[sector].copies)
			{
				trkh->sector[sector].filler = buffer[0];
			}
			else	
			{
				char sectype[4];

				ldbs_encode_secid(sectype, 
						sech[0], sech[1], sech[2]);
				err = ldbs_putblock
						(d88self->d88_super.ld_store,
						&trkh->sector[sector].blockid,
						sectype, buffer, buflen);
				if (err)
				{
					dsk_free(d88self->d88_filename);
					ldbs_close(&d88self->d88_super.ld_store);
					fclose(fp);
					return err;
				}
			}
			seco += (buflen + 0x10);
		}
/* In an LDBS file, we record a track's physical location on disk (and the ID in the
 * sector headers). Other disk images (such as DSK) use the tracks' order in the 
 * file to give their physical locations on disk. I'm not familiar enough with D88 to 
 * know how the location might be derived from the track's order in the file, so for 
 * now assume that the ID in the last sector header processed accurately specifies the 
 * physical location. */
		err = ldbs_put_trackhead(d88self->d88_super.ld_store, trkh, sech[0],
					sech[1]);
		if (trkh)
		{
			dsk_free(trkh);
			trkh = NULL;
		}
		if (err)
		{
			dsk_free(d88self->d88_filename);
			dsk_free(buffer);
			ldbs_close(&d88self->d88_super.ld_store);
			fclose(fp);
			return err;
		}
		/* End of track */
	}
	err = ldbs_putblock_d(d88self->d88_super.ld_store, 
				D88_USER_BLOCK, &d88self->d88_header[0x1B], 1);
	if (err)
	{
		dsk_free(d88self->d88_filename);
		dsk_free(buffer);
		ldbs_close(&d88self->d88_super.ld_store);
		fclose(fp);
		return err;
	}
	dsk_free(buffer);
	return ldbsdisk_attach(self);
}


dsk_err_t d88_creat(DSK_DRIVER *self, const char *filename)
{
	D88_DSK_DRIVER *d88self;
	dsk_err_t err;
	FILE *fp;

	/* Sanity check: Is this meant for our driver? */
	if (self->dr_class != &dc_d88) return DSK_ERR_BADPTR;
	d88self = (D88_DSK_DRIVER *)self;

	/* Save the filename, we'll want it when doing output */
	d88self->d88_filename = dsk_malloc_string(filename);
	if (!d88self->d88_filename) return DSK_ERR_NOMEM;

	/* Create an empty file, just to be sure we can */
	fp = fopen(filename, "wb");
	if (!fp) return DSK_ERR_SYSERR;

	memset(d88self->d88_header, 0, sizeof(d88self->d88_header));
	ldbs_poke4(d88self->d88_header + 0x20, sizeof(d88self->d88_header)); 
	if (fwrite(d88self->d88_header, 1, sizeof(d88self->d88_header), fp) < sizeof(d88self->d88_header))
	{
		fclose(fp);
		return DSK_ERR_SYSERR;
	}
	fclose(fp);

	/* OK, create a new empty blockstore */
	err = ldbs_new(&d88self->d88_super.ld_store, NULL, LDBS_DSK_TYPE);
	if (err)
	{
		dsk_free(d88self->d88_filename);
		return err;
	}
	/* Finally, hand the blockstore to the superclass so it can provide
	 * all the read/write/format methods */
	return ldbsdisk_attach(self);
}

static dsk_err_t save_data(PLDBS self, dsk_pcyl_t cyl, dsk_phead_t head, 
				LDBS_TRACKHEAD *th, void *param)
{
	unsigned short sec, pos;
	D88_DSK_DRIVER *d88self = (D88_DSK_DRIVER *)param;
	unsigned char sech[0x10];
	unsigned char *secbuf;
	size_t buflen;

	if (d88self->d88_track >= 164)	/* D88 can hold at most 164 tracks */
	{
		return DSK_ERR_OK;
	}

	ldbs_poke4(d88self->d88_header + 0x20 + 4 * d88self->d88_track, 
			d88self->d88_trko); 

	for (sec = 0; sec < th->count; sec++)
	{
		memset(sech, 0, sizeof(sech));
		sech[0] = th->sector[sec].id_cyl;
		sech[1] = th->sector[sec].id_head;
		sech[2] = th->sector[sec].id_sec;
		sech[3] = th->sector[sec].id_psh;
		sech[4] = (th->count) & 0xFF;
		sech[5] = (th->count >> 8) & 0xFF;
		sech[6] = (th->recmode == 1) ? 0x40 : 0x00;	/* Data rate */
		sech[7] = (th->sector[sec].st2 & 0x40) ? 0x10 : 0x00;	/* Deleted data */
		sech[8] = th->sector[sec].st1;				/* Status ST1 */
		sech[14] = th->sector[sec].datalen & 0xFF;
		sech[15] = (th->sector[sec].datalen >> 8) & 0xFF;


		if (fwrite(sech, 1, sizeof(sech), d88self->d88_fp) < sizeof(sech))
		{
			return DSK_ERR_SYSERR;
		}
		if (th->sector[sec].copies == 0)
		{
			for (pos = 0; pos < th->sector[sec].datalen; pos++)
			{
				if (fputc(th->sector[sec].filler, d88self->d88_fp) == EOF)
					return DSK_ERR_SYSERR; 
			}
		}
		else
		{
			secbuf = dsk_malloc(buflen = th->sector[sec].datalen);
			if (secbuf == NULL) return DSK_ERR_NOMEM;
			memset(secbuf, th->sector[sec].filler, th->sector[sec].datalen);
			if (th->sector[sec].blockid)
			{
				dsk_err_t err = ldbs_getblock(self, th->sector[sec].blockid,
						NULL, secbuf, &buflen);
				if (err) return err;
			}
			if (fwrite(secbuf, 1, th->sector[sec].datalen, d88self->d88_fp) < 
					th->sector[sec].datalen) return DSK_ERR_SYSERR;

		}
		d88self->d88_trko += sizeof(sech) + th->sector[sec].datalen;
	}

	++d88self->d88_track;
	return DSK_ERR_OK;
}


dsk_err_t d88_close(DSK_DRIVER *self)
{
	D88_DSK_DRIVER *d88self;
	dsk_err_t err;
	char *comment;
	size_t len = 1;
	unsigned char media_id;

	/* Sanity check: Is this meant for our driver? */
	if (self->dr_class != &dc_d88) return DSK_ERR_BADPTR;
	d88self = (D88_DSK_DRIVER *)self;

	/* Firstly, ensure any pending changes are flushed to the LDBS 
	 * blockstore. Once this has been done we own the blockstore again 
	 * and have to close it after we've finished with it. */
	err = ldbsdisk_detach(self); 
	if (err)
	{
		dsk_free(d88self->d88_filename);
		ldbs_close(&d88self->d88_super.ld_store);
		return err;
	}

	/* If this disc image has not been written to, just close it and 
	 * dispose thereof. */
	if (!self->dr_dirty)
	{
		dsk_free(d88self->d88_filename);
		return ldbs_close(&d88self->d88_super.ld_store);
	}
	/* Trying to save changes but source is read-only */
	if (d88self->d88_super.ld_readonly)
	{
		dsk_free(d88self->d88_filename);
		ldbs_close(&d88self->d88_super.ld_store);
		return DSK_ERR_RDONLY;
	}
 	/* Now write out the blockstore. Start by creating the header. */
	memset(d88self->d88_header, 0, sizeof(d88self->d88_header));
	if (dsk_get_comment(self, &comment) == DSK_ERR_OK && NULL != comment)
	{
		strncpy((char *)d88self->d88_header, comment, 16);
	}
	/* Write out the (empty) header */
	d88self->d88_fp = fopen(d88self->d88_filename, "wb");
	if (!d88self->d88_fp)
	{
		ldbs_close(&d88self->d88_super.ld_store);
		return DSK_ERR_SYSERR;
	}
	if (fwrite(d88self->d88_header, 1, sizeof(d88self->d88_header), d88self->d88_fp) < (int)sizeof(d88self->d88_header))
	{
		fclose(d88self->d88_fp);
		remove(d88self->d88_filename);
		ldbs_close(&d88self->d88_super.ld_store);
		return DSK_ERR_SYSERR;
	}
	d88self->d88_trko = sizeof(d88self->d88_header);
	d88self->d88_track = 0;

	/* Retrieve media ID if there is one */
	len = 1;
	err = ldbs_getblock_d(d88self->d88_super.ld_store, D88_USER_BLOCK, &media_id, &len);
	if ((err == DSK_ERR_OK || err == DSK_ERR_OVERRUN) && len != 0)
	{
		d88self->d88_header[0x1B] = media_id;
	}

	err = ldbs_all_tracks(d88self->d88_super.ld_store, save_data,
				SIDES_ALT, d88self);
	/* Store correct file size */
	ldbs_poke4(d88self->d88_header + 0x1C, d88self->d88_trko); 

	/* Also store it as the offset to the next unused track, just in case something
	 * tries to determine the length of a track as offset[track+1] - offset[track] */
	if (d88self->d88_track < 164)	/* D88 can hold at most 164 tracks */
	{
		ldbs_poke4(d88self->d88_header + 0x20 + 4 * d88self->d88_track, 
			d88self->d88_trko); 
	}



	if (!err)	/* Rewrite the header, now populated */
	{
		if (fseek(d88self->d88_fp, 0, SEEK_SET)) err = DSK_ERR_SYSERR;
		else if (fwrite(d88self->d88_header, 1, sizeof(d88self->d88_header), d88self->d88_fp) < (int)sizeof(d88self->d88_header)) err = DSK_ERR_SYSERR;
		else if (fclose(d88self->d88_fp)) err = DSK_ERR_SYSERR;
	}
	if (err)
	{
		fclose(d88self->d88_fp);
		remove(d88self->d88_filename);
		ldbs_close(&d88self->d88_super.ld_store);
		return err;
	}
	ldbs_close(&d88self->d88_super.ld_store);
	return DSK_ERR_OK;
}


