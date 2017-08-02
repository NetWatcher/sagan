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

#include <syslog.h>

#define PCRE_OVECCOUNT		 30

/* Various buffers used during configurations loading */

#define SENSOR_NAME		"default_sensor_name"

#define CLASSBUF		1024
#define RULEBUF			5128
#define CONFBUF			4096

#define MAXPATH 		255		/* Max path for files/directories */
#define MAXHOST         	255		/* Max host length */
#define MAXPROGRAM		32		/* Max syslog 'program' length */
#define MAXDATE			25		/* Max syslog 'date' length */
#define MAXTIME			10		/* Max syslog 'time length */
#define MAXFACILITY		25		/* Max syslog 'facility' length */
#define MAXPRIORITY		20		/* Max syslog 'priority' length */
#define MAXTAG			32		/* Max syslog 'tag' length */
#define MAXLEVEL		15		/* Max syslog 'level' length */

#define MAX_PCRE_SIZE		1024		/* Max pcre length in a rule */

#define MAX_FIFO_SIZE		1048576		/* Max pipe/FIFO size in bytes/pages */

#define MAX_THREADS     	4096            /* Max system threads */
#define MAX_SYSLOGMSG   	10240		/* Max length of a syslog message */

#define MAX_VAR_NAME_SIZE  	64		/* Max "var" name size */
#define MAX_VAR_VALUE_SIZE 	4096		/* Max "var" value size */

#define MAX_PCRE		10		/* Max PCRE within a rule */
#define MAX_CONTENT		30		/* Max 'content' within a rule */
#define MAX_META_CONTENT	10		/* Max 'meta_content' within a rule */
#define MAX_XBITS		20		/* Max 'xbits' within a rule */

#define MAX_CHECK_FLOWS		50		/* Max amount of IP addresses to be checked in a flow */

#define MAX_REFERENCE		10		/* Max references within a rule */
#define MAX_PARSE_IP		10		/* Max IP to collect form log line via parse.c */

#define MAXIP			64		/* Max IP length */
#define MAXIPBIT	    16		/* Max IP length in bytes */
#define MAXSELECTOR		64		/* Max tracking selector length */

#define LOCKFILE 		"/var/run/sagan/sagan.pid"
#define SAGANLOG		"/var/log/sagan/sagan.log"
#define SAGANLOGPATH		"/var/log/sagan"
#define FIFO			"/var/run/sagan.fifo"
#define RULE_PATH		"/usr/local/etc/sagan-rules"

#define HOME_NET		"any"
#define EXTERNAL_NET		"any"

#define RUNAS			"sagan"

#define PLOG_INTERFACE		"eth0"
#define PLOG_FILTER		"port 514"
#define PLOG_LOGDEV		"/dev/log"

#define TRACK_TIME		1440

#define S_NORMAL		0
#define S_ERROR			1
#define S_WARN			2
#define S_DEBUG			3

#define DEFAULT_SYSLOG_FACILITY	LOG_AUTH
#define DEFAULT_SYSLOG_PRIORITY LOG_ALERT

#define PARSEIP_RETURN_STRING	0

#define DEFAULT_SMTP_SUBJECT 	"[Sagan]"

/* defaults if the user doesn't define */

#define MAX_PROCESSOR_THREADS   100

#define SUNDAY			1
#define MONDAY			2
#define TUESDAY			4
#define WEDNESDAY		8
#define THURSDAY		16
#define FRIDAY			32
#define SATURDAY		64

/* This is for loading/reloading Sagan log files */

#define OPEN			0
#define REOPEN			1

#define SAGAN_LOG		0
#define ALERT_LOG		1
#define ALL_LOGS		100

#define MD5_HASH_SIZE		32
#define SHA1_HASH_SIZE		40
#define SHA256_HASH_SIZE	64

#define MAX_FILENAME_SIZE	256
#define MAX_URL_SIZE		8192
#define MAX_USERNAME_SIZE	512
#define MAX_HOSTNAME_SIZE	255

/* Locations of IPC/Share memory "files" */

#define IPC_DIRECTORY			"/var/run/sagan"

#define COUNTERS_IPC_FILE 		"sagan-counters.shared"
#define XBIT_IPC_FILE 	     	        "sagan-xbits.shared"
#define THRESH_BY_SRC_IPC_FILE 		"sagan-thresh-by-source.shared"
#define THRESH_BY_DST_IPC_FILE 		"sagan-thresh-by-destination.shared"
#define THRESH_BY_DSTPORT_IPC_FILE 	"sagan-thresh-by-destination-port.shared"
#define THRESH_BY_SRCPORT_IPC_FILE 	"sagan-thresh-by-source-port.shared"
#define THRESH_BY_USERNAME_IPC_FILE 	"sagan-thresh-by-username.shared"
#define AFTER_BY_SRC_IPC_FILE 		"sagan-after-by-source.shared"
#define AFTER_BY_DST_IPC_FILE 		"sagan-after-by-destination.shared"
#define AFTER_BY_SRCPORT_IPC_FILE 	"sagan-after-by-source-port.shared"
#define AFTER_BY_DSTPORT_IPC_FILE 	"sagan-after-by-destination-port.shared"
#define AFTER_BY_USERNAME_IPC_FILE 	"sagan-after-by-username.shared"
#define CLIENT_TRACK_IPC_FILE 		"sagan-track-clients.shared"

/* Default IPC/mmap sizes */

#define DEFAULT_IPC_CLIENT_TRACK_IPC	10000
#define DEFAULT_IPC_AFTER_BY_SRC	1000000
#define DEFAULT_IPC_AFTER_BY_DST	1000000
#define DEFAULT_IPC_AFTER_BY_SRC_PORT   1000000
#define DEFAULT_IPC_AFTER_BY_DST_PORT	1000000
#define DEFAULT_IPC_AFTER_BY_USERNAME	10000
#define DEFAULT_IPC_THRESH_BY_SRC	1000000
#define DEFAULT_IPC_THRESH_BY_DST	1000000
#define DEFAULT_IPC_THRESH_BY_SRC_PORT	1000000
#define DEFAULT_IPC_THRESH_BY_DST_PORT  1000000
#define DEFAULT_IPC_THRESH_BY_USERNAME	10000
#define DEFAULT_IPC_XBITS		10000


#define AFTER_BY_SRC			1
#define AFTER_BY_DST			2
#define AFTER_BY_DSTPORT		3
#define AFTER_BY_USERNAME		4
#define AFTER_BY_SRCPORT                5

#define THRESH_BY_SRC			6
#define THRESH_BY_DST			7
#define THRESH_BY_DSTPORT		8
#define THRESH_BY_USERNAME		9
#define THRESH_BY_SRCPORT               10

#define XBIT				11

#define PARSE_HASH_MD5			1
#define	PARSE_HASH_SHA1			2
#define PARSE_HASH_SHA256		3
#define PARSE_HASH_ALL			4

#define NORMAL_RULE			0
#define DYNAMIC_RULE			1

#define XBIT_STORAGE_MMAP		0
#define XBIT_STORAGE_REDIS		1

#define	THREAD_NAME_LEN			16
