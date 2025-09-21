/***************************************************************************
 *                                                                         *
 *    LIBDSK: General floppy and diskimage access library                  *
 *    Copyright (C) 2001,2024  John Elliott <seasip.webmaster@gmail.com>   *
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

/* This driver is for a D64 floppy image, a dump (with no metadata) of a 
 * Commodore 64 GCR disk. 
 *
 * The format is documented at 
 * <http://unusedino.de/ec64/technical/formats/d64.html>. What makes it a
 * challenge for libdsk is:
 *
 * - variable geometry - the outer tracks have more sectors than the inner
 *  ones. DSK_GEOMETRY can't cope with that.
 * - Commodore number their tracks from 1. So sector headers on what LibDsk
 *  thinks of as track (n) will be numbered track (n+1). A fruitful source 
 *  of off-by-one errors.
 *
 * There is also a specialised version of this driver for the disk formats
 * used by Commodore 64 CP/M. That treats the D64 file as a container for 
 * the CP/M filesystem and only presents the sectors used by CP/M as sectors.
 * 
 *
 */

#include "drvi.h"
#include "drvldbs.h"
#include "drvd64.h"

/* Number of sectors in each track */
static unsigned d64_spt[] =
{
	21, 21, 21, 21, 21, 21, 21, 21, 21, 21,	/* Tracks  1-10 */
	21, 21, 21, 21, 21, 21, 21, 19, 19, 19,	/* Tracks 11-20 */
	19, 19, 19, 19, 18, 18, 18, 18, 18, 18,	/* Tracks 21-30 */
	17, 17, 17, 17, 17, 17, 17, 17, 17, 17	/* Tracks 31-40 */
};

/* This struct contains function pointers to the driver's functions, and the
 * size of its DSK_DRIVER subclass */

DRV_CLASS dc_d64 = 
{
	sizeof(D64_DSK_DRIVER),
	&dc_ldbsdisk,		/* superclass */
	"d64\0D64\0",
	"D64 disk image driver",
	d64_open,	/* open */
	d64_creat,	/* create new */
	d64_close,	/* close */
};

DRV_CLASS dc_d64cpm = 
{
	sizeof(D64CPM_DSK_DRIVER),
	&dc_ldbsdisk,		/* superclass */
	"d64cpm\0D64CPM\0c64cpm\0C64CPM\0",
	"D64 CP/M disk image driver",
	d64cpm_open,	/* open */
	d64cpm_creat,	/* create new */
	d64cpm_close,	/* close */
};



long ts_to_lba(dsk_pcyl_t track, dsk_psect_t sec)
{
	long lba = 0;
	dsk_pcyl_t n;

	for (n = 1; n < track; n++) lba += d64_spt[n - 1];
	lba += sec;
	return lba;
}

dsk_err_t d64_open(DSK_DRIVER *self, const char *filename, 
		DSK_REPORTFUNC diagfunc)
{
	D64_DSK_DRIVER *d64self;
	long filesize;
	FILE *fp;
	dsk_err_t err;
	dsk_pcyl_t cyl, maxcyl, dircyl;
	dsk_psect_t sec;
	dsk_lsect_t lba;
	unsigned char dirbuf[256];
	unsigned char secbuf[256];
	unsigned char errors[768];
	unsigned char dirlba[768];
	size_t n;
	long pos;
	LDBS_DPB dpb;
	DSK_GEOMETRY geom;
	int ndir = 0;
	
	/* Sanity check: Is this meant for our driver? */
	if (self->dr_class != &dc_d64) return DSK_ERR_BADPTR;
	d64self = (D64_DSK_DRIVER *)self;

	fp = fopen(filename, "r+b");
	if (!fp) 
	{
		d64self->d64_super.ld_readonly = 1;
		fp = fopen(filename, "rb");
	}
	if (!fp) return DSK_ERR_NOTME;
/* Get the size of the file; a valid D64 image should be one of:
 *
 *      Disk type                  Size
 *     ---------                  ------
 *     35 track, no errors        174848
 *     35 track, 683 error bytes  175531
 *     40 track, no errors        196608
 *     40 track, 768 error bytes  197376
 */
	if (fseek(fp, 0, SEEK_END)) return DSK_ERR_SYSERR;
	filesize = ftell(fp);

	memset(errors, 0, sizeof(errors));
	memset(dirlba, 0, sizeof(dirlba));
	if (filesize == 174848 || filesize == 175531)
	{
		maxcyl = 35;
	}
	else if (filesize == 196608 || filesize == 197376)
	{
		maxcyl = 40;
	}
	else
	{
		fclose(fp);
		return DSK_ERR_NOTME;
	}
	if (filesize == 175531)
	{
		if (fseek(fp, 174848, SEEK_SET)) return DSK_ERR_SYSERR;
		if (fread(errors, 1, 683, fp) < 683) return DSK_ERR_SYSERR;
	}
	if (filesize == 197376)
	{
		if (fseek(fp, 196608, SEEK_SET)) return DSK_ERR_SYSERR;
		if (fread(errors, 1, 768, fp) < 768) return DSK_ERR_SYSERR;
	}


	if (diagfunc)
	{
        	diaghead(diagfunc, "D64 disk image");
// Locate the directory
		if (fseek(fp, 0x16500, SEEK_SET)) return DSK_ERR_SYSERR;
		if (fread(dirbuf, 1, 256, fp) < 256) 
			return DSK_ERR_SYSERR;
// Walk the chain of directory sectors
		while (dirbuf[0])
		{
			lba = ts_to_lba(dirbuf[0], dirbuf[1]);
			if (((long)lba) < 0 || lba >= 768 || dirlba[lba])
				break;
			dirlba[lba] = ++ndir;
			if (fseek(fp, lba * 256, SEEK_SET)) 
				return DSK_ERR_SYSERR;
			if (fread(dirbuf, 1, 256, fp) < 256) 
				return DSK_ERR_SYSERR;
		}	
	}
	if (fseek(fp, 0, SEEK_SET)) return DSK_ERR_SYSERR;
// Now convert to (internal) LDBS format.
	err = ldbs_new(&d64self->d64_super.ld_store, NULL, LDBS_DSK_TYPE);
	if (err) return err;

	lba = 0;
	pos = 0;
	for (cyl = 0; cyl < maxcyl; cyl++)
	{
		LDBS_TRACKHEAD *trkh = ldbs_trackhead_alloc(d64_spt[cyl]);
	
		if (!trkh)
		{
			ldbs_close(&d64self->d64_super.ld_store);
			return DSK_ERR_NOMEM;
		}
		trkh->datarate = 1;
		trkh->recmode = RECMODE_GCR_C64;
		trkh->gap3 = 0x12;
		trkh->filler = 0x00;
		for (sec = 0; sec < d64_spt[cyl]; sec++, lba++, pos += 256)
		{
/* For each sector in the D64 drive image file, read it in */
			if (fread(secbuf, 1, 256, fp) < 256) 
			{
				ldbs_free(trkh);
				ldbs_close(&d64self->d64_super.ld_store);
				return DSK_ERR_SYSERR;
			}
			if (diagfunc)
			{
				if (pos == 0x16500)
				{
        				diaghead(diagfunc, "Block availability map");
					diaghex(diagfunc, pos, secbuf, 1, 
						"Directory track");
					diaghex(diagfunc, pos+1, secbuf+1, 1, 
						"Directory sector");
					diaghex(diagfunc, pos+2, secbuf+2, 1, 
						"DOS version");
					diaghex(diagfunc, pos+3, secbuf+3, 1, 
						"Unused");
					for (dircyl = 1; dircyl <= 35; dircyl++)
					{
						diaghex(diagfunc, pos + 4 * dircyl, secbuf + 4 * dircyl, 4, "Track %d BAM", dircyl);
					}
					diaghex(diagfunc, pos+0x90, secbuf+0x90, 16, 
						"Disk name");
					diaghex(diagfunc, pos+0xA0, secbuf+0xA0, 2, "Unused");
					diaghex(diagfunc, pos+0xA2, secbuf+0xA2, 2, "Disk ID");
					diaghex(diagfunc, pos+0xA4, secbuf+0xA4, 1, "Unused");
					diaghex(diagfunc, pos+0xA5, secbuf+0xA5, 2, "DOS type");
					diaghex(diagfunc, pos+0xA7, secbuf+0xA7, 89, "Unused / extra BAM");
				
				}	
				else if (dirlba[lba])
				{
					int entry;

        				diaghead(diagfunc, "Directory sector %d", dirlba[lba]);
					for (entry = 0; entry < 8; entry++)
					{
						if (entry == 0)
						{
							diaghex(diagfunc, pos + 32*entry, secbuf + 32 * entry, 1, "Next directory track");
							diaghex(diagfunc, pos + 32*entry + 1, secbuf + 32 * entry + 1, 1, "Next directory sector");
						}
						else
						{
							diaghex(diagfunc, pos + 32*entry, secbuf + 32 * entry, 2, "Unused");
						}
						diaghex(diagfunc, pos + 32*entry + 2, secbuf + 32 * entry + 2, 1, "File %d type", entry + 1);
						diaghex(diagfunc, pos + 32*entry + 3, secbuf + 32 * entry + 3, 1, "File %d track", entry + 1);
						diaghex(diagfunc, pos + 32*entry + 4, secbuf + 32 * entry + 4, 1, "File %d sector", entry + 1);
						diaghex(diagfunc, pos + 32*entry + 5, secbuf + 32 * entry + 5, 16, "File %d name", entry + 1);
						diaghex(diagfunc, pos + 32*entry + 21, secbuf + 32 * entry + 21, 1, "REL stream track");
						diaghex(diagfunc, pos + 32*entry + 22, secbuf + 32 * entry + 22, 1, "REL stream sector");
						diaghex(diagfunc, pos + 32*entry + 23, secbuf + 32 * entry + 23, 1, "REL record length");
						diaghex(diagfunc, pos + 32*entry + 24, secbuf + 34 * entry + 24, 6, "Unused");
						diaghex(diagfunc, pos + 32*entry + 30, secbuf + 34 * entry + 30, 2, "File %d size", entry + 1);
					}		

				}
				else diaghex(diagfunc, pos, secbuf, 256, 
					"Track %d sector %d", cyl + 1, sec);
			}
/* Add it to the track header */
			trkh->sector[sec].id_cyl = cyl + 1;
			trkh->sector[sec].id_head = 0;
			trkh->sector[sec].id_sec = sec;
			trkh->sector[sec].id_psh = 1;
			trkh->sector[sec].datalen = 256;
			trkh->sector[sec].copies = 0;
/* Migrate error bytes if present */
			switch (errors[lba])
			{
				case 2: // Header descriptor not found
				case 3: // No SYNC sequence found
					trkh->sector[sec].st1 |= 1; break;
				case 4: // ID found but no data
					trkh->sector[sec].st1 |= 4; break;
				case 5: // Data error in data block
					trkh->sector[sec].st2 |= 0x20; break;
				case 9: // Data error in header block
					trkh->sector[sec].st1 |= 0x20; break;
			}
			for (n = 1; n < 256; n++)
				if (secbuf[n] != secbuf[0])
			{
				trkh->sector[sec].copies = 1;
				break;
			}
/* And write it out (if it's not blank) */
			if (!trkh->sector[sec].copies)
			{
				trkh->sector[sec].filler = secbuf[0];
			}
			else
			{
				char id[4];
				ldbs_encode_secid(id, cyl, 0, sec);
				err = ldbs_putblock(d64self->d64_super.ld_store,
					&trkh->sector[sec].blockid, id, secbuf,
					256);
				if (err)
				{
					ldbs_free(trkh);
					ldbs_close(&d64self->d64_super.ld_store);
					return err;
				}
			}
		}
		/* All sectors transferred. Write the track header */
		err = ldbs_put_trackhead(d64self->d64_super.ld_store, trkh, cyl, 0);
		ldbs_free(trkh);
		if (err)
		{
			ldbs_close(&d64self->d64_super.ld_store);
			return err;
		}
	}
	fclose(fp);

	if (filesize == 175531)
	{
		diaghex(diagfunc, 174848, errors, 683, "Sector error codes");
	}
	else if (filesize == 197376)
	{
		diaghex(diagfunc, 196608, errors, 768, "Sector error codes");
	}
	/* Add a DPB block */
	dpb.spt = 17 * 2;
	dpb.bsh = 3;
	dpb.blm = 1;
	dpb.exm = 0;
	dpb.dsm = 135;
	dpb.drm = 63;
	dpb.al[0] = 0xC0;
	dpb.al[1] = 0;
	dpb.cks = 0x10;
	dpb.off = 2;
	dpb.psh = 1;
	dpb.phm = 1;

	/* Create a DSK_GEOMETRY that describes the D64 file we want to see */
	if (maxcyl > 35) maxcyl = 40; else maxcyl = 35;
	geom.dg_sidedness = SIDES_ALT;
	geom.dg_cylinders = maxcyl;
	geom.dg_heads     = 1;
	geom.dg_sectors   = d64_spt[1];
	geom.dg_secbase   = 0;
	geom.dg_secsize   = 256;
	geom.dg_datarate  = RATE_SD;
	geom.dg_rwgap     = 0x12;
	geom.dg_fmtgap    = 0x52;
	geom.dg_fm        = RECMODE_GCR_C64;
	geom.dg_nomulti   = 0;

	err = ldbs_put_dpb(d64self->d64_super.ld_store, &dpb);
	if (!err) err = ldbs_put_geometry(d64self->d64_super.ld_store, &geom);
	if (err)
	{
		ldbs_close(&d64self->d64_super.ld_store);
		return err;
	}
	
	d64self->d64_filename = dsk_malloc_string(filename);
	return ldbsdisk_attach(self);
}


dsk_err_t d64_creat(DSK_DRIVER *self, const char *filename)
{
	D64_DSK_DRIVER *d64self;
	long n;	
	FILE *fp;
	dsk_err_t err;
	
	/* Sanity check: Is this meant for our driver? */
	if (self->dr_class != &dc_d64) return DSK_ERR_BADPTR;
	d64self = (D64_DSK_DRIVER *)self;

	fp = fopen(filename, "wb");
	if (!fp) 
	{
		return DSK_ERR_SYSERR;
	}
	/* Create a minimal 35-track image */
	for (n = 0; n < 174848; n++) 
	{
		if (fputc(0x00, fp) == EOF)
		{
			fclose(fp);
			return DSK_ERR_SYSERR;
		}
	}
	if (fclose(fp))
	{
		return DSK_ERR_SYSERR;
	}
	dsk_isetoption(self, "FS:CP/M:BSH", 3, 1);
	dsk_isetoption(self, "FS:CP/M:BLM", 7, 1);
	dsk_isetoption(self, "FS:CP/M:EXM", 0, 1); 
	dsk_isetoption(self, "FS:CP/M:DSM", 135, 1);
	dsk_isetoption(self, "FS:CP/M:DRM", 63, 1);
	dsk_isetoption(self, "FS:CP/M:AL0", 0xC0, 1);
	dsk_isetoption(self, "FS:CP/M:AL1", 0, 1);
	dsk_isetoption(self, "FS:CP/M:CKS", 0x10, 1);
	dsk_isetoption(self, "FS:CP/M:OFF", 2, 1);
	err = ldbs_new(&d64self->d64_super.ld_store, NULL, LDBS_DSK_TYPE);
	if (err) return err;

	d64self->d64_filename = dsk_malloc_string(filename);
	return DSK_ERR_OK;
}


dsk_err_t d64_close(DSK_DRIVER *self)
{
	D64_DSK_DRIVER *d64self;
	dsk_err_t err;
	dsk_pcyl_t cyl, maxcyl;
	dsk_lsect_t lba;
	dsk_psect_t sec;
	dsk_phead_t maxhead;
	int have_errors;
	unsigned char secbuf[256];
	unsigned char errors[768];
	DSK_GEOMETRY geom;
	FILE *fp;

	have_errors = 0;
	memset(errors, 0, sizeof(errors));

	if (self->dr_class != &dc_d64) return DSK_ERR_BADPTR;
	d64self = (D64_DSK_DRIVER *)self;
	/* Detach the blockstore (causing any changes to be flushed) */
	err = ldbsdisk_detach(self);
	if (err)
	{
		dsk_free(d64self->d64_filename);
		ldbs_close(&d64self->d64_super.ld_store);
		return err;	
	}
	/* If image not touched, close */
	if (!self->dr_dirty)
	{
		dsk_free(d64self->d64_filename);
		return ldbs_close(&d64self->d64_super.ld_store);
	}
	/* If read-only, abandon */
	if (d64self->d64_super.ld_readonly)
	{
		dsk_free(d64self->d64_filename);
		ldbs_close(&d64self->d64_super.ld_store);
		return DSK_ERR_RDONLY;	
	}
	fp = fopen(d64self->d64_filename, "wb");
	if (!fp)
	{
		dsk_free(d64self->d64_filename);
		ldbs_close(&d64self->d64_super.ld_store);
		return DSK_ERR_SYSERR;
	}
	/* Output file opened. Now reattach so we can use ldbsdisk_xread() */
	err = ldbs_max_cyl_head(d64self->d64_super.ld_store, &maxcyl, &maxhead);
	if (!err) err = ldbsdisk_attach(self);
	if (err)
	{
		dsk_free(d64self->d64_filename);
		ldbs_close(&d64self->d64_super.ld_store);
		return err;	
	}

	/* Create a DSK_GEOMETRY that describes the D64 file we want to see */
	if (maxcyl > 35) maxcyl = 40; else maxcyl = 35;
	geom.dg_sidedness = SIDES_ALT;
	geom.dg_cylinders = maxcyl;
	geom.dg_heads     = 1;
	geom.dg_sectors   = d64_spt[1];
	geom.dg_secbase   = 0;
	geom.dg_secsize   = 256;
	geom.dg_datarate  = RATE_SD;
	geom.dg_rwgap     = 0x12;
	geom.dg_fmtgap    = 0x52;
	geom.dg_fm        = RECMODE_GCR_C64;
	geom.dg_nomulti   = 0;

	lba = 0;
	for (cyl = 0; cyl < maxcyl; cyl++)
	{
		for (sec = 0; sec < d64_spt[cyl]; sec++, lba++)
		{
			err = ldbsdisk_xread(self, &geom, secbuf,
					cyl, 0, cyl + 1, 0, sec, 256, NULL);
			switch (err)
			{
				case DSK_ERR_OK:
					break;
				case DSK_ERR_NOADDR:
					errors[lba] = 2; have_errors = 1;
					memset(secbuf, 0, sizeof(secbuf));
					break;
				case DSK_ERR_NODATA:
					errors[lba] = 4; have_errors = 1;
					memset(secbuf, 0, sizeof(secbuf));
					break;
				case DSK_ERR_DATAERR:
					errors[lba] = 5; have_errors = 1;
					break;
// Map all other errors to 'Data descriptor byte not found'
				default:
					errors[lba] = 4; have_errors = 1;
					memset(secbuf, 0, sizeof(secbuf));
					break;
			}
			if (fwrite(secbuf, 1, 256, fp) < 256)
			{
				dsk_free(d64self->d64_filename);
				ldbs_close(&d64self->d64_super.ld_store);
				return DSK_ERR_SYSERR;
			}
		}	
	}
	// Detach again 
	err = ldbsdisk_detach(self);
	if (err)
	{
		fclose(fp);
		dsk_free(d64self->d64_filename);
		ldbs_close(&d64self->d64_super.ld_store);
		return err;	
	}
	if (have_errors)
	{
		if (fwrite(errors, 1, lba, fp) < lba)	
		{
			fclose(fp);
			dsk_free(d64self->d64_filename);
			ldbs_close(&d64self->d64_super.ld_store);
			return DSK_ERR_SYSERR;
		}
	}
	dsk_free(d64self->d64_filename);
	if (fclose(fp))
	{
		ldbs_close(&d64self->d64_super.ld_store);
		return DSK_ERR_SYSERR;
	}
	ldbs_close(&d64self->d64_super.ld_store);
	return DSK_ERR_OK;	
}




dsk_err_t d64cpm_open(DSK_DRIVER *self, const char *filename, 
		DSK_REPORTFUNC diagfunc)
{
	D64CPM_DSK_DRIVER *d64self;
	long filesize;
	FILE *fp;
	dsk_err_t err;
	dsk_pcyl_t cyl, cpmcyl, maxcyl;
	dsk_psect_t sec;
	dsk_lsect_t lba;
	unsigned char dirbuf[256];
	unsigned char secbuf[256];
	unsigned char errors[768];
	unsigned char dirlba[768];
	size_t n;
	long pos;
	LDBS_DPB dpb;
	DSK_GEOMETRY geom;
	int ndir = 0;
	
	/* Sanity check: Is this meant for our driver? */
	if (self->dr_class != &dc_d64cpm) return DSK_ERR_BADPTR;
	d64self = (D64CPM_DSK_DRIVER *)self;

	fp = fopen(filename, "r+b");
	if (!fp) 
	{
		d64self->d64cpm_super.ld_readonly = 1;
		fp = fopen(filename, "rb");
	}
	if (!fp) return DSK_ERR_NOTME;
/* Get the size of the file; a valid D64 image should be one of:
 *
 *      Disk type                  Size
 *     ---------                  ------
 *     35 track, no errors        174848
 *     35 track, 683 error bytes  175531
 *     40 track, no errors        196608
 *     40 track, 768 error bytes  197376
 */
	if (fseek(fp, 0, SEEK_END)) return DSK_ERR_SYSERR;
	filesize = ftell(fp);

	memset(errors, 0, sizeof(errors));
	memset(dirlba, 0, sizeof(dirlba));
	if (filesize == 174848 || filesize == 175531)
	{
		maxcyl = 35;
	}
	else if (filesize == 196608 || filesize == 197376)
	{
		maxcyl = 40;
	}
	else
	{
		fclose(fp);
		return DSK_ERR_NOTME;
	}
	if (filesize == 175531)
	{
		if (fseek(fp, 174848, SEEK_SET)) return DSK_ERR_SYSERR;
		if (fread(errors, 1, 683, fp) < 683) return DSK_ERR_SYSERR;
	}
	if (filesize == 197376)
	{
		if (fseek(fp, 196608, SEEK_SET)) return DSK_ERR_SYSERR;
		if (fread(errors, 1, 768, fp) < 768) return DSK_ERR_SYSERR;
	}
	if (fseek(fp, 0x16590, SEEK_SET)) return DSK_ERR_SYSERR;
	if (fread(secbuf, 1, 256, fp) < 256) return DSK_ERR_SYSERR;
	if (memcmp(secbuf, "CP/M DISK", 9)) return DSK_ERR_NOTME;

	if (diagfunc)
	{
        	diaghead(diagfunc, "D64 CP/M disk image");
// Locate the directory
		if (fseek(fp, 0x16500, SEEK_SET)) return DSK_ERR_SYSERR;
		if (fread(dirbuf, 1, 256, fp) < 256) 
			return DSK_ERR_SYSERR;
// Walk the chain of directory sectors
		while (dirbuf[0])
		{
			lba = ts_to_lba(dirbuf[0], dirbuf[1]);
			if (((long)lba) < 0 || lba >= 768 || dirlba[lba])
				break;
			dirlba[lba] = ++ndir;
			if (fseek(fp, lba * 256, SEEK_SET)) 
				return DSK_ERR_SYSERR;
			if (fread(dirbuf, 1, 256, fp) < 256) 
				return DSK_ERR_SYSERR;
		}	
	}
	if (fseek(fp, 0, SEEK_SET)) return DSK_ERR_SYSERR;
// Now convert to (internal) LDBS format.
	err = ldbs_new(&d64self->d64cpm_super.ld_store, NULL, LDBS_DSK_TYPE);
	if (err) return err;

	lba = 0;
	pos = 0;
	cpmcyl = 0;
	for (cyl = 0; cyl < maxcyl; cyl++)
	{
		sec = 0;
		if (cyl != 17) 
		{
			LDBS_TRACKHEAD *trkh = ldbs_trackhead_alloc(17);
	
			if (!trkh)
			{
				ldbs_close(&d64self->d64cpm_super.ld_store);
				return DSK_ERR_NOMEM;
			}
			trkh->datarate = 1;
			trkh->recmode = RECMODE_GCR_C64;
			trkh->gap3 = 0x12;
			trkh->filler = 0x00;
			for (sec = 0; sec < 17; sec++, lba++, pos += 256)
			{
/* For each sector in the D64 drive image used by CP/M, read it in */
				if (fread(secbuf, 1, 256, fp) < 256) 
				{
					ldbs_free(trkh);
					ldbs_close(&d64self->d64cpm_super.ld_store);
					return DSK_ERR_SYSERR;
				}
				if (diagfunc)
				{
					diaghex(diagfunc, pos, secbuf, 256, 
						"Track %d sector %d (%s)", 
						cyl, sec,
						(cyl < 2) ? "System track" : "CP/M filesystem");
				}
/* Add it to the track header */
				trkh->sector[sec].id_cyl = cpmcyl;
				trkh->sector[sec].id_head = 0;
				trkh->sector[sec].id_sec = sec;
				trkh->sector[sec].id_psh = 1;
				trkh->sector[sec].datalen = 256;
				trkh->sector[sec].copies = 0;
/* Migrate error bytes if present */
				switch (errors[lba])
				{
					case 2: // Header descriptor not found
					case 3: // No SYNC sequence found
						trkh->sector[sec].st1 |= 1; break;
					case 4: // ID found but no data
						trkh->sector[sec].st1 |= 4; break;
					case 5: // Data error in data block
						trkh->sector[sec].st2 |= 0x20; break;
					case 9: // Data error in header block
						trkh->sector[sec].st1 |= 0x20; break;
				}
				for (n = 1; n < 256; n++)
					if (secbuf[n] != secbuf[0])
				{
					trkh->sector[sec].copies = 1;
					break;
				}
/* And write it out (if it's not blank) */
				if (!trkh->sector[sec].copies)
				{
					trkh->sector[sec].filler = secbuf[0];
				}
				else
				{
					char id[4];
					ldbs_encode_secid(id, cyl, 0, sec);
					err = ldbs_putblock(d64self->d64cpm_super.ld_store,
						&trkh->sector[sec].blockid, id, secbuf, 256);
					if (err)
					{
						ldbs_free(trkh);
						ldbs_close(&d64self->d64cpm_super.ld_store);
						return err;
					}
				} 
			} /* end for (sectors) */
			/* All sectors transferred. Write the track header */
			err = ldbs_put_trackhead(d64self->d64cpm_super.ld_store, trkh, cpmcyl, 0);
			ldbs_free(trkh);
			if (err)
			{
				ldbs_close(&d64self->d64cpm_super.ld_store);
				return err;
			}
			++cpmcyl;
		} /* end if (cyl != 17) */
		for (; sec < d64_spt[cyl]; sec++, lba++, pos += 256)
		{
			/* Skip sectors not used by CP/M filesystem */
			if (fread(secbuf, 1, 256, fp) < 256) 
			{
				ldbs_close(&d64self->d64cpm_super.ld_store);
				return DSK_ERR_SYSERR;
			}
			if (diagfunc)
			{
				diaghex(diagfunc, pos, secbuf, 256, 
					"Track %d sector %d (skipped)", cyl, sec);
			}
		}
	} /* End for (tracks) */
	fclose(fp);

	if (filesize == 175531)
	{
		diaghex(diagfunc, 174848, errors, 683, "Sector error codes");
	}
	else if (filesize == 197376)
	{
		diaghex(diagfunc, 196608, errors, 768, "Sector error codes");
	}
	/* Add a DPB block */
	dpb.spt = 17 * 2;
	dpb.bsh = 3;
	dpb.blm = 1;
	dpb.exm = 0;
	dpb.dsm = 135;
	dpb.drm = 63;
	dpb.al[0] = 0xC0;
	dpb.al[1] = 0;
	dpb.cks = 0x10;
	dpb.off = 2;
	dpb.psh = 1;
	dpb.phm = 1;

	/* Create a DSK_GEOMETRY (we deduct one cylinder because of 
	 * skipping the C64 directory track) */
	if (maxcyl > 35) maxcyl = 39; else maxcyl = 34;
	geom.dg_sidedness = SIDES_ALT;
	geom.dg_cylinders = maxcyl;
	geom.dg_heads     = 1;
	geom.dg_sectors   = 17;
	geom.dg_secbase   = 0;
	geom.dg_secsize   = 256;
	geom.dg_datarate  = RATE_SD;
	geom.dg_rwgap     = 0x12;
	geom.dg_fmtgap    = 0x52;
	geom.dg_fm        = RECMODE_GCR_C64;
	geom.dg_nomulti   = 0;

	err = ldbs_put_dpb(d64self->d64cpm_super.ld_store, &dpb);
	if (!err) err = ldbs_put_geometry(d64self->d64cpm_super.ld_store, &geom);
	if (err)
	{
		ldbs_close(&d64self->d64cpm_super.ld_store);
		return err;
	}
	
	d64self->d64cpm_filename = dsk_malloc_string(filename);
	return ldbsdisk_attach(self);
}



/* Protective C64 directory for a C64 CP/M disk (showing all sectors in use) */
static const unsigned char blankdir[512] = 
{
	0x12, 0x01, 0x41, 0x00, 0x15, 0xff, 0xff, 0x1f,	/* 00 */
	0x15, 0xff, 0xff, 0x1f, 0x15, 0xff, 0xff, 0x1f,
	0x15, 0xff, 0xff, 0x1f, 0x15, 0xff, 0xff, 0x1f, /* 10 */
	0x15, 0xff, 0xff, 0x1f, 0x15, 0xff, 0xff, 0x1f, 
	0x15, 0xff, 0xff, 0x1f, 0x15, 0xff, 0xff, 0x1f, /* 20 */
	0x15, 0xff, 0xff, 0x1f, 0x15, 0xff, 0xff, 0x1f, 
	0x15, 0xff, 0xff, 0x1f, 0x15, 0xff, 0xff, 0x1f, /* 30 */
	0x15, 0xff, 0xff, 0x1f, 0x15, 0xff, 0xff, 0x1f, 
	0x15, 0xff, 0xff, 0x1f, 0x15, 0xff, 0xff, 0x1f, /* 40 */
	0x11, 0xfc, 0xff, 0x07, 0x13, 0xff, 0xff, 0x07,
	0x13, 0xff, 0xff, 0x07, 0x13, 0xff, 0xff, 0x07, /* 50 */
	0x13, 0xff, 0xff, 0x07, 0x13, 0xff, 0xff, 0x07,
	0x13, 0xff, 0xff, 0x07, 0x12, 0xff, 0xff, 0x03, /* 60 */
	0x12, 0xff, 0xff, 0x03, 0x12, 0xff, 0xff, 0x03,
	0x12, 0xff, 0xff, 0x03, 0x12, 0xff, 0xff, 0x03,	/* 70 */
	0x12, 0xff, 0xff, 0x03, 0x11, 0xff, 0xff, 0x01,
	0x11, 0xff, 0xff, 0x01, 0x11, 0xff, 0xff, 0x01, /* 80 */
	0x11, 0xff, 0xff, 0x01, 0x11, 0xff, 0xff, 0x01,
	'C', 'P', '/', 'M', ' ', 'D', 'I', 'S', 'K',	/* 90 */
	      0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0,
	0xA0, 0xA0, 0x36, 0x35, 0xA0, 0x32, 0x41, 0xA0, /* A0 */
	0xA0, 0xA0, 0xA0, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* B0 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* C0 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* D0 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* E0 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* F0 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 100 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
	
dsk_err_t d64cpm_creat(DSK_DRIVER *self, const char *filename)
{
	D64CPM_DSK_DRIVER *d64self;
	long n;	
	FILE *fp;
	dsk_err_t err;
	/* Sanity check: Is this meant for our driver? */
	if (self->dr_class != &dc_d64cpm) return DSK_ERR_BADPTR;
	d64self = (D64CPM_DSK_DRIVER *)self;

	fp = fopen(filename, "wb");
	if (!fp) 
	{
		return DSK_ERR_SYSERR;
	}
	/* Create a 35-track image */
	for (n = 0; n < 174848; n++) 
	{
		int c = 1;

		if (n >= 0x2A00 && n < 0x3200)
		{
			c = 0xE5;	/* CP/M directory */
		}
		else if (n >= 0x16500 && n < 0x16700)
		{
			c = blankdir[n - 0x16500]; /* C64 directory */
		}
		else if (((n & 0xFF) == 0) && n < 0x1500)
		{
			c = 0x55;	/* System track sectors go 55 01 */
		}
		else if (((n & 0xFF) == 0) && n >= 0x1500)
		{
			c = 0x4B;	/* Other sectors go 4B 01 */
		}
		if (fputc(c, fp) == EOF)
		{
			fclose(fp);
			return DSK_ERR_SYSERR;
		}
	}


	if (fclose(fp))
	{
		return DSK_ERR_SYSERR;
	}
	dsk_isetoption(self, "FS:CP/M:BSH", 3, 1);
	dsk_isetoption(self, "FS:CP/M:BLM", 7, 1);
	dsk_isetoption(self, "FS:CP/M:EXM", 0, 1); 
	dsk_isetoption(self, "FS:CP/M:DSM", 135, 1);
	dsk_isetoption(self, "FS:CP/M:DRM", 63, 1);
	dsk_isetoption(self, "FS:CP/M:AL0", 0xC0, 1);
	dsk_isetoption(self, "FS:CP/M:AL1", 0, 1);
	dsk_isetoption(self, "FS:CP/M:CKS", 0x10, 1);
	dsk_isetoption(self, "FS:CP/M:OFF", 2, 1);
	err = ldbs_new(&d64self->d64cpm_super.ld_store, NULL, LDBS_DSK_TYPE);
	if (err) return err;

	d64self->d64cpm_filename = dsk_malloc_string(filename);
	return DSK_ERR_OK;
}


dsk_err_t d64cpm_close(DSK_DRIVER *self)
{
	D64CPM_DSK_DRIVER *d64self;
	dsk_err_t err;
	dsk_pcyl_t cyl, maxcyl, cpmcyl, c;
	dsk_lsect_t lba;
	dsk_psect_t sec;
	dsk_phead_t maxhead;
	int have_errors, errsize;
	long offset;
	unsigned char secbuf[256];
	unsigned char errors[768];
	DSK_GEOMETRY geom;
	FILE *fp;

	have_errors = 0;
	memset(errors, 0, sizeof(errors));

	if (self->dr_class != &dc_d64cpm) return DSK_ERR_BADPTR;
	d64self = (D64CPM_DSK_DRIVER *)self;
	/* Detach the blockstore (causing any changes to be flushed) */
	err = ldbsdisk_detach(self);
	if (err)
	{
		dsk_free(d64self->d64cpm_filename);
		ldbs_close(&d64self->d64cpm_super.ld_store);
		return err;	
	}
	/* If image not touched, close */
	if (!self->dr_dirty)
	{
		dsk_free(d64self->d64cpm_filename);
		return ldbs_close(&d64self->d64cpm_super.ld_store);
	}
	/* If read-only, abandon */
	if (d64self->d64cpm_super.ld_readonly)
	{
		dsk_free(d64self->d64cpm_filename);
		ldbs_close(&d64self->d64cpm_super.ld_store);
		return DSK_ERR_RDONLY;	
	}
	fp = fopen(d64self->d64cpm_filename, "r+b");
	if (!fp)
	{
		dsk_free(d64self->d64cpm_filename);
		ldbs_close(&d64self->d64cpm_super.ld_store);
		return DSK_ERR_SYSERR;
	}
	/* Output file opened. Now reattach so we can use ldbsdisk_xread() */
	err = ldbs_max_cyl_head(d64self->d64cpm_super.ld_store, &maxcyl, &maxhead);
	if (!err) err = ldbsdisk_attach(self);
	if (err)
	{
		dsk_free(d64self->d64cpm_filename);
		ldbs_close(&d64self->d64cpm_super.ld_store);
		return err;	
	}

	/* Create a DSK_GEOMETRY that describes the D64 file we want to see */
	if (maxcyl > 35) 
	{
		maxcyl = 40, errsize = 768;
	}
	else 
	{	
		maxcyl = 35, errsize = 683;
	}
	geom.dg_sidedness = SIDES_ALT;
	geom.dg_cylinders = maxcyl;
	geom.dg_heads     = 1;
	geom.dg_sectors   = d64_spt[1];
	geom.dg_secbase   = 0;
	geom.dg_secsize   = 256;
	geom.dg_datarate  = RATE_SD;
	geom.dg_rwgap     = 0x12;
	geom.dg_fmtgap    = 0x52;
	geom.dg_fm        = RECMODE_GCR_C64;
	geom.dg_nomulti   = 0;

	for (cpmcyl = 0; cpmcyl < maxcyl - 1; cpmcyl++)
	{
		if (cpmcyl < 17) cyl = cpmcyl;
		else		 cyl = cpmcyl + 1;

		for (c = 0, lba = 0, offset = 0; c < cyl; c++)
		{
			lba += d64_spt[c];
			offset += 256 * d64_spt[c];
		}
		for (sec = 0; sec < 17; sec++, lba++, offset += 256)
		{
			err = ldbsdisk_xread(self, &geom, secbuf,
					cpmcyl, 0, cpmcyl, 0, sec, 256, NULL);
			switch (err)
			{
				case DSK_ERR_OK:
					break;
				case DSK_ERR_NOADDR:
					errors[lba] = 2; have_errors = 1;
					memset(secbuf, 0, sizeof(secbuf));
					break;
				case DSK_ERR_NODATA:
					errors[lba] = 4; have_errors = 1;
					memset(secbuf, 0, sizeof(secbuf));
					break;
				case DSK_ERR_DATAERR:
					errors[lba] = 5; have_errors = 1;
					break;
// Map all other errors to 'Data descriptor byte not found'
				default:
					errors[lba] = 4; have_errors = 1;
					memset(secbuf, 0, sizeof(secbuf));
					break;
			}
			if (fseek(fp, offset, SEEK_SET) ||
			    fwrite(secbuf, 1, 256, fp) < 256)
			{
				dsk_free(d64self->d64cpm_filename);
				ldbs_close(&d64self->d64cpm_super.ld_store);
				return DSK_ERR_SYSERR;
			}
		}	
	}
	// Detach again 
	err = ldbsdisk_detach(self);
	if (err)
	{
		fclose(fp);
		dsk_free(d64self->d64cpm_filename);
		ldbs_close(&d64self->d64cpm_super.ld_store);
		return err;	
	}
	if (have_errors)
	{
		if (fwrite(errors, 1, errsize, fp) < errsize)	
		{
			fclose(fp);
			dsk_free(d64self->d64cpm_filename);
			ldbs_close(&d64self->d64cpm_super.ld_store);
			return DSK_ERR_SYSERR;
		}
	}
	dsk_free(d64self->d64cpm_filename);
	if (fclose(fp))
	{
		ldbs_close(&d64self->d64cpm_super.ld_store);
		return DSK_ERR_SYSERR;
	}
	ldbs_close(&d64self->d64cpm_super.ld_store);
	return DSK_ERR_OK;	
}


