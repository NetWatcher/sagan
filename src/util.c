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

/* util.c
 *
 * Various re-usable functions.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"             /* From autoconf */
#endif

#include <stdio.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <stdarg.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <fcntl.h>


#include "sagan.h"
#include "sagan-defs.h"
#include "sagan-config.h"
#include "lockfile.h"

#include "parsers/strstr-asm/strstr-hook.h"

#include "version.h"

struct _SaganConfig *config;
struct _SaganCounters *counters;
struct _SaganVar *var;
struct _Sagan_Processor_Generator *generator;

sbool daemonize;
sbool quiet;

/*****************************************************************************
 * This force Sagan to chroot.
 *
 * Note: printf/fprints are used,  because we actually chroot before the log
 * it initalized
 *****************************************************************************/

void Chroot(const char *chrootdir )
{

    printf("[*] Chroot to %s\n", chrootdir);

    if (chroot(chrootdir) != 0 || chdir ("/") != 0) {
        fprintf(stderr, "[E] Could not chroot to '%s'.\n",  chrootdir);
        exit(1);		/* sagan.log isn't open yet */
    }
}

/************************************************
 * Drop priv's so we aren't running as "root".
 ************************************************/

void Droppriv(void)
{

    struct stat fifocheck;
    struct passwd *pw = NULL;
    int ret;

    pw = getpwnam(config->sagan_runas);

    if (!pw) {
        Sagan_Log(S_ERROR, "Couldn't locate user '%s'. Aborting...", config->sagan_runas);
    }

    if ( getuid() == 0 ) {

        Sagan_Log(S_NORMAL, "Setting permissions and dropping privileges! [UID: %lu GID: %lu]", (unsigned long)pw->pw_uid, (unsigned long)pw->pw_gid);

        /*
         * We chown certain log files to our Sagan user.  This is done so no files are "owned"
         * by "root".  This prevents problems in the future when doing things like handling
             * SIGHUP's and what not.
             *
             * Champ Clark (04/14/2015)
             */

        if ( config->sagan_is_file == false ) {	/* Don't change ownsership/etc if we're processing a file */

            ret = chown(config->sagan_fifo, (unsigned long)pw->pw_uid,(unsigned long)pw->pw_gid);

            if ( ret < 0 ) {
                Sagan_Log(S_ERROR, "[%s, line %d] Cannot change ownership of %s to username %s - %s", __FILE__, __LINE__, config->sagan_fifo, config->sagan_runas, strerror(errno));
            }

            if (stat(config->sagan_fifo, &fifocheck) != 0 ) {
                Sagan_Log(S_ERROR, "[%s, line %d] Cannot open %s FIFO - %s!",  __FILE__, __LINE__, config->sagan_fifo, strerror(errno));
            }

        }

        if (initgroups(pw->pw_name, pw->pw_gid) != 0 ||
            setgid(pw->pw_gid) != 0 || setuid(pw->pw_uid) != 0) {
            Sagan_Log(S_ERROR, "[%s, line %d] Could not drop privileges to uid: %lu gid: %lu - %s!", __FILE__, __LINE__, (unsigned long)pw->pw_uid, (unsigned long)pw->pw_gid, strerror(errno));
        }

    } else {
        Sagan_Log(S_NORMAL, "Not dropping privileges.  Already running as a non-privileged user");
    }
}

/********************
 * Remove new-lines
 ********************/

void Remove_Return(char *s)
{
    char *s1, *s2;
    for(s1 = s2 = s; *s1; *s1++ = *s2++ )
        while( *s2 == '\n' )s2++;
}

/***********************************************
 * Removes spaces from certain rule fields, etc
 ***********************************************/

void Remove_Spaces(char *s)
{
    char *s1, *s2;
    for(s1 = s2 = s; *s1; *s1++ = *s2++ )
        while( *s2 == ' ')s2++;
}

/**********************************
 * Shift a string to all uppercase
 **********************************/

void To_UpperC(char *const s)
{
    char* cur = s;
    while (*cur) {
        *cur = toupper(*cur);
        ++cur;
    }
}

/**********************************
 * Shift a string to all lowercase
 **********************************/

void To_LowerC(char *const s)
{
    char* cur = s;
    while (*cur) {
        *cur = tolower(*cur);
        ++cur;
    }
}

/******************************************************
 * Generic "sagan.log" style logging and screen output.
 *******************************************************/

void Sagan_Log (int type, const char *format,... )
{

    char buf[5128] = { 0 };
    va_list ap;
    va_start(ap, format);
    char *chr="*";
    char curtime[64];
    time_t t;
    struct tm *now;
    t = time(NULL);
    now=localtime(&t);
    strftime(curtime, sizeof(curtime), "%m/%d/%Y %H:%M:%S",  now);

    if ( type == 1 ) {
        chr="E";
    }

    if ( type == 2 ) {
        chr="W";
    }

    if ( type == 3 ) {
        chr="D";
    }

    vsnprintf(buf, sizeof(buf), format, ap);
    fprintf(config->sagan_log_stream, "[%s] [%s] - %s\n", chr, curtime, buf);
    fflush(config->sagan_log_stream);

    if ( config->daemonize == 0 && config->quiet == 0 ) {
        printf("[%s] %s\n", chr, buf);
    }

    if ( type == 1 ) {
        exit(1);
    }

}

/******************************************
 * Check if system is big || little endian
 ******************************************/

int Check_Endian()
{
    int i = 1;
    char *p = (char *) &i;
    if (p[0] == 1)  /* Lowest address contains the least significant byte */
        return 0;   /* Little endian */
    else
        return 1;   /* Big endian */
}


sbool Mask2Bit(int mask, unsigned char *out) {
    int i;
    bool ret = false;

    if (mask < 1 || mask > 128) {
        return false;
    }

    ret = true;

    for (i=0;i<mask;i+=8) {
        out[i/8] = i+8 <= mask ? 0xff : ~((1 << (8 - mask%8)) - 1);
    }
    return ret;

}


/* Converts IP address.  We assume that out is at least 16 bytes.  */
sbool IP2Bit (char *ipaddr, unsigned char *out)
{

    sbool ret = false;
    struct addrinfo hints = {0};
    struct addrinfo *result = NULL;

    // Use getaddrinfo so we can get ipv4 or 6
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE|AI_NUMERICHOST;

    ret = getaddrinfo(ipaddr, NULL, &hints, &result) == 0;
    if (!ret) {
        Sagan_Log(S_WARN, "Warning: Got a getaddrinfo() error for \"%s\" but continuing...", ipaddr);
    } else {
        switch (((struct sockaddr_storage *)result->ai_addr)->ss_family) {
        case AF_INET:
            ret = true;
            if (NULL != out) {
                memcpy(out, &((struct sockaddr_in *)result->ai_addr)->sin_addr, sizeof(((struct sockaddr_in *)0)->sin_addr));
            }
            break;
        case AF_INET6:
            ret = true;
            if (NULL != out) {
                memcpy(out, &((struct sockaddr_in6 *)result->ai_addr)->sin6_addr, sizeof(((struct sockaddr_in6 *)0)->sin6_addr));
            }
            break;
        default:
            Sagan_Log(S_WARN, "Warning: Got a getaddrinfo() received a non IPv4/IPv6 address for \"%s\" but continuing...", ipaddr);
        }
    } 
    if (NULL != result) {
        freeaddrinfo(result);
    }

    return ret;
}

/****************************************
 * Check if string contains only numbers
 ****************************************/

sbool Is_Numeric (char *str)
{

    if(strlen(str) == strspn(str, "0123456789")) {
        return(true);
    } else {
        return(false);
    }

}

/***************************************************************************
 * Grab's information between "quotes" and returns it.  Use for things like
 * parsing msg: and pcre
 ***************************************************************************/

void Between_Quotes(char *instr, char *str, size_t size)
{
    sbool flag=0;
    int i;

    char tmp1[2];
    char tmp2[512] = { 0 };

    for ( i=0; i<strlen(instr); i++) {

        if ( flag == 1 && instr[i] == '\"' ) {
            flag = 0;
        }

        if ( flag == 1 ) {
            snprintf(tmp1, sizeof(tmp1), "%c", instr[i]);
            strlcat(tmp2, tmp1, sizeof(tmp2));
        }

        if ( instr[i] == '\"' ) flag++;

    }

    snprintf(str, size, "%s", tmp2);
}

/*****************************
 * CalcPct (Taken from Snort)
 *****************************/

double CalcPct(uintmax_t cnt, uintmax_t total)
{
    double pct = 0.0;

    if (total == 0.0) {
        pct = (double)cnt;
    } else {
        pct = (double)cnt / (double)total;
    }

    pct *= 100.0;

    return pct;
}

/********************************************************************
 * DNS lookup of hostnames.  Wired for IPv4 and IPv6.  Code largely
 * based on Beej's showip.c
 ********************************************************************/

int DNS_Lookup( char *host, char *str, size_t size )
{

    char ipstr[INET6_ADDRSTRLEN] = { 0 };

    struct addrinfo hints = {0}, *res = NULL;
    int status;
    void *addr;

    /* Short circuit if it's a "localhost" lookup */

    if ( !strcmp(host, "localhost" ) ) {
        snprintf(str, size, "%s", config->sagan_host);
        return(0);
    }

    if ( config->disable_dns_warnings == 0 ) {

        Sagan_Log(S_WARN, "--------------------------------------------------------------------------");
        Sagan_Log(S_WARN, "Sagan DNS lookup need for '%s'.", host);
        Sagan_Log(S_WARN, "This can affect performance.  Please see:" );
        Sagan_Log(S_WARN, "https://wiki.quadrantsec.com/bin/view/Main/SaganDNS");
        Sagan_Log(S_WARN, "--------------------------------------------------------------------------");
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;     /* AF_INET or AF_INET6 to force version */
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(host, NULL, &hints, &res)) != 0) {

        Sagan_Log(S_WARN, "%s: %s", gai_strerror(status), host);
        return -1;

    }

    if (res->ai_family == AF_INET) { /* IPv4 */

        struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
        addr = &(ipv4->sin_addr);

    } else { /* IPv6 */

        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)res->ai_addr;
        addr = &(ipv6->sin6_addr);

    }

    inet_ntop(res->ai_family, addr, ipstr, sizeof ipstr);
    freeaddrinfo(res);

    snprintf(str, size, "%s", ipstr);
    return 0;
}

/****************************************************************
 * String replacement function.  Used for things like $RULE_PATH
 ****************************************************************/

void Replace_String(char *in_str, char *orig, char *rep, char *str, size_t size)
{

    char buffer[4096] = { 0 };
    char *p = NULL;

    if(!(p = strstr(in_str, orig))) {
        snprintf(str, size, "%s", in_str);
        return;
    }

    strlcpy(buffer, in_str, p-in_str);
    buffer[p-in_str] = '\0';
    sprintf(buffer+(p-in_str), "%s%s", rep, p+strlen(orig));

    snprintf(str, size, "%s", buffer);

}

sbool is_inrange ( unsigned char *ip, unsigned char *tests, int count) {
    int i,j,k;
    sbool inrange = false;
    for (i=0;i<count;i++) {
        inrange = true;
        // We can stop if the mask is 0.  We only handle wellformed masks.
        for(j=0,k=16;j<16 && tests[i+k] != 0x00;j++,k++) {
            if((tests[i+j] & tests[i+k]) != (ip[j] & tests[i+k])) {
                inrange = false;
                break;
            }
        }
        if (inrange) {
            break;
        }
    }
    return inrange;
}

/****************************************************************************/
/* is_notroutable                                                           */
/*                                                                          */
/* Checks to see if an ip address is routable or not                        */
/****************************************************************************/

sbool is_notroutable ( unsigned char *ip )
{
    sbool notroutable = false;
    // Start of subnet followd by mask
    static unsigned char tests[][32] = {
        // IPv6 Multicast - ff00::/8
        {
            0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        },
        // IPv6 Link Local fe80::/10
        {
            0xFE, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0xFF, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        },
        // IPv6 RFC4193 - fc00::/7
        {
            0xFC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0xFE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        },
        // IPv6  LocalHost - ::1/128
        {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
        },
        // IPv4 RFC1918 - 10.0.0.0/8
        {
            0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        },
        // IPv4-mapped RFC1918 - ::ffff:10.0.0.0/104
        {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0xFF, 0xFF, 0xA0, 0x00, 0x00, 0x00,
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00
        },
        // IPv4 RFC1918 - 192.168.0.0/16
        {
            0xC0, 0xA8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        },
        // IPv4-mapped RFC1918 - ::ffff:192.168.0.0/112
        {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0xFF, 0xFF, 0xC0, 0xA8, 0x00, 0x00,
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00
        },
        // IPv4 localhost - 127.0.0.0/8
        {
            0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        },
        // IPv4-mapped localhost - ::ffff:127.0.0.0/104
        {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0xFF, 0xFF, 0x7F, 0x00, 0x00, 0x00,
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00
        },
        // IPv4  Mulitcast - 224.0.0.0/4
        {
            0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        },
        // IPv4-mapped  Mulitcast - ::ffff:224.0.0.0/100
        {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0xFF, 0xFF, 0xE0, 0x00, 0x00, 0x00,
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
            0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0x00, 0x00, 0x00
        },
        // IPv4  Broadcast - 255.255.255.255/32
        {
            0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        },
        // IPv4-mapped  Broadcast - ::ffff:255.255.255.255/128
        {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
        }
    };
    return is_inrange(ip, (unsigned char *)tests, sizeof(tests)/(sizeof(char[32])));
}

/****************************************************************************
 * Var_To_Value - Changes a variable in a configuration file (for
 * example - $RULE_PATH into it's true value.
 ****************************************************************************/

void Var_To_Value(char *in_str, char *str, size_t size)
{

    char *ptmp = NULL;
    char *tok = NULL;
    char tmp2[MAX_VAR_VALUE_SIZE] = { 0 };
    char tmp3[MAX_VAR_VALUE_SIZE] = { 0 };
    char tmp_result[MAX_VAR_VALUE_SIZE] = { 0 };
    char tmp[MAX_VAR_VALUE_SIZE] = { 0 };

    int i=0;

    snprintf(tmp, sizeof(tmp), "%s", in_str);		/* Segfault with strlcpy */

    for (i=0; i<counters->var_count; i++) {

        ptmp = strtok_r(tmp, " ", &tok);

        while (ptmp != NULL ) {

            Replace_String(ptmp, var[i].var_name, var[i].var_value, tmp2, sizeof(tmp2));
            snprintf(tmp3, sizeof(tmp3), "%s ", tmp2);
            strlcat(tmp_result, tmp3, sizeof(tmp_result));
            ptmp = strtok_r(NULL, " ", &tok);
        }

        strlcpy(tmp, tmp_result, sizeof(tmp));
        memset(tmp_result, 0, sizeof(tmp_result));
    }


    tmp[strlen(tmp)-1] = 0;		/* Remove trailing space */

    snprintf(str, size, "%s", tmp);

}

/****************************************************************************
 * Validate_HEX - Makes sure a string is valid hex.
 ****************************************************************************/

sbool Validate_HEX (const char *string)
{

    const char *curr = string;

    while (*curr != 0) {
        if (('A' <= *curr && *curr <= 'F') || ('a' <= *curr && *curr <= 'f') || ('0' <= *curr && *curr <= '9')) {
            ++curr;
        } else {
            return(false);
        }
    }
    return(true);
}

/****************************************************************************
 * Check_Var - Checks to make sure a "var" is present in memory
 ****************************************************************************/

int Check_Var(const char *string)
{

    int i;
    int flag = 0;

    for (i=0; i<counters->var_count; i++) {

        if (!strcmp(string, var[i].var_name)) {
            flag = 1;
            break;
        }
    }

    return(flag);
}


/************************************************************************************************
 * This is for |HEX| support (like in Snort).  From example: content: "User |3a 3c 53| and such";
 * If the content has no pipes,  we leave it unaltered.  If it has pipes,  we insert the ASCII
 * values of the Hex within the content (keeping formating correct - Champ Clark - 12/04/2013
 * Move to this function 05/05/2014 - Champ Clark
 *************************************************************************************************/

void Content_Pipe(char *in_string, int linecount, const char *ruleset, char *str, size_t size )
{

    int pipe_flag = 0;

    static char final_content[512] = { 0 };
    memset(final_content,0,sizeof(final_content));

    char final_content_tmp[512] = { 0 };
    char tmp2[512];
    int i;
    int x;
    char tmp[2];

    strlcpy(tmp2, in_string, sizeof(tmp2));

    pipe_flag = 0;

    for ( i=0; i<strlen(tmp2); i++) {

        if ( tmp2[i] == '|' && pipe_flag == 0 ) {
            pipe_flag = 1;              /* First | has been found */
        }

        /* If we haven't found any |'s,  just copy the content verbatium */

        if ( pipe_flag == 0 ) {
            snprintf(final_content_tmp, sizeof(final_content_tmp), "%c", tmp2[i]);
            strncat(final_content, final_content_tmp, 1);
        }

        /* If | has been found,  start the conversion */

        if ( pipe_flag == 1 ) {

            if ( tmp2[i+1] == ' ' || tmp2[i+2] == ' ' ) {
                Sagan_Log(S_ERROR, "The 'content' option with hex formatting (|HEX|) appears to be incorrect. at line %d in %s", linecount, ruleset);
            }

            snprintf(final_content_tmp, sizeof(final_content_tmp), "%c%c", tmp2[i+1], tmp2[i+2]);       /* Copy the hex value - ie 3a, 1B, etc */

            if (!Validate_HEX(final_content_tmp)) {
                Sagan_Log(S_ERROR, "Invalid '%s' Hex detected at line %d in %s", final_content_tmp, linecount, ruleset);
            }

            sscanf(final_content_tmp, "%x", &x);        /* Convert hex to dec */
            snprintf(tmp, sizeof(tmp), "%c", x);        /* Convert dec to ASCII */
            strncat(final_content, tmp, 1);             /* Append value */

            /* Last | found,  but continue processing rest of content as normal */

            if ( tmp2[i+3] == '|' ) {
                pipe_flag = 0;
                i=i+3;
            } else {
                i = i+2;
            }
        }

    }

    snprintf(str, size, "%s", final_content);
}

/****************************************************************************
 * Replace_Sagan() - Take the %sagan% out of a string and replaces it
 * with *replace
 ****************************************************************************/

void Replace_Sagan( char *string_in, char *replace, char *str, size_t size)
{

    char string[1024] = { 0 };
    char tmp[2] = { 0 };

    char new_string[CONFBUF] = { 0 };

    int i;

    strlcpy(string, string_in, sizeof(string));

    for (i = 0; i < strlen(string); i++) {

        if ( string[i] == '%' ) {

            if ( string[i+1] == 's' && string[i+2] == 'a' && string[i+3] == 'g' &&
                 string[i+4] == 'a' && string[i+5] == 'n' && string[i+6] == '%' ) {

                strlcat(new_string, replace, sizeof(new_string));

                i = i + 6;  /* Skip to end of %sagan% */

            } else {

                strlcat(new_string, "%", sizeof(new_string));
            }
        } else {

            snprintf(tmp, sizeof(tmp), "%c", string[i]);
            strlcat(new_string, tmp, sizeof(new_string));

        }
    }


    snprintf(str, size, "%s", new_string);
}


/****************************************************************************
 * Character_Count - Simple routine that "counts" the number of
 * time "char_to_count" (single character) occurs.   Returns the int
 * value of what it found
 ****************************************************************************/

int Character_Count ( char *string_in, char *char_to_count)
{

    char str_to_count[128] = { 0 };
    char tmp[2] = { 0 };

    int i = 0;
    int to_count = 0;
    int return_count = 0;

    /* Convert to usable types */

    strlcpy(tmp, char_to_count, 2);
    strlcpy(str_to_count, string_in, sizeof(str_to_count));

    to_count = (int)tmp[0];

    for (i = 0; i < strlen(str_to_count); i++) {

        /* Search for and count int char[i] */

        if ( (int)str_to_count[i] == to_count ) {
            return_count++;
        }
    }

    return(return_count);
}

/****************************************************************************
 * Wildcard - Used for comparing strings with wildcard support.  This
 * function was taken from:
 *
 * http://www.geeksforgeeks.org/wildcard-character-matching/
 *
 * They had a much better solution than mine!
 ****************************************************************************/

sbool Wildcard( char *first, char *second )
{
    if (*first == '\0' && *second == '\0') {
        return true;
    }

    if (*first == '*' && *(first+1) != '\0' && *second == '\0') {
        return false;
    }

    if (*first == '?' || *first == *second) {
        return Wildcard(first+1, second+1);
    }

    if (*first == '*') {
        return Wildcard(first+1, second) || Wildcard(first, second+1);
    }

    return false;
}

void CloseStream( FILE *stream, int *fd ) {
    if (NULL != stream) {
        fclose(stream);
    }
    if (NULL != fd && *fd >= 0) {
        close(*fd);
        *fd = -1;
    }
}

FILE *OpenStream( char *path, int *fd, unsigned long pw_uid, unsigned long pw_gid ) {
    FILE *ret = NULL;
    char *_path = NULL;
    struct sockaddr_un name = {0};

    if (NULL == fd || NULL == path) {
        fprintf(stderr, "[E] [%s, line %d] Invalid (null) argument(s) passed to OpenStream!\n", __FILE__, __LINE__);
    }

    _path = strstr(path, "://");

    if (NULL == _path) {
        _path = path;
    } else {
        _path += 3;
    }

    // TODO: Add cases here for UDP and TCP
    if (Starts_With(path, "unix://")) {
        /* Create socket from which to write. Currently only stream mode is supported */
        *fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (*fd < 0) {
            fprintf(stderr, "[E] [%s, line %d] Could not init unix socket. Failed to open socket at %s - %s!\n", __FILE__, __LINE__ , _path, strerror(errno));
        }
        /* Create name. */
        name.sun_family = AF_UNIX;
        strncpy(name.sun_path, _path, sizeof(name.sun_path)-1);

        /* Bind the UNIX domain address to the created socket */
        if (connect(*fd, (struct sockaddr *) &name, sizeof(struct sockaddr_un))) {
            fprintf(stderr, "[E] [%s, line %d] Could not init unix socket. Failed to connect to socket %s - %s!\n", __FILE__, __LINE__, _path, strerror(errno));
            goto ErrorExit;
        } else {
            Sagan_Log(S_NORMAL, "[%s, line %d] Connected to unix socket: %s: %d", __FILE__, __LINE__, name.sun_path, *fd);
            ret = fdopen(*fd, "a");
        }
    } else {
        *fd = -1;
        ret = fopen(_path, "a");
    }

    /* Chown the log files in case we get a SIGHUP or whatnot later (due to Sagan_Chroot()) */
    if ( chown(_path, pw_uid,pw_gid) < 0 ) {
        Sagan_Log(S_ERROR, "[%s, line %d] Cannot change ownership of %s to username %s - %s", __FILE__, __LINE__, _path, config->sagan_runas, strerror(errno));
    }

ErrorExit:

    if (NULL == ret && *fd >= 0) {
        close(*fd);
        *fd = -1;
    }

    return ret;
}

/****************************************************************************
 * Open_Log_File - This controls the opening and/or re-opening of log
 * files.  This is useful for situation like SIGHUP,  where we want to
 * close a file handle and start a new one.  Think of 'logrotate'.
 ****************************************************************************/

void Open_Log_File( sbool state, int type )
{

    struct passwd *pw = NULL;
    int ret;

    pw = getpwnam(config->sagan_runas);

    if( pw == NULL) {
        fprintf(stderr, "[E] [%s, line %d] Invalid user %s (use -u option to set a user)\n", __FILE__, __LINE__, config->sagan_runas);
        exit(1);
    }

    if ( type == SAGAN_LOG || type == ALL_LOGS ) {

        /* For SIGHUP */

        if ( state == REOPEN ) {
            CloseStream(config->sagan_log_stream, &config->sagan_log_fd);
        }

        if ((config->sagan_log_stream = OpenStream(config->sagan_log_filepath, &config->sagan_log_fd,(unsigned long)pw->pw_uid,(unsigned long)pw->pw_gid)) == NULL) {
            fprintf(stderr, "[E] [%s, line %d] Cannot open %s - %s!\n", __FILE__, __LINE__, config->sagan_log_filepath, strerror(errno));
            exit(1);
        }
    }


    if ( type == ALERT_LOG || type == ALL_LOGS ) {

        /* For SIGHUP */

        if ( state == REOPEN && config->eve_flag == true ) {
            CloseStream(config->eve_stream, &config->eve_fd);
        }

        if ( state == REOPEN && config->alert_flag == true ) {
            CloseStream(config->sagan_alert_stream, &config->sagan_alert_fd);
        }

        if ( state == REOPEN && config->fast_flag == true ) {
            CloseStream(config->sagan_fast_stream, &config->sagan_fast_fd);
        }

        if ( config->eve_flag ) {

            if (( config->eve_stream = OpenStream(config->eve_filename, &config->eve_fd, (unsigned long)pw->pw_uid, (unsigned long)pw->pw_gid )) == NULL ) {
                Remove_Lock_File();
                Sagan_Log(S_ERROR, "[%s, line %d] Can't open \"%s\" - %s!", __FILE__, __LINE__, config->eve_filename, strerror(errno));
            }


        }
        if ( config->fast_flag ) {

            if (( config->sagan_fast_stream = OpenStream(config->fast_filename, &config->sagan_fast_fd, (unsigned long)pw->pw_uid, (unsigned long)pw->pw_gid )) == NULL ) {
                Remove_Lock_File();
                Sagan_Log(S_ERROR, "[%s, line %d] Can't open %s - %s!", __FILE__, __LINE__, config->fast_filename, strerror(errno));
            }

        }


        if ( config->alert_flag ) {

            if (( config->sagan_alert_stream = OpenStream(config->sagan_alert_filepath, &config->sagan_alert_fd, (unsigned long)pw->pw_uid, (unsigned long)pw->pw_gid )) == NULL ) {
                Remove_Lock_File();
                Sagan_Log(S_ERROR, "[%s, line %d] Can't open %s - %s!", __FILE__, __LINE__, config->sagan_alert_filepath, strerror(errno));
            }

        }
    }

}

/****************************************************************************
 * Set_Pipe_Size - Changes the capacity of the pipe/FIFO.
 ****************************************************************************/

#if defined(HAVE_GETPIPE_SZ) && defined(HAVE_SETPIPE_SZ)

void Set_Pipe_Size ( FILE *fd )
{

    int fd_int;
    int current_fifo_size;
    int fd_results;


    if ( config->sagan_fifo_size != 0 ) {

        fd_int = fileno(fd);
        current_fifo_size = fcntl(fd_int, F_GETPIPE_SZ);

        if ( current_fifo_size == config->sagan_fifo_size ) {

            Sagan_Log(S_NORMAL, "FIFO capacity already set to %d bytes.", config->sagan_fifo_size);

        } else {

            Sagan_Log(S_NORMAL, "FIFO capacity is %d bytes.  Changing to %d bytes.", current_fifo_size, config->sagan_fifo_size);

            fd_results = fcntl(fd_int, F_SETPIPE_SZ, config->sagan_fifo_size );

            if ( fd_results == -1 ) {
                Sagan_Log(S_WARN, "FIFO capacity could not be changed.  Continuing anyways...");
            }

            if ( fd_results > config->sagan_fifo_size ) {
                Sagan_Log(S_WARN, "FIFO capacity was rounded up to the next page size of %d bytes.", fd_results);
            }
        }
    }
}

#endif

/****************************************************************************
 * File_Lock - Takes in a file descriptor and "locks" the file.  Used
 * with IPC/memory mapped files.
 ****************************************************************************/

sbool File_Lock ( int fd )
{

    struct flock fl;

    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    fl.l_pid = getpid();

    if (fcntl(fd, F_SETLKW, &fl) == -1) {
        Sagan_Log(S_WARN, "[%s, line %d] Unable to get LOCK on file. (%s)", __FILE__, __LINE__, strerror(errno));
    }

    return(0);
}

/****************************************************************************
 * File_Unlock - Takes in a file descriptor and "unlocks" the file.
 * Used with IPC/memory mapped files.
 ****************************************************************************/

sbool File_Unlock( int fd )
{

    struct flock fl;

    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    fl.l_pid = getpid();

    if (fcntl(fd, F_SETLK, &fl) == -1) {
        Sagan_Log(S_WARN, "[%s, line %d] Unable to get UNLOCK on file. (%s)", __FILE__, __LINE__, strerror(errno));
    }

    return(0);
}

/****************************************************************************
 * Bit2IP - Takes a 16 byte char IP address and returns a string
 ****************************************************************************/

const char *Bit2IP(unsigned char *ipbits, char *str, size_t size)
{

    int i;
    int ss_family = AF_INET;
    static __thread char retbuf[MAXIP];
    memset(retbuf,0,sizeof(retbuf));

    const char *ret = NULL;

    for (i=4;i<16;i++) {
        if (ipbits[i] != 0x00) {
            ss_family = AF_INET6;
            break;
        }
    }
    ret = inet_ntop(ss_family, ipbits, NULL == str ? retbuf : str, NULL == str ? sizeof(retbuf) : size);

    return ret;
}

/************************************************************************/
/* Convert an IP or IP/CIDR into 128bit IP and 128bit mask.             */
/* Return if masked.  Assume that out is at least 32 bytes              */
/************************************************************************/
sbool Netaddr_To_Range( char *ipstr, unsigned char *out )
{

    int mask;
    char *t = NULL;
    char _t = '\0';
    char *ip = ipstr;
    int maxmask = NULL != strchr(ipstr, ':') ? 128 : 32;


    if ( t = strchr(ipstr, '/') ) {
        mask = atoi(t+1);
    } else {
        mask = maxmask;
    }
    if (NULL != t) {
        _t = t[0];
        t[0] = '\0'; 
    }
    IP2Bit(ipstr, out);
    if (NULL != t) {
        t[0] = _t; 
    }
    Mask2Bit(mask, out+16);

    return mask != maxmask;
} /* netaddr_to_range() */


sbool Starts_With(const char *str, const char *prefix)
{
    size_t lenpre = strlen(prefix),
           lenstr = strlen(str);
    return lenstr < lenpre ? false : strncmp(prefix, str, lenpre) == 0;
}

/**********************************/
/* Strip characters from a string */
/**********************************/

void Strip_Chars(const char *string, const char *chars, char *str, size_t size)
{
    int counter = 0;

    for ( ; *string,counter<size; string++) {
        if (!strchr(chars, *string)) {
            str[ counter ] = *string;
            ++ counter;
        }
    }
    if (counter >= size) {
        counter = size - 1;
    }

    str[counter] = 0;
}

/***************************************************
 * Check if str is valid IP from decimal or dotted
 * quad ( 167772160, 1.1.1.1, 192.168.192.168/28 )
 ***************************************************/

sbool Is_IP (char *str)
{

    char *tmp = NULL;
    char *ip = NULL;
    int prefix;
    unsigned int ipint = 0;
    unsigned char ipbits[MAXIP] = {0};

    if(strlen(str) == strspn(str, "0123456789./:")) {

        if(strspn(str, "./") == 0) {
            ipint = atol(str); 
            memcpy(ipbits, &ipint, sizeof(ipint));
            if ( Bit2IP(ipbits, NULL, 0) == 0 ) {
                return(false);
            }
        }

        if ( strchr(str, '/') ) {
            ip = strtok_r(str, "/", &tmp);
            prefix = atoi(strtok_r(NULL, "/", &tmp));
            if(prefix < 1 || prefix > 128 || !IP2Bit(ip, NULL)) {
                return(false);
            }
        } else {
            if ( !IP2Bit(str, NULL) ) {
                return(false);
            }
        }

        return(true);

    } else {

        return(false);
    }

}

/***************************************************************************
 * PageSupportsRWX - Checks the OS to see if it allows RMX pages.  This
 * function is from Suricata and is by Shawn Webb from HardenedBSD. GRSec
 * will cause things like PCRE JIT to fail.
 ***************************************************************************/

#ifndef HAVE_SYS_MMAN_H
#ifndef PageSupportsRWX
#define PageSupportsRWX 1
#endif
#else
#include <sys/mman.h>

int PageSupportsRWX(void)
{
    int retval = 1;
    void *ptr;
    ptr = mmap(0, getpagesize(), PROT_READ|PROT_WRITE, MAP_ANON|MAP_SHARED, -1, 0);
    if (ptr != MAP_FAILED) {
        if (mprotect(ptr, getpagesize(), PROT_READ|PROT_WRITE|PROT_EXEC) == -1) {
            retval = 0;
        }
        munmap(ptr, getpagesize());
    }
    return retval;
}
#endif /* HAVE_SYS_MMAN_H */

/***************************************************************************
 * FlowGetId - Generates a Suricata "FLow ID".  We don't really support
 * "FLow ID" idea like Suricata.  This is for compatibility with Suricata
 * EVE
 ***************************************************************************/

int64_t FlowGetId( _Sagan_Event *Event)
{
    return (int64_t)(Event->event_time.tv_sec & 0x0000FFFF) << 16 |
           (int64_t)(Event->event_time.tv_usec & 0x0000FFFF);
}

/***************************************************************************
 * Check_Content_Not - Simply returns true/false if a "not" (!) is present
 * in a string.  For example, content!"something";
 ***************************************************************************/

sbool Check_Content_Not( char *s )
{

    char rule_tmp[RULEBUF] = { 0 };
    int i;

    strlcpy(rule_tmp, s, sizeof(rule_tmp));

    for (i=0; i<strlen(rule_tmp); i++) {

        /* We found the first ",  no need to go any further */

        if ( rule_tmp[i] == '"' ) {

            return(false);

        }

        /* Got ! .  This is a content:! or meta_content:! rule! */

        else if ( rule_tmp[i] == '!' ) {

            return(true);

        }
    }

    return(false);
}


/***************************************************************************
 * Djd2_Hash - creates a hash based off a string.  This code is from Dan
 * Bernstein.  See http://www.cse.yorku.ca/~oz/hash.html.
 ***************************************************************************/

uint32_t Djb2_Hash(char *str)
{

    uint32_t hash = 5381;
    int32_t c;

    while ( (c = *str++ ) )
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return(hash);
}

char *strrpbrk(const char *str, const char *accept)
{
    const char *test = NULL;
    const char *pstr = str+strlen(str);
    while (pstr >= str) {
        test = accept;
        while (test[0] != '\0') {
            if ((test++)[0] == pstr[0]) {
                return (char *)pstr;
            }
        }
        pstr--;
    }
    return NULL;
}

