/* --------------------------------------------------------------------------

   MusicBrainz -- The Internet music metadatabase

   Copyright (C) 2008 Patrick Hurrelmann 
   Copyright (C) 2006 Matthias Friedrich
   Copyright (C) 2000 Robert Kaye
   Copyright (C) 1999 Marc E E van Woerkom
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.
   
   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

--------------------------------------------------------------------------- */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/cdio.h>
#include <arpa/inet.h>


#include "discid/discid.h"
#include "discid/discid_private.h"
#include "unix.h"


#define NUM_CANDIDATES 2

/* starting with FreeBSD 9 /dev/cd0 is always used, /dev/acd0 is deprecated */
static char *device_candidates[NUM_CANDIDATES] = {"/dev/cd0", "/dev/acd0"};


int mb_disc_unix_read_toc_header(int fd, mb_disc_toc *toc) {
	struct ioc_toc_header th;

	int ret = ioctl(fd, CDIOREADTOCHEADER, &th);

	if ( ret < 0 )
		return 0; /* error */

	toc->first_track_num = th.starting_track;
	toc->last_track_num = th.ending_track;

	return 1;
}


int mb_disc_unix_read_toc_entry(int fd, int track_num, mb_disc_toc_track *track) {
	struct ioc_read_toc_single_entry te;
	int ret;

	memset(&te, 0, sizeof te);
	te.track = track_num;
	te.address_format = CD_LBA_FORMAT;

	/* CDIOREADTOCENTRY is created in FreeBSD because it doesn't need
	 * an additional struct inside the te struct.
	 * (freebsd commit ae544a60069e99ed14cab46e981a79ba165564a9)
	 */
	ret = ioctl(fd, CDIOREADTOCENTRY, &te);

	if ( ret < 0 )
		return 0; /* error */

	assert( te.address_format == CD_LBA_FORMAT );
	/* FreeBSD header note: the lba has network byte order */
	track->address = ntohl(te.entry.addr.lba);
	track->control = te.entry.control;

	return 1;
}

void mb_disc_unix_read_mcn(int fd, mb_disc_private *disc) {
	struct cd_sub_channel_info sci;
	struct ioc_read_subchannel rsc;

	memset(&sci, 0, sizeof sci);
	memset(&rsc, 0, sizeof rsc);
	rsc.address_format = CD_LBA_FORMAT; /* not technically relevant */
	rsc.data_format    = CD_MEDIA_CATALOG;
	rsc.data_len       = sizeof sci;
	rsc.data           = &sci;

	if ( ioctl(fd, CDIOCREADSUBCHANNEL, &rsc) < 0 )
		perror ("Warning: Unable to read the disc's media catalog number");
	else {

		assert( rsc.address_format                 == CD_LBA_FORMAT );
		assert( rsc.data_format                    == CD_MEDIA_CATALOG );
		assert( sci.what.media_catalog.data_format == CD_MEDIA_CATALOG );

		if (sci.what.media_catalog.mc_valid)
		  strncpy( disc->mcn, (const char *) sci.what.media_catalog.mc_number,
			   MCN_STR_LENGTH );
		else
		  strncpy( disc->mcn, "*invalid*", MCN_STR_LENGTH );

	}  
}

void mb_disc_unix_read_isrc(int fd, mb_disc_private *disc, int track_num) {
	struct cd_sub_channel_info sci;
	struct ioc_read_subchannel rsc;

	memset(&sci, 0, sizeof sci);
	memset(&rsc, 0, sizeof rsc);
	rsc.address_format = CD_LBA_FORMAT; /* not technically relevant */
	rsc.data_format    = CD_TRACK_INFO;
	rsc.track          = track_num;
	rsc.data_len       = sizeof sci;
	rsc.data           = &sci;

	if ( ioctl(fd, CDIOCREADSUBCHANNEL, &rsc) < 0 )
		perror ("Warning: Unable to read track info (ISRC)");
	else {

		assert( rsc.address_format               == CD_LBA_FORMAT );
		assert( rsc.data_format                  == CD_TRACK_INFO );
		assert( sci.what.track_info.data_format  == CD_TRACK_INFO );
		assert( sci.what.track_info.track_number == track_num );

		if (sci.what.track_info.ti_valid)
		  strncpy( disc->isrc[track_num], (const char *) sci.what.track_info.ti_number,
			   ISRC_STR_LENGTH );
		else
		  strncpy( disc->isrc[track_num], "*invalid*", ISRC_STR_LENGTH );

	}  
}

char *mb_disc_get_default_device_unportable(void) {
	return mb_disc_unix_find_device(device_candidates, NUM_CANDIDATES);
}

int mb_disc_has_feature_unportable(enum discid_feature feature) {
	switch(feature) {
		case DISCID_FEATURE_READ:
		case DISCID_FEATURE_MCN:
		case DISCID_FEATURE_ISRC:
			return 1;
		default:
			return 0;
	}
}

int mb_disc_read_unportable(mb_disc_private *disc, const char *device,
			    unsigned int features) {
	return mb_disc_unix_read(disc, device, features);
}

/* EOF */
