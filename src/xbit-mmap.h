/*
** Copyright (C) 2009-2017 Quadrant Information Security <quadrantsec.com>
** Copyright (C) 2009-2017 Champ Clark III <cclark@quadrantsec.com>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License Version 2 as
** published by the Free Software Foundation.  You may not use, modify or
** distribute this program under any other version of the GNU General
** Public License.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"             /* From autoconf */
#endif

#include "sagan-defs.h"

void Xbit_Set_MMAP( int, char *, char *, int, int, char * );
sbool Xbit_Condition_MMAP ( int, char *, char *, int, int, char * );
void Xbit_Cleanup_MMAP( void );
sbool Xbit_Count_MMAP( int, char *, char *, char * );

typedef struct _Sagan_Xbit_Track _Sagan_Xbit_Track;
struct _Sagan_Xbit_Track {
    char	xbit_name[64];
    int		xbit_timeout;
    int		xbit_srcport;
    int		xbit_dstport;
};

typedef struct _Sagan_IPC_Xbit _Sagan_IPC_Xbit;
struct _Sagan_IPC_Xbit {
    char xbit_name[64];
    sbool xbit_state;
    unsigned char ip_src[MAXIPBIT];
    unsigned char ip_dst[MAXIPBIT];
    int src_port;
    int dst_port;
    char username[64];
    uintmax_t xbit_date;
    uintmax_t xbit_expire;
    int expire;
    char selector[MAXSELECTOR]; // No need to clean this, as we always set it when tracking
};

