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

/* sagan-bluedot.h
 *
 * Does real time lookups of IP addresses from the Quadrant reputation
 * database.   This means you have to have authentication!
 *
 */


#ifdef WITH_BLUEDOT

#define BLUEDOT_PROCESSOR_USER_AGENT "User-Agent: Sagan-SIEM"

/* Extensions on URL passed depending on what type of query we want to do */

#define BLUEDOT_IP_LOOKUP_URL "&qip="
#define BLUEDOT_HASH_LOOKUP_URL "&qhash="
#define BLUEDOT_FILENAME_LOOKUP_URL "&qfilename="
#define BLUEDOT_URL_LOOKUP_URL "&qurl="

#define BLUEDOT_LOOKUP_IP 1
#define BLUEDOT_LOOKUP_HASH 2
#define BLUEDOT_LOOKUP_URL 3
#define BLUEDOT_LOOKUP_FILENAME 4

int Sagan_Bluedot_Cat_Compare ( unsigned char, int, unsigned char );
int Sagan_Bluedot ( _Sagan_Proc_Syslog *, int  );
unsigned char Sagan_Bluedot_Lookup(char *, unsigned char, int);			/* what to lookup,  lookup type */
int Sagan_Bluedot_IP_Lookup_All(char *, int);

void Sagan_Bluedot_Clean_Cache ( void );
void Sagan_Bluedot_Init(void);
void Sagan_Bluedot_Load_Cat(void);
void Sagan_Verify_Categories( char *, int , const char *, int, unsigned char );
void Sagan_Bluedot_Check_Cache_Time (void);

int Sagan_Bluedot_Clean_Queue ( char *, unsigned char );


typedef struct _Sagan_Bluedot_Cat_List _Sagan_Bluedot_Cat_List;
struct _Sagan_Bluedot_Cat_List {
    int		cat_number;
    char	cat[50];
};


typedef struct _Sagan_Bluedot_IP_Cache _Sagan_Bluedot_IP_Cache;
struct _Sagan_Bluedot_IP_Cache {
    unsigned char host[MAXIPBIT];
    uintmax_t mdate_utime;
    uintmax_t cdate_utime;
    uintmax_t cache_utime;
    int	alertid;
};

typedef struct _Sagan_Bluedot_Hash_Cache _Sagan_Bluedot_Hash_Cache;
struct _Sagan_Bluedot_Hash_Cache {
    char hash[SHA256_HASH_SIZE+1];
    uintmax_t cache_utime;
    int alertid;
};

typedef struct _Sagan_Bluedot_URL_Cache _Sagan_Bluedot_URL_Cache;
struct _Sagan_Bluedot_URL_Cache {
    char url[8192];
    uintmax_t cache_utime;
    int alertid;
};


typedef struct _Sagan_Bluedot_Filename_Cache _Sagan_Bluedot_Filename_Cache;
struct _Sagan_Bluedot_Filename_Cache {
    char filename[256];
    uintmax_t cache_utime;
    int alertid;
};

typedef struct _Sagan_Bluedot_IP_Queue _Sagan_Bluedot_IP_Queue;
struct _Sagan_Bluedot_IP_Queue {
    unsigned char host[MAXIPBIT];
};

typedef struct _Sagan_Bluedot_Hash_Queue _Sagan_Bluedot_Hash_Queue;
struct _Sagan_Bluedot_Hash_Queue {
    char hash[SHA256_HASH_SIZE+1];
};

typedef struct _Sagan_Bluedot_URL_Queue _Sagan_Bluedot_URL_Queue;
struct _Sagan_Bluedot_URL_Queue {
    char url[8192];
};

typedef struct _Sagan_Bluedot_Filename_Queue _Sagan_Bluedot_Filename_Queue;
struct _Sagan_Bluedot_Filename_Queue {
    char filename[256];
};


#endif

