/* $Id$ */
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

/* sagan.h
 *
 * Sagan prototypes and definitions.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"             /* From autoconf */
#endif

#include <json.h>
#include <stdio.h>
#include <stdint.h>
#include <pcre.h>
#include <time.h>
#include <arpa/inet.h>

#include "sagan-defs.h"

#ifdef HAVE_LIBMAXMINDDB
#include <maxminddb.h>
#endif

#ifndef HAVE_STRLCAT
size_t strlcat(char *, const char *, size_t );
#endif

#ifndef HAVE_STRLCPY
size_t strlcpy(char *, const char *, size_t );
#endif

/*
 * OS specific macro's for setting the thread name. "top" can display
 * this name. This was largely taken from Suricata.
 */

#if defined __FreeBSD__ /* FreeBSD */
/** \todo Add implementation for FreeBSD */
#define SetThreadName(n) ({ \
    char tname[16] = ""; \
    if (strlen(n) > 16) \
        Sagan_Log(S_WARN, "Thread name is too long, truncating it..."); \
    strlcpy(tname, n, 16); \
    pthread_set_name_np(pthread_self(), tname); \
    0; \
})
#elif defined __OpenBSD__ /* OpenBSD */
/** \todo Add implementation for OpenBSD */
#define SetThreadName(n) (0)
#elif defined OS_WIN32 /* Windows */
/** \todo Add implementation for Windows */
#define SetThreadName(n) (0)
#elif defined OS_DARWIN /* Mac OS X */
/** \todo Add implementation for MacOS */
#define SetThreadName(n) (0)
#elif defined HAVE_SYS_PRCTL_H /* PR_SET_NAME */
/**
 * \brief Set the threads name
 */
#define SetThreadName(n) ({ \
    char tname[THREAD_NAME_LEN + 1] = ""; \
    if (strlen(n) > THREAD_NAME_LEN) \
        Sagan_Log(S_WARN, "Thread name is too long, truncating it..."); \
    strlcpy(tname, n, THREAD_NAME_LEN); \
    int ret = 0; \
    if ((ret = prctl(PR_SET_NAME, tname, 0, 0, 0)) < 0) \
        Sagan_Log(S_WARN, "Error setting thread name \"%s\": %s", tname, strerror(errno)); \
    ret; \
})
#else
#define SetThreadName(n) (0)
#endif


typedef char sbool;	/* From rsyslog. 'bool' causes compatiablity problems on OSX.
			   "(small bool) I intentionally use char, to keep it slim so
		           that many fit into the CPU cache!".  */

sbool     Is_Numeric (char *);
void      To_UpperC(char* const );
void      To_LowerC(char* const );
int	      Check_Endian( void );
void      Usage( void );
void      Chroot( const char * );
void	  Remove_Return(char *);
int       Classtype_Lookup( const char *, char *, size_t size );
void      Remove_Spaces(char *);
void      Between_Quotes( char *, char *str, size_t size );
double    CalcPct(uintmax_t, uintmax_t);
void      Replace_String(char *, char *, char *, char *str, size_t size);
uintmax_t Value_To_Seconds (char *, uintmax_t);
void      Sagan_Log( int, const char *, ... );
void      Droppriv( void );
int       DNS_Lookup( char *, char *str, size_t size );
void      Var_To_Value(char *, char *str, size_t size);
sbool     IP2Bit (char *, unsigned char * );
sbool     Mask2Bit (int, unsigned char * );
const char *Bit2IP(unsigned char *, char *str, size_t size);
sbool     Validate_HEX (const char *);
void      Content_Pipe(char *, int, const char *, char *, size_t size);
sbool     is_notroutable ( unsigned char * );
sbool     is_inrange ( unsigned char *, unsigned char *, int );
void      Replace_Sagan( char *, char *, char *str, size_t size);
int       Character_Count ( char *, char *);
sbool     Wildcard( char *, char *);
void      Open_Log_File( sbool, int );
int       Check_Var(const char *);
sbool     Netaddr_To_Range( char *, unsigned char * );
void      Strip_Chars(const char *string, const char *chars, char *str, size_t size);
sbool     Is_IP (char *str);
sbool     File_Lock ( int );
sbool     File_Unlock ( int );
sbool     Check_Content_Not( char * );
uint32_t  Djb2_Hash( char * );
sbool     Starts_With(const char *str, const char *prefix);
char      *strrpbrk(const char *str, const char *accept);



#if defined(F_GETPIPE_SZ) && defined(F_SETPIPE_SZ)
void      Set_Pipe_Size( FILE * );
#endif


#ifdef __OpenBSD__
/* OpenBSD won't allow for this test:
 * "suricata(...): mprotect W^X violation" */
#ifndef PageSupportsRWX()
#define PageSupportsRWX() 0
#endif
#else
#ifndef HAVE_SYS_MMAN_H
#define PageSupportsRWX() 1
#else
int       PageSupportsRWX(void);
#endif /* HAVE_SYS_MMAN_H */
#endif


typedef struct _SaganDNSCache _SaganDNSCache;
struct _SaganDNSCache {

    char hostname[64];
    char src_ip[20];
};

typedef struct _Sagan_IPC_Counters _Sagan_IPC_Counters;
struct _Sagan_IPC_Counters {

    int  xbit_count;
    int	 thresh_count_by_src;
    int	 thresh_count_by_dst;
    int	 thresh_count_by_dstport;
    int  thresh_count_by_srcport;
    int	 thresh_count_by_username;
    int	 after_count_by_src;
    int	 after_count_by_dst;
    int  after_count_by_srcport;
    int	 after_count_by_dstport;
    int	 after_count_by_username;

    int	 track_client_count;
    int  track_clients_client_count;
    int  track_clients_down;

};

typedef struct _SaganCounters _SaganCounters;
struct _SaganCounters {

    uintmax_t threshold_total;
    uintmax_t after_total;
    uintmax_t sagantotal;
    uintmax_t saganfound;
    uintmax_t sagan_output_drop;
    uintmax_t sagan_processor_drop;
    uintmax_t sagan_log_drop;
    uintmax_t dns_cache_count;
    uintmax_t dns_miss_count;
    uintmax_t fwsam_count;
    uintmax_t ignore_count;
    uintmax_t blacklist_count;

    uintmax_t alert_total;

    uintmax_t malformed_host;
    uintmax_t malformed_facility;
    uintmax_t malformed_priority;
    uintmax_t malformed_level;
    uintmax_t malformed_tag;
    uintmax_t malformed_date;
    uintmax_t malformed_time;
    uintmax_t malformed_program;
    uintmax_t malformed_message;

    uintmax_t worker_thread_exhaustion;

    uintmax_t blacklist_hit_count;
    uintmax_t blacklist_lookup_count;

    int	     thread_output_counter;
    int	     thread_processor_counter;

    int	     xbit_total_counter;

    int      var_count;

    int	     dynamic_rule_count;

    int	     classcount;
    int      rulecount;
    int	     refcount;
    int      ruletotal;

    int      genmapcount;

    int      mapcount_message;
    int      mapcount_program;

    int	     droplist_count;

    int      brointel_addr_count;
    int      brointel_domain_count;
    int      brointel_file_hash_count;
    int      brointel_url_count;
    int      brointel_software_count;
    int      brointel_email_count;
    int      brointel_user_name_count;
    int      brointel_file_name_count;
    int      brointel_cert_hash_count;
    int      brointel_dups;

    int	      rules_loaded_count;

    uintmax_t follow_flow_total;			/* This will only be needed if follow_flow is an option */
    uintmax_t follow_flow_drop;	     		        /* Amount of flows that did not match and were dropped */

#ifdef HAVE_LIBMAXMINDDB
    uintmax_t geoip2_hit;				/* GeoIP2 hit count */
    uintmax_t geoip2_lookup;				/* Total lookups */
    uintmax_t geoip2_miss;				/* Misses (country not found) */
#endif

#ifdef WITH_BLUEDOT
    uintmax_t bluedot_ip_cache_count;                      /* Bluedot cache processor */
    uintmax_t bluedot_ip_cache_hit;                        /* Bluedot hit's from Cache */
    uintmax_t bluedot_ip_positive_hit;
    uintmax_t bluedot_ip_total;

    uintmax_t bluedot_mdate;					   /* Hits , but where over a modification date */
    uintmax_t bluedot_cdate;            	                   /* Hits , but where over a creation date */
    uintmax_t bluedot_mdate_cache;                                 /* Hits from cache , but where over a modification date */
    uintmax_t bluedot_cdate_cache;      			   /* Hits from cache , but where over a create date */
    uintmax_t bluedot_error_count;

    uintmax_t bluedot_hash_cache_count;
    uintmax_t bluedot_hash_cache_hit;
    uintmax_t bluedot_hash_positive_hit;
    uintmax_t bluedot_hash_total;

    uintmax_t bluedot_url_cache_count;
    uintmax_t bluedot_url_cache_hit;
    uintmax_t bluedot_url_positive_hit;
    uintmax_t bluedot_url_total;

    uintmax_t bluedot_filename_cache_count;
    uintmax_t bluedot_filename_cache_hit;
    uintmax_t bluedot_filename_positive_hit;
    uintmax_t bluedot_filename_total;

    int bluedot_cat_count;

#endif


#ifdef HAVE_LIBESMTP
    uintmax_t esmtp_count_success;
    uintmax_t esmtp_count_failed;
#endif

#ifdef HAVE_LIBHIREDIS
    uintmax_t redis_writer_threads_drop;
#endif

};

typedef struct _SaganDebug _SaganDebug;
struct _SaganDebug {

    sbool debugsyslog;
    sbool debugload;
    sbool debugfwsam;
    sbool debugexternal;
    sbool debugthreads;
    sbool debugxbit;
    sbool debugengine;
    sbool debugbrointel;
    sbool debugmalformed;
    sbool debuglimits;
    sbool debugipc;
    sbool debugjson;

#ifdef HAVE_LIBMAXMINDDB
    sbool debuggeoip2;
#endif

#ifdef HAVE_LIBLOGNORM
    sbool debugnormalize;
#endif

#ifdef HAVE_LIBESMTP
    sbool debugesmtp;
#endif

#ifdef HAVE_LIBPCAP
    sbool debugplog;
#endif

#ifdef WITH_BLUEDOT
    sbool debugbluedot;
#endif

#ifdef HAVE_LIBHIREDIS
    sbool debugredis;
#endif

};

#ifdef HAVE_LIBHIREDIS

typedef struct _Sagan_Redis _Sagan_Redis;
struct _Sagan_Redis {
    char redis_command[2048];
};

#endif

typedef struct _Sagan_Proc_Syslog _Sagan_Proc_Syslog;
struct _Sagan_Proc_Syslog {
    char syslog_host[50];
    char syslog_facility[50];
    char syslog_priority[50];
    char syslog_level[50];
    char syslog_tag[50];
    char syslog_date[50];
    char syslog_time[50];
    char syslog_program[50];
    char syslog_message[MAX_SYSLOGMSG];

};

typedef struct _Sagan_Event _Sagan_Event;
struct _Sagan_Event {

    char *ip_src;
    char *ip_dst;
    int   dst_port;
    int   src_port;

    char *selector;

    struct timeval event_time;

    int  found;

    char *fpri;             /* *priority */

    sbool endian;
    sbool drop;

    char *f_msg;

    /* message information */

    char *time;
    char *date;

    char *priority;         /* Syslog priority */
    char *host;
    char *facility;
    char *level;
    char *tag;
    char *program;
    char *message;

    char *sid;
    char *rev;
    char *class;
    int pri;
    int ip_proto;

    char *normalize_http_uri;
    char *normalize_http_hostname;

    unsigned long generatorid;
    unsigned long alertid;

    json_object *json_normalize;
};


/* Thresholding structure by source */

typedef struct thresh_by_src_ipc thresh_by_src_ipc;
struct thresh_by_src_ipc {
    unsigned char ipsrc[MAXIPBIT];
    int  count;
    uintmax_t utime;
    char sid[20];
    int expire;
    char selector[MAXSELECTOR]; 
};


/* Thresholding structure by destination */

typedef struct thresh_by_dst_ipc thresh_by_dst_ipc;
struct thresh_by_dst_ipc {
    unsigned char ipdst[MAXIPBIT];
    int  count;
    uintmax_t utime;
    char sid[20];
    int expire;
    char selector[MAXSELECTOR]; 
};


/* Thresholding structure by source port */

typedef struct thresh_by_srcport_ipc thresh_by_srcport_ipc;
struct thresh_by_srcport_ipc {
    uint32_t ipsrcport;
    int  count;
    uintmax_t utime;
    char sid[20];
    int expire;
    char selector[MAXSELECTOR]; 
};

/* Thresholding structure by destination port */

typedef struct thresh_by_dstport_ipc thresh_by_dstport_ipc;
struct thresh_by_dstport_ipc {
    uint32_t ipdstport;
    int  count;
    uintmax_t utime;
    char sid[20];
    int expire;
    char selector[MAXSELECTOR]; 
};


/* Thesholding structure by username */

typedef struct thresh_by_username_ipc thresh_by_username_ipc;
struct thresh_by_username_ipc {
    char username[128];
    int  count;
    uintmax_t utime;
    char sid[20];
    int expire;
    char selector[MAXSELECTOR]; 
};

/* After structure by source */

typedef struct after_by_src_ipc after_by_src_ipc;
struct after_by_src_ipc {
    unsigned char ipsrc[MAXIPBIT];
    uintmax_t count;
    uintmax_t total_count;
    uintmax_t utime;
    char sid[20];
    int expire;
    char selector[MAXSELECTOR]; 
};

/* After structure by destination */

typedef struct after_by_dst_ipc after_by_dst_ipc;
struct after_by_dst_ipc {
    unsigned char ipdst[MAXIPBIT];
    int  count;
    uintmax_t total_count;
    uintmax_t utime;
    char sid[20];
    int expire;
    char selector[MAXSELECTOR]; 
};

/* After structure by source port */

typedef struct after_by_srcport_ipc after_by_srcport_ipc;
struct after_by_srcport_ipc {
    uint32_t ipsrcport;
    uintmax_t count;
    uintmax_t total_count;
    uintmax_t utime;
    char sid[20];
    int expire;
    char selector[MAXSELECTOR]; 
};

/* After structure by destination port */

typedef struct after_by_dstport_ipc after_by_dstport_ipc;
struct after_by_dstport_ipc {
    uint32_t ipdstport;
    uintmax_t count;
    uintmax_t total_count;
    uintmax_t utime;
    char sid[20];
    int expire;
    char selector[MAXSELECTOR]; 
};

/* After structure by username */

typedef struct after_by_username_ipc after_by_username_ipc;
struct after_by_username_ipc {
    char username[128];
    uintmax_t count;
    uintmax_t total_count;
    uintmax_t utime;
    char sid[20];
    int expire;
    char selector[MAXSELECTOR]; 
};

typedef struct _SaganVar _SaganVar;
struct _SaganVar {
    char var_name[MAX_VAR_NAME_SIZE];
    char var_value[MAX_VAR_VALUE_SIZE];
};

typedef struct _Sagan_Processor_Info _Sagan_Processor_Info;
struct _Sagan_Processor_Info {

    char *processor_name;
    char *processor_facility;
    char *processor_priority;		/* Syslog priority */
    int   processor_pri;		/* Sagan priority */
    char *processor_class;
    char *processor_tag;
    char *processor_rev;
    int   processor_generator_id;
};

/* IP Lookup cache */

typedef struct _Sagan_Lookup_Cache_Entry _Sagan_Lookup_Cache_Entry;
struct _Sagan_Lookup_Cache_Entry {
    sbool searched;
    ptrdiff_t offset;
    char ip[MAXIP];
};


/* Function that require the above arrays */

int64_t   FlowGetId( _Sagan_Event *);

