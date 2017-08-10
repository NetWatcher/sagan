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

/* liblognormalize.c
 *
 * These functions deal with liblognorm / data normalization.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"             /* From autoconf */
#endif

#ifdef HAVE_LIBLOGNORM

#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <liblognorm.h>
#include <json.h>

#include "sagan.h"
#include "sagan-defs.h"
#include "liblognormalize.h"
#include "sagan-config.h"

struct _SaganConfig *config;
struct _SaganDebug *debug;

struct _SaganNormalizeLiblognorm *SaganNormalizeLiblognorm = NULL;

pthread_mutex_t Lognorm_Mutex = PTHREAD_MUTEX_INITIALIZER;

/************************************************************************
 * liblognorm GLOBALS
 ************************************************************************/

struct stat liblognorm_fileinfo;
struct liblognorm_toload_struct *liblognormtoloadstruct;
int liblognorm_count;

static ln_ctx ctx;

struct _SaganCounters *counters;


/************************************************************************
 * Liblognorm_Load
 *
 * Load in the normalization files into memory
 ************************************************************************/

void Liblognorm_Load(char *infile)
{

    SaganNormalizeLiblognorm = malloc(sizeof(struct _SaganNormalizeLiblognorm));

    if ( SaganNormalizeLiblognorm == NULL ) {
        Sagan_Log(S_ERROR, "[%s, line %d] Failed to allocate memory for SaganNormalizeLiblognorm. Abort!", __FILE__, __LINE__);
    }

    memset(SaganNormalizeLiblognorm, 0, sizeof(_SaganNormalizeLiblognorm));

    if((ctx = ln_initCtx()) == NULL) {
        Sagan_Log(S_ERROR, "[%s, line %d] Cannot initialize liblognorm context.", __FILE__, __LINE__);
    }

    Sagan_Log(S_NORMAL, "Loading %s for normalization.", infile);

    /* Remember - On reload,  file access will be by the "sagan" user! */

    if (stat(infile, &liblognorm_fileinfo)) {
        Sagan_Log(S_ERROR, "[%s, line %d] Error accessing '%s'. Abort.", __FILE__, __LINE__, infile);
    }

    ln_loadSamples(ctx, infile);

}

/***********************************************************************
 * sagan_normalize_liblognom
 *
 * Locates interesting log data via Rainer's liblognorm library
 ***********************************************************************/

json_object *Normalize_Liblognorm(char *syslog_msg)
{

    char buf[10*1024] = { 0 };
    char tmp_host[254] = { 0 };

    int rc = 0;
    int rc_normalize = 0;

    const char *cstr = NULL;
    const char *tmp = NULL;

    struct json_object *json = NULL;

    json_object *string_obj;

    SaganNormalizeLiblognorm->ip_src[0] = '0';
    SaganNormalizeLiblognorm->ip_src[1] = '\0';
    SaganNormalizeLiblognorm->ip_dst[0] = '0';
    SaganNormalizeLiblognorm->ip_dst[1] = '\0';

    SaganNormalizeLiblognorm->selector[0] = '\0';

    SaganNormalizeLiblognorm->username[0] = '\0';
    SaganNormalizeLiblognorm->src_host[0] = '\0';
    SaganNormalizeLiblognorm->dst_host[0] = '\0';

    SaganNormalizeLiblognorm->hash_sha1[0] = '\0';
    SaganNormalizeLiblognorm->hash_sha256[0] = '\0';
    SaganNormalizeLiblognorm->hash_md5[0] = '\0';

    SaganNormalizeLiblognorm->http_uri[0] = '\0';
    SaganNormalizeLiblognorm->http_hostname[0] = '\0';

    SaganNormalizeLiblognorm->src_port = 0;
    SaganNormalizeLiblognorm->dst_port = 0;

    snprintf(buf, sizeof(buf),"%s", syslog_msg);

    /* int ln_normalize(ln_ctx ctx, const char *str, size_t strLen, struct json_object **json_p); */
    rc_normalize = ln_normalize(ctx, buf, strlen(buf), &json);

    cstr = (char*)json_object_to_json_string(json);

    /* Get source address information */

    json_object_object_get_ex(json, "src-ip", &string_obj);
    tmp = json_object_get_string(string_obj);

    if ( tmp != NULL) {
        snprintf(SaganNormalizeLiblognorm->ip_src, sizeof(SaganNormalizeLiblognorm->ip_src), "%s", tmp);
    }

    json_object_object_get_ex(json, "dst-ip", &string_obj);
    tmp = json_object_get_string(string_obj);

    if ( tmp != NULL ) {
        snprintf(SaganNormalizeLiblognorm->ip_dst, sizeof(SaganNormalizeLiblognorm->ip_dst), "%s", tmp);
    }

    /* Used for tracking in multi-tenant environment */

    if (config->selector_flag) {
        json_object_object_get_ex(json, config->selector_name[0] != '\0' ? config->selector_name : "selector", &string_obj);
        tmp = json_object_get_string(string_obj);

        if ( tmp != NULL ) {
            snprintf(SaganNormalizeLiblognorm->selector, sizeof(SaganNormalizeLiblognorm->selector), "%s", tmp);
        }
    }

    /* Get username information - Will be used in the future */

    json_object_object_get_ex(json, "username", &string_obj);
    tmp = json_object_get_string(string_obj);

    if ( tmp != NULL ) {
        snprintf(SaganNormalizeLiblognorm->username, sizeof(SaganNormalizeLiblognorm->username), "%s", tmp);
    }


    /* Do DNS lookup for source hostname */

    json_object_object_get_ex(json, "src-host", &string_obj);
    tmp = json_object_get_string(string_obj);

    if ( tmp != NULL ) {
        strlcpy(SaganNormalizeLiblognorm->src_host, tmp, sizeof(SaganNormalizeLiblognorm->src_host));

        if ( SaganNormalizeLiblognorm->ip_src[0] == '0' ) {

            rc = DNS_Lookup(SaganNormalizeLiblognorm->src_host, tmp_host, sizeof(tmp_host));

            if ( rc == -1 ) {
                Sagan_Log(S_WARN, "Failed to do a DNS lookup for source '%s'. Using '%s' instead.", SaganNormalizeLiblognorm->src_host, config->sagan_host);
                strlcpy(SaganNormalizeLiblognorm->src_host, config->sagan_host, sizeof(SaganNormalizeLiblognorm->src_host));
            }
        }

    }

    json_object_object_get_ex(json, "dst-host", &string_obj);
    tmp = json_object_get_string(string_obj);

    if ( tmp != NULL ) {
        strlcpy(SaganNormalizeLiblognorm->dst_host, tmp, sizeof(SaganNormalizeLiblognorm->dst_host));

        if ( SaganNormalizeLiblognorm->ip_dst[0] == '0' ) {

            rc = DNS_Lookup(SaganNormalizeLiblognorm->dst_host, tmp_host, sizeof(tmp_host));

            if ( rc == -1 ) {
                Sagan_Log(S_WARN, "Failed to do a DNS lookup for destination '%s'. Using '%s' instead.", SaganNormalizeLiblognorm->dst_host, config->sagan_host);
                strlcpy(SaganNormalizeLiblognorm->src_host, config->sagan_host, sizeof(SaganNormalizeLiblognorm->src_host));
            }
        }
    }

    /* Get port information */

    json_object_object_get_ex(json, "src-port", &string_obj);
    tmp = json_object_get_string(string_obj);

    if ( tmp != NULL ) {
        SaganNormalizeLiblognorm->src_port = atoi(tmp);
    }

    json_object_object_get_ex(json, "dst-port", &string_obj);
    tmp = json_object_get_string(string_obj);

    if ( tmp != NULL ) {
        SaganNormalizeLiblognorm->dst_port = atoi(tmp);
    }


    json_object_object_get_ex(json, "hash-md5", &string_obj);
    tmp = json_object_get_string(string_obj);

    if ( tmp != NULL ) {
        strlcpy(SaganNormalizeLiblognorm->hash_md5, tmp, sizeof(SaganNormalizeLiblognorm->hash_md5));
    }


    json_object_object_get_ex(json, "hash-sha1", &string_obj);
    tmp = json_object_get_string(string_obj);

    if ( tmp != NULL ) {
        strlcpy(SaganNormalizeLiblognorm->hash_sha1, tmp, sizeof(SaganNormalizeLiblognorm->hash_sha1));
    }

    json_object_object_get_ex(json, "hash-sha256", &string_obj);
    tmp = json_object_get_string(string_obj);

    if ( tmp != NULL ) {
        strlcpy(SaganNormalizeLiblognorm->hash_sha256, tmp, sizeof(SaganNormalizeLiblognorm->hash_sha256));
    }


    json_object_object_get_ex(json, "http_uri", &string_obj);
    tmp = json_object_get_string(string_obj);

    if ( tmp != NULL ) {
        strlcpy(SaganNormalizeLiblognorm->http_uri, tmp, sizeof(SaganNormalizeLiblognorm->http_uri));
    }

    json_object_object_get_ex(json, "http_hostname", &string_obj);
    tmp = json_object_get_string(string_obj);

    if ( tmp != NULL ) {
        strlcpy(SaganNormalizeLiblognorm->http_hostname, tmp, sizeof(SaganNormalizeLiblognorm->http_hostname));
    }

    json_object_object_get_ex(json, "filename", &string_obj);
    tmp = json_object_get_string(string_obj);

    if ( tmp != NULL ) {
        strlcpy(SaganNormalizeLiblognorm->filename, tmp, sizeof(SaganNormalizeLiblognorm->filename));
    }

    if ( debug->debugnormalize ) {
        Sagan_Log(S_DEBUG, "Liblognorm DEBUG output: %d", rc_normalize);
        Sagan_Log(S_DEBUG, "---------------------------------------------------");
        Sagan_Log(S_DEBUG, "Log message to normalize: |%s|", syslog_msg);
        Sagan_Log(S_DEBUG, "Parsed: %s", cstr);
        Sagan_Log(S_DEBUG, "Slector: %s", SaganNormalizeLiblognorm->selector);
        Sagan_Log(S_DEBUG, "Source IP: %s", SaganNormalizeLiblognorm->ip_src);
        Sagan_Log(S_DEBUG, "Destination IP: %s", SaganNormalizeLiblognorm->ip_dst);
        Sagan_Log(S_DEBUG, "Source Port: %d", SaganNormalizeLiblognorm->src_port);
        Sagan_Log(S_DEBUG, "Destination Port: %d", SaganNormalizeLiblognorm->dst_port);
        Sagan_Log(S_DEBUG, "Source Host: %s", SaganNormalizeLiblognorm->src_host);
        Sagan_Log(S_DEBUG, "Destination Host: %s", SaganNormalizeLiblognorm->dst_host);
        Sagan_Log(S_DEBUG, "Username: %s", SaganNormalizeLiblognorm->username);
        Sagan_Log(S_DEBUG, "MD5 Hash: %s", SaganNormalizeLiblognorm->hash_md5);
        Sagan_Log(S_DEBUG, "SHA1 Hash: %s", SaganNormalizeLiblognorm->hash_sha1);
        Sagan_Log(S_DEBUG, "SHA265 Hash: %s", SaganNormalizeLiblognorm->hash_sha256);
        Sagan_Log(S_DEBUG, "HTTP URI: %s", SaganNormalizeLiblognorm->http_uri);
        Sagan_Log(S_DEBUG, "HTTP HOSTNAME: %s", SaganNormalizeLiblognorm->http_hostname);
        Sagan_Log(S_DEBUG, "Filename: %s", SaganNormalizeLiblognorm->filename);

        Sagan_Log(S_DEBUG, "");
    }


    if (0 != rc_normalize && json) {
        json_object_put(json);
        json = NULL;
    }
    return json;
}

#endif
