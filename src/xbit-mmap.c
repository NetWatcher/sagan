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

/*
 * xbit-mmap.c - Functions used for tracking events over multiple log
 * lines.
 *
 */


#ifdef HAVE_CONFIG_H
#include "config.h"             /* From autoconf */
#endif

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/mman.h>

#include "sagan.h"
#include "sagan-defs.h"
#include "ipc.h"
#include "xbit-mmap.h"
#include "rules.h"
#include "sagan-config.h"
#include "parsers/parsers.h"

struct _SaganCounters *counters;
struct _Rule_Struct *rulestruct;
struct _SaganDebug *debug;
struct _SaganConfig *config;

pthread_mutex_t Xbit_Mutex=PTHREAD_MUTEX_INITIALIZER;

struct _Sagan_IPC_Counters *counters_ipc;
struct _Sagan_IPC_Xbit *xbit_ipc;

/*****************************************************************************
 * Xbit_Condition - Used for testing "isset" & "isnotset".  Full
 * rule condition is tested here and returned.
 *****************************************************************************/

sbool Xbit_Condition_MMAP(int rule_position, char *ip_src_char, char *ip_dst_char, int src_port, int dst_port, char *selector )
{

    time_t t;
    struct tm *now;
    char  timet[20];
    char  tmp[128] = { 0 };
    char *tmp_xbit_name = NULL;
    char *tok = NULL;

    int i;
    int a;

    unsigned char ip_src[MAXIPBIT] = {0};
    unsigned char ip_dst[MAXIPBIT] = {0};

    sbool xbit_match = false;
    int xbit_total_match = 0;

    t = time(NULL);
    now=localtime(&t);
    strftime(timet, sizeof(timet), "%s",  now);

    sbool has_ip_src = IP2Bit(ip_src_char, ip_src);
    sbool has_ip_dst = IP2Bit(ip_dst_char, ip_dst);

    sbool and_or = false;

    Xbit_Cleanup_MMAP();

    for (i = 0; i < rulestruct[rule_position].xbit_count; i++) {

        /*******************
         *      ISSET      *
         *******************/

        if ( rulestruct[rule_position].xbit_type[i] == 3 ) {

            for (a = 0; a < counters_ipc->xbit_count; a++) {

                strlcpy(tmp, rulestruct[rule_position].xbit_name[i], sizeof(tmp));

                if (Sagan_strstr(rulestruct[rule_position].xbit_name[i], "|")) {
                    tmp_xbit_name = strtok_r(tmp, "|", &tok);
                    and_or = true;
                } else {
                    tmp_xbit_name = strtok_r(tmp, "&", &tok);
                    and_or = false;
                }

                // Short circuit if no selector match
                if (
                        (NULL == selector && xbit_ipc[a].selector[0] != '\0') || 
                        (NULL != selector && 0 != strcmp(selector, xbit_ipc[a].selector))
                   ) {

                        continue;
                }

                while (tmp_xbit_name != NULL ) {

                    if (!strcmp(tmp_xbit_name, xbit_ipc[a].xbit_name) &&
                        xbit_ipc[a].xbit_state == true ) {

                        /* direction: by_src - most common check */

                        if ( rulestruct[rule_position].xbit_direction[i] == 2 && has_ip_src &&
                             0 == memcmp(xbit_ipc[a].ip_dst, ip_src, sizeof(ip_src)) )

                        {

                            if ( debug->debugxbit ) {
                                Sagan_Log(S_DEBUG, "[%s, line %d] \"isset\" xbit \"%s\" (direction: \"by_src\"). (%s -> any)", __FILE__, __LINE__, xbit_ipc[a].xbit_name, ip_src_char);
                            }

                            xbit_total_match++;

                            /* Only 1 xbit, not need to continue loop */

                            if ( counters_ipc->xbit_count == 1 ) {
                                break;
                            }

                        }

                        /* direction: none */

                        else if ( rulestruct[rule_position].xbit_direction[i] == 0 )

                        {

                            if ( debug->debugxbit ) {
                                Sagan_Log(S_DEBUG, "[%s, line %d] \"isset\" xbit \"%s\" (direction: \"none\"). (any -> any)", __FILE__, __LINE__, xbit_ipc[a].xbit_name);
                            }

                            xbit_total_match++;

                            /* Only 1 xbit, not need to continue loop */

                            if ( counters_ipc->xbit_count == 1 ) {
                                break;
                            }

                        }

                        /* direction: both */

                        else if ( rulestruct[rule_position].xbit_direction[i] == 1 &&
                                  has_ip_src && has_ip_dst && 
                                  0 == memcmp(xbit_ipc[a].ip_src, ip_src, sizeof(ip_src)) &&
                                  0 == memcmp(xbit_ipc[a].ip_dst, ip_dst, sizeof(ip_src)) )

                        {

                            if ( debug->debugxbit ) {
                                Sagan_Log(S_DEBUG, "[%s, line %d] \"isset\" xbit \"%s\" (direction: \"both\"). (%s -> %s)", __FILE__, __LINE__, xbit_ipc[a].xbit_name, ip_src_char, ip_dst_char);
                            }

                            xbit_total_match++;

                            /* Only 1 xbit, not need to continue loop */

                            if ( counters_ipc->xbit_count == 1 ) {
                                break;
                            }

                        }

                        /* direction: by_dst */

                        else if ( rulestruct[rule_position].xbit_direction[i] == 3 &&
                                  has_ip_dst && 
                                  0 == memcmp(xbit_ipc[a].ip_dst, ip_dst, sizeof(ip_dst)) )

                        {

                            if ( debug->debugxbit) {
                                Sagan_Log(S_DEBUG, "[%s, line %d] \"isset\" xbit \"%s\" (direction: \"by_dst\"). (any -> %s)", __FILE__, __LINE__, xbit_ipc[a].xbit_name, ip_dst_char);
                            }

                            xbit_total_match++;

                            /* Only 1 xbit, not need to continue loop */

                            if ( counters_ipc->xbit_count == 1 ) {
                                break;
                            }
                        }

                        /* direction: reverse */

                        else if ( rulestruct[rule_position].xbit_direction[i] == 4 &&
                                  has_ip_src && has_ip_dst && 
                                  0 == memcmp(xbit_ipc[a].ip_src, ip_dst, sizeof(ip_dst)) &&
                                  0 == memcmp(xbit_ipc[a].ip_dst, ip_src, sizeof(ip_src)) )

                        {
                            if ( debug->debugxbit) {
                                Sagan_Log(S_DEBUG, "[%s, line %d] \"isset\" xbit \"%s\" (direction: \"reverse\"). (%s -> %s)", __FILE__, __LINE__, xbit_ipc[a].xbit_name, ip_dst_char, ip_src_char);
                            }

                            xbit_total_match++;

                            /* Only 1 xbit, not need to continue loop */

                            if ( counters_ipc->xbit_count == 1 ) {
                                break;
                            }
                        }

                        /* direction: src_xbitdst */

                        else if ( rulestruct[rule_position].xbit_direction[i] == 5 &&
                                  has_ip_src && 
                                  0 == memcmp(xbit_ipc[a].ip_dst, ip_src, sizeof(ip_src)) )

                        {

                            if ( debug->debugxbit) {
                                Sagan_Log(S_DEBUG, "[%s, line %d] \"isset\" xbit \"%s\" (direction: \"src_xbitdst\"). (%s -> any)", __FILE__, __LINE__, xbit_ipc[a].xbit_name, ip_src_char);
                            }

                            xbit_total_match++;

                            /* Only 1 xbit, not need to continue loop */

                            if ( counters_ipc->xbit_count == 1 ) {
                                break;
                            }
                        }

                        /* direction: dst_xbitsrc */

                        else if ( rulestruct[rule_position].xbit_direction[i] == 6 &&
                                  has_ip_dst && 
                                  0 == memcmp(xbit_ipc[a].ip_src, ip_dst, sizeof(ip_dst)) )

                        {

                            if ( debug->debugxbit) {
                                Sagan_Log(S_DEBUG, "[%s, line %d] \"isset\" xbit \"%s\" (direction: \"dst_xbitsrc\"). (any -> %s)", __FILE__, __LINE__, xbit_ipc[a].xbit_name, ip_dst_char);
                            }

                            xbit_total_match++;

                            /* Only 1 xbit, not need to continue loop */

                            if ( counters_ipc->xbit_count == 1 ) {
                                break;
                            }
                        }

                        /* direction: both_p */

                        else if ( rulestruct[rule_position].xbit_direction[i] == 7 &&
                                  has_ip_src && has_ip_dst && 
                                  0 == memcmp(xbit_ipc[a].ip_src, ip_src, sizeof(ip_src)) &&
                                  0 == memcmp(xbit_ipc[a].ip_dst, ip_dst, sizeof(ip_dst)) &&
                                  xbit_ipc[a].src_port == src_port &&
                                  xbit_ipc[a].dst_port == dst_port )

                        {

                            if ( debug->debugxbit) {
                                Sagan_Log(S_DEBUG, "[%s, line %d] \"isset\" xbit \"%s\" (direction: \"both_p\"). (%s -> %s)", __FILE__, __LINE__, xbit_ipc[a].xbit_name, ip_src_char, ip_dst_char);
                            }

                            xbit_total_match++;

                            /* Only 1 xbit, not need to continue loop */

                            if ( counters_ipc->xbit_count == 1 ) {
                                break;
                            }
                        }

                        /* direction: by_src_p */

                        else if ( rulestruct[rule_position].xbit_direction[i] == 8 &&
                                  has_ip_src && 
                                  0 == memcmp(xbit_ipc[a].ip_src, ip_src, sizeof(ip_src)) &&
                                  xbit_ipc[a].ip_src == ip_src &&
                                  xbit_ipc[a].src_port == src_port )

                        {

                            if ( debug->debugxbit) {
                                Sagan_Log(S_DEBUG, "[%s, line %d] \"isset\" xbit \"%s\" (direction: \"by_src_p\"). (%s -> any)", __FILE__, __LINE__, xbit_ipc[a].xbit_name, ip_src_char);
                            }

                            xbit_total_match++;

                            /* Only 1 xbit, not need to continue loop */

                            if ( counters_ipc->xbit_count == 1 ) {
                                break;
                            }
                        }

                        /* direction: by_dst_p */

                        else if ( rulestruct[rule_position].xbit_direction[i] == 9 &&
                                  has_ip_dst && 
                                  0 == memcmp(xbit_ipc[a].ip_dst, ip_dst, sizeof(ip_dst)) &&
                                  xbit_ipc[a].ip_dst == ip_dst &&
                                  xbit_ipc[a].dst_port == dst_port )

                        {

                            if ( debug->debugxbit) {
                                Sagan_Log(S_DEBUG, "[%s, line %d] \"isset\" xbit \"%s\" (direction: \"by_dst_p\"). (any -> %s)", __FILE__, __LINE__, xbit_ipc[a].xbit_name, ip_dst_char);
                            }

                            xbit_total_match++;

                            /* Only 1 xbit, not need to continue loop */

                            if ( counters_ipc->xbit_count == 1 ) {
                                break;
                            }
                        }

                        /* direction: reverse_p */

                        else if ( rulestruct[rule_position].xbit_direction[i] == 10 &&
                                  has_ip_src && has_ip_dst && 
                                  0 == memcmp(xbit_ipc[a].ip_src, ip_dst, sizeof(ip_dst)) &&
                                  0 == memcmp(xbit_ipc[a].ip_dst, ip_src, sizeof(ip_src)) &&
                                  xbit_ipc[a].src_port == dst_port &&
                                  xbit_ipc[a].dst_port == src_port )


                        {
                            if ( debug->debugxbit) {
                                Sagan_Log(S_DEBUG, "[%s, line %d] \"isset\" xbit \"%s\" (direction: \"reverse_p\"). (%s -> %s)", __FILE__, __LINE__, xbit_ipc[a].xbit_name, ip_dst_char, ip_src_char);
                            }

                            xbit_total_match++;

                            /* Only 1 xbit, not need to continue loop */

                            if ( counters_ipc->xbit_count == 1 ) {
                                break;
                            }
                        }

                        /* direction: src_xbitdst_p */

                        else if ( rulestruct[rule_position].xbit_direction[i] == 11 &&
                                  has_ip_src && 
                                  0 == memcmp(xbit_ipc[a].ip_dst, ip_src, sizeof(ip_src)) &&
                                  xbit_ipc[a].dst_port == src_port )

                        {

                            if ( debug->debugxbit) {
                                Sagan_Log(S_DEBUG, "[%s, line %d] \"isset\" xbit \"%s\" (direction: \"src_xbitdst_p\"). (%s -> any)", __FILE__, __LINE__, xbit_ipc[a].xbit_name, ip_src_char);
                            }

                            xbit_total_match++;

                            /* Only 1 xbit, not need to continue loop */

                            if ( counters_ipc->xbit_count == 1 ) {
                                break;
                            }
                        }

                        /* direction: dst_xbitsrc_p */

                        else if ( rulestruct[rule_position].xbit_direction[i] == 12 &&
                                  has_ip_dst && 
                                  0 == memcmp(xbit_ipc[a].ip_src, ip_dst, sizeof(ip_dst)) &&
                                  xbit_ipc[a].ip_src == ip_dst &&
                                  xbit_ipc[a].src_port == dst_port )

                        {

                            if ( debug->debugxbit) {
                                Sagan_Log(S_DEBUG, "[%s, line %d] \"isset\" xbit \"%s\" (direction: \"dst_xbitsrc_p\"). (any -> %s)", __FILE__, __LINE__, xbit_ipc[a].xbit_name, ip_dst_char);
                            }

                            xbit_total_match++;

                            /* Only 1 xbit, not need to continue loop */

                            if ( counters_ipc->xbit_count == 1 ) {
                                break;
                            }
                        }

                    } /* End of strcmp xbit_name & xbit_state = 1 */


                    if ( and_or == true ) {
                        tmp_xbit_name = strtok_r(NULL, "|", &tok);
                    } else {
                        tmp_xbit_name = strtok_r(NULL, "&", &tok);
                    }

                } /* End of "while tmp_xbit_name" */

            } /* End of "for a" */

        } /* End "if" xbit_type == 3 (ISSET) */

        /*******************
        *    ISNOTSET     *
        *******************/

        else if ( rulestruct[rule_position].xbit_type[i] == 4 ) {

            xbit_match = false;

            for (a = 0; a < counters_ipc->xbit_count; a++) {

                strlcpy(tmp, rulestruct[rule_position].xbit_name[i], sizeof(tmp));

                if (Sagan_strstr(rulestruct[rule_position].xbit_name[i], "|")) {

                    tmp_xbit_name = strtok_r(tmp, "|", &tok);
                    and_or = true;

                } else {

                    tmp_xbit_name = strtok_r(tmp, "&", &tok);
                    and_or = false;

                }

                // Short circuit if no selector match
                if (
                        (NULL == selector && xbit_ipc[a].selector[0] != '\0') || 
                        (NULL != selector && 0 != strcmp(selector, xbit_ipc[a].selector))
                   ) {

                        continue;
                }

                while (tmp_xbit_name != NULL ) {

                    if (!strcmp(tmp_xbit_name, xbit_ipc[a].xbit_name)) {

                        xbit_match = true;

                        if ( xbit_ipc[a].xbit_state == false ) {

                            /* direction: by_src  - most common check */

                            if ( rulestruct[rule_position].xbit_direction[i] == 2 ) {

                                if ( xbit_ipc[a].ip_src == ip_src ) {

                                    if ( debug->debugxbit) {
                                        Sagan_Log(S_DEBUG, "[%s, line %d] \"isnotset\" xbit \"%s\" (direction: \"by_src\"). (%s -> any)", __FILE__, __LINE__, xbit_ipc[a].xbit_name, ip_src_char);
                                    }

                                    xbit_total_match++;

                                    /* Only 1 xbit, not need to continue loop */

                                    if ( counters_ipc->xbit_count == 1 ) {
                                        break;
                                    }
                                }
                            }

                            /* direction: none */

                            else if ( rulestruct[rule_position].xbit_direction[i] == 0 ) {

                                if ( debug->debugxbit) {
                                    Sagan_Log(S_DEBUG, "[%s, line %d] \"isnotset\" xbit \"%s\" (direction: \"none\"). (any -> any)", __FILE__, __LINE__, xbit_ipc[a].xbit_name);
                                }

                                xbit_total_match++;

                                /* Only 1 xbit, not need to continue loop */

                                if ( counters_ipc->xbit_count == 1 ) {
                                    break;
                                }

                            }

                            /* direction: both */

                            else if ( rulestruct[rule_position].xbit_direction[i] == 1 ) {


                                if ( has_ip_src && has_ip_dst &&
                                     0 == memcmp(xbit_ipc[a].ip_src, ip_src, sizeof(ip_src)) &&
                                     0 == memcmp(xbit_ipc[a].ip_dst, ip_dst, sizeof(ip_dst)) ) {

                                    if ( debug->debugxbit) {
                                        Sagan_Log(S_DEBUG, "[%s, line %d] \"isnotset\" xbit \"%s\" (direction: \"both\"). (%s -> %s)", __FILE__, __LINE__, xbit_ipc[a].xbit_name, ip_src_char, ip_dst_char);
                                    }

                                    xbit_total_match++;

                                    /* Only 1 xbit, not need to continue loop */

                                    if ( counters_ipc->xbit_count == 1 ) {
                                        break;
                                    }

                                }
                            }

                            /* direction: by_dst */

                            else if ( rulestruct[rule_position].xbit_direction[i] == 3 ) {

                                if ( has_ip_dst && 0 == memcmp(xbit_ipc[a].ip_dst, ip_dst, sizeof(ip_dst)) ) {

                                    if ( debug->debugxbit) {
                                        Sagan_Log(S_DEBUG, "[%s, line %d] \"isnotset\" xbit \"%s\" (direction: \"by_dst\"). (any -> %s)", __FILE__, __LINE__, xbit_ipc[a].xbit_name, ip_dst_char);
                                    }

                                    xbit_total_match++;

                                    /* Only 1 xbit, not need to continue loop */

                                    if ( counters_ipc->xbit_count == 1 ) {
                                        break;
                                    }
                                }
                            }

                            /* direction: reverse */

                            else if ( rulestruct[rule_position].xbit_direction[i] == 4 ) {

                                if ( has_ip_src && has_ip_dst && 
                                     0 == memcmp(xbit_ipc[a].ip_src, ip_dst, sizeof(ip_dst)) &&
                                     0 == memcmp(xbit_ipc[a].ip_dst, ip_src, sizeof(ip_src)) ) {

                                    if ( debug->debugxbit) {
                                        Sagan_Log(S_DEBUG, "[%s, line %d] \"isnotset\" xbit \"%s\" (direction: \"reverse\"). (%s -> %s)", __FILE__, __LINE__, xbit_ipc[a].xbit_name, ip_dst_char, ip_src_char);
                                    }

                                    xbit_total_match++;

                                    /* Only 1 xbit, not need to continue loop */

                                    if ( counters_ipc->xbit_count == 1 ) {
                                        break;
                                    }

                                }
                            }

                            /* direction: src_xbitdst */

                            else if ( rulestruct[rule_position].xbit_direction[i] == 5 ) {

                                if ( xbit_ipc[a].ip_dst == ip_src ) {

                                    if ( debug->debugxbit) {
                                        Sagan_Log(S_DEBUG, "[%s, line %d] \"isnotset\" xbit \"%s\" (direction: \"src_xbitdst\"). (%s -> any)", __FILE__, __LINE__, xbit_ipc[a].xbit_name, ip_src_char);
                                    }

                                    xbit_total_match++;

                                    /* Only 1 xbit, not need to continue loop */

                                    if ( counters_ipc->xbit_count == 1 ) {
                                        break;
                                    }

                                }
                            }

                            /* direction: dst_xbitsrc */

                            else if ( rulestruct[rule_position].xbit_direction[i] == 6 ) {

                                if ( xbit_ipc[a].ip_src == ip_dst ) {

                                    if ( debug->debugxbit) {
                                        Sagan_Log(S_DEBUG, "[%s, line %d] \"isnotset\" xbit \"%s\" (direction: \"dst_xbitsrc\"). (any -> %s)", __FILE__, __LINE__, xbit_ipc[a].xbit_name, ip_dst_char);
                                    }

                                    xbit_total_match++;

                                    /* Only 1 xbit, not need to continue loop */

                                    if ( counters_ipc->xbit_count == 1 ) {
                                        break;
                                    }

                                }
                            }

                            /* direction: both_p */

                            else if ( rulestruct[rule_position].xbit_direction[i] == 7 ) {


                                if ( has_ip_src && has_ip_dst &&
                                     0 == memcmp(xbit_ipc[a].ip_src, ip_src, sizeof(ip_src)) &&
                                     0 == memcmp(xbit_ipc[a].ip_dst, ip_dst, sizeof(ip_dst)) &&
                                     xbit_ipc[a].src_port == src_port &&
                                     xbit_ipc[a].dst_port == dst_port ) {

                                    if ( debug->debugxbit) {
                                        Sagan_Log(S_DEBUG, "[%s, line %d] \"isnotset\" xbit \"%s\" (direction: \"both_y\"). (%s -> %s)", __FILE__, __LINE__, xbit_ipc[a].xbit_name, ip_src_char, ip_dst_char);
                                    }

                                    xbit_total_match++;

                                    /* Only 1 xbit, not need to continue loop */

                                    if ( counters_ipc->xbit_count == 1 ) {
                                        break;
                                    }

                                }
                            }

                            /* direction: by_src_p */

                            else if ( rulestruct[rule_position].xbit_direction[i] == 8 ) {

                                if ( has_ip_src &&
                                     0 == memcmp(xbit_ipc[a].ip_src, ip_src, sizeof(ip_src)) &&
                                     xbit_ipc[a].src_port == src_port ) {

                                    if ( debug->debugxbit) {
                                        Sagan_Log(S_DEBUG, "[%s, line %d] \"isnotset\" xbit \"%s\" (direction: \"by_src_p\"). (%s -> any)", __FILE__, __LINE__, xbit_ipc[a].xbit_name, ip_src_char);
                                    }

                                    xbit_total_match++;

                                    /* Only 1 xbit, not need to continue loop */

                                    if ( counters_ipc->xbit_count == 1 ) {
                                        break;
                                    }
                                }
                            }

                            /* direction: by_dst_p */

                            else if ( rulestruct[rule_position].xbit_direction[i] == 9 ) {

                                if ( has_ip_dst &&
                                     0 == memcmp(xbit_ipc[a].ip_dst, ip_dst, sizeof(ip_dst)) &&
                                     xbit_ipc[a].dst_port == dst_port ) {

                                    if ( debug->debugxbit) {
                                        Sagan_Log(S_DEBUG, "[%s, line %d] \"isnotset\" xbit \"%s\" (direction: \"by_dst_p\"). (any -> %s)", __FILE__, __LINE__, xbit_ipc[a].xbit_name, ip_dst_char);
                                    }

                                    xbit_total_match++;

                                    /* Only 1 xbit, not need to continue loop */

                                    if ( counters_ipc->xbit_count == 1 ) {
                                        break;
                                    }
                                }
                            }

                            /* direction: reverse_p */

                            else if ( rulestruct[rule_position].xbit_direction[i] == 10 ) {

                                if ( has_ip_src && has_ip_dst &&
                                     0 == memcmp(xbit_ipc[a].ip_src, ip_dst, sizeof(ip_dst)) &&
                                     0 == memcmp(xbit_ipc[a].ip_dst, ip_src, sizeof(ip_src)) &&
                                     xbit_ipc[a].src_port == dst_port &&
                                     xbit_ipc[a].dst_port == src_port) {

                                    if ( debug->debugxbit) {
                                        Sagan_Log(S_DEBUG, "[%s, line %d] \"isnotset\" xbit \"%s\" (direction: \"reverse_p\"). (%s -> %s)", __FILE__, __LINE__, xbit_ipc[a].xbit_name, ip_dst_char, ip_src_char);
                                    }

                                    xbit_total_match++;

                                    /* Only 1 xbit, not need to continue loop */

                                    if ( counters_ipc->xbit_count == 1 ) {
                                        break;
                                    }
                                }
                            }

                            /* direction: src_xbitdst_p */

                            else if ( rulestruct[rule_position].xbit_direction[i] == 11 ) {

                                if ( has_ip_src &&
                                     0 == memcmp(xbit_ipc[a].ip_dst, ip_src, sizeof(ip_src)) &&
                                     xbit_ipc[a].dst_port == src_port ) {

                                    if ( debug->debugxbit) {
                                        Sagan_Log(S_DEBUG, "[%s, line %d] \"isnotset\" xbit \"%s\" (direction: \"src_xbitdst_p\"). (%s -> any)", __FILE__, __LINE__, xbit_ipc[a].xbit_name, ip_src_char);
                                    }

                                    xbit_total_match++;

                                    /* Only 1 xbit, not need to continue loop */

                                    if ( counters_ipc->xbit_count == 1 ) {
                                        break;
                                    }
                                }
                            }

                            /* direction: dst_xbitsrc_p */

                            else if ( rulestruct[rule_position].xbit_direction[i] == 12 ) {

                                if ( has_ip_dst && 
                                     0 == memcmp(xbit_ipc[a].ip_src, ip_dst, sizeof(ip_dst)) &&
                                     xbit_ipc[a].src_port == dst_port ) {

                                    if ( debug->debugxbit) {
                                        Sagan_Log(S_DEBUG, "[%s, line %d] \"isnotset\" xbit \"%s\" (direction: \"dst_xbitsrc_p\"). (any -> %s)", __FILE__, __LINE__, xbit_ipc[a].xbit_name, ip_dst_char);
                                    }

                                    xbit_total_match++;

                                    /* Only 1 xbit, not need to continue loop */

                                    if ( counters_ipc->xbit_count == 1 ) {
                                        break;
                                    }
                                }
                            }

                        } /* End xbit_state == 0 */

                    } /* End of strcmp xbit_name */

                    if ( and_or == true) {
                        tmp_xbit_name = strtok_r(NULL, "|", &tok);
                    } else {
                        tmp_xbit_name = strtok_r(NULL, "&", &tok);
                    }

                } /* End of "while tmp_xbit_name" */
            } /* End of "for a" */

            if ( and_or == true && xbit_match == true ) {
                xbit_total_match = rulestruct[rule_position].xbit_condition_count;	/* Do we even need this for OR? */
            }

            if ( and_or == false && xbit_match == false ) {
                xbit_total_match = rulestruct[rule_position].xbit_condition_count;
            }

        } /* End of "xbit_type[i] == 4" */

        else if ( rulestruct[rule_position].xbit_type[i] == 4 ) {

        }


    } /* End of "for i" */

    /* IF we match all criteria for isset/isnotset
     *
     * If we match the xbit_conditon_count (number of concurrent xbits)
     * we trigger.  It it's an "or" statement,  we trigger if any of the
     * xbits are set.
     *
     */

    if ( ( rulestruct[rule_position].xbit_condition_count == xbit_total_match ) || ( and_or == true && xbit_total_match != 0 ) ) {

        if ( debug->debugxbit) {
            Sagan_Log(S_DEBUG, "[%s, line %d] Condition of xbit returning TRUE. %d %d", __FILE__, __LINE__, rulestruct[rule_position].xbit_condition_count, xbit_total_match);
        }

        return(true);
    }

    /* isset/isnotset failed. */

    if ( debug->debugxbit) {
        Sagan_Log(S_DEBUG, "[%s, line %d] Condition of xbit returning FALSE. Needed %d but got %d matches.", __FILE__, __LINE__, rulestruct[rule_position].xbit_condition_count, xbit_total_match);
    }

    return(false);

}  /* End of Xbit_Condition(); */


/*****************************************************************************
 * Xbit_Count - Used to determine how many xbits has been set based on a
 * source or destination address.  This is useful for identification of
 * distributed attacks.
 *****************************************************************************/

sbool Xbit_Count_MMAP( int rule_position, char *ip_src_char, char *ip_dst_char, char *selector )
{

    uint32_t a = 0;
    uint32_t i = 0;
    uint32_t counter = 0;

    unsigned char ip_src[MAXIPBIT] = {0};
    unsigned char ip_dst[MAXIPBIT] = {0};

    sbool has_ip_src = IP2Bit(ip_src_char, ip_src);
    sbool has_ip_dst = IP2Bit(ip_dst_char, ip_dst);

    for (i = 0; i < rulestruct[rule_position].xbit_count_count; i++) {

        for (a = 0; a < counters_ipc->xbit_count; a++) {
            // Short circuit if no selector match
            if (
                    (NULL == selector && xbit_ipc[a].selector[0] != '\0') || 
                    (NULL != selector && 0 != strcmp(selector, xbit_ipc[a].selector))
               ) {

                    continue;
            }

            if ( rulestruct[rule_position].xbit_direction[i] == 2 &&
                 has_ip_src &&
                 0 == memcmp(xbit_ipc[a].ip_src, ip_src, sizeof(ip_src)) ) {

                counter++;

                if ( rulestruct[rule_position].xbit_count_gt_lt[i] == 0 ) {

                    if ( counter > rulestruct[rule_position].xbit_count_counter[i] ) {

                        if ( debug->debugxbit) {
                            Sagan_Log(S_DEBUG, "[%s, line %d] Xbit count 'by_src' threshold reached for xbit '%s'.", __FILE__, __LINE__, xbit_ipc[a].xbit_name);
                        }


                        return(true);
                    }
                }
            }

            else if ( rulestruct[rule_position].xbit_direction[i] == 3 &&
                      has_ip_src &&
                      0 == memcmp(xbit_ipc[a].ip_dst, ip_dst, sizeof(ip_dst)) ) {

                counter++;

                if ( rulestruct[rule_position].xbit_count_gt_lt[i] == 0 ) {

                    if ( counter > rulestruct[rule_position].xbit_count_counter[i] ) {

                        if ( debug->debugxbit) {
                            Sagan_Log(S_DEBUG, "[%s, line %d] Xbit count 'by_dst' threshold reached for xbit '%s'.", __FILE__, __LINE__, xbit_ipc[a].xbit_name);
                        }

                        return(true);
                    }
                }
            }
        }
    }

    if ( debug->debugxbit) {
        Sagan_Log(S_DEBUG, "[%s, line %d] Xbit count threshold NOT reached for xbit." , __FILE__, __LINE__);
    }

    return(false);
}


/*****************************************************************************
 * Xbit_Set - Used to "set" & "unset" xbit.  All rule "set" and
 * "unset" happen here.
 *****************************************************************************/

void Xbit_Set_MMAP(int rule_position, char *ip_src_char, char *ip_dst_char, int src_port, int dst_port, char *selector )
{

    int i = 0;
    int a = 0;

    time_t t;
    struct tm *now;
    char  timet[20];

    char tmp[128] = { 0 };
    char *tmp_xbit_name = NULL;
    char *tok = NULL;

    sbool xbit_match = false;
    sbool xbit_unset_match = 0;

    unsigned char ip_src[MAXIPBIT] = {0};
    unsigned char ip_dst[MAXIPBIT] = {0};

    sbool has_ip_src = IP2Bit(ip_src_char, ip_src);
    sbool has_ip_dst = IP2Bit(ip_dst_char, ip_dst);

    t = time(NULL);
    now=localtime(&t);
    strftime(timet, sizeof(timet), "%s",  now);

    struct _Sagan_Xbit_Track *xbit_track;

    xbit_track = malloc(sizeof(_Sagan_Xbit_Track));

    if ( xbit_track  == NULL ) {
        Sagan_Log(S_ERROR, "[%s, line %d] Failed to allocate memory for xbit_track. Abort!", __FILE__, __LINE__);
    }

    memset(xbit_track, 0, sizeof(_Sagan_Xbit_Track));

    int xbit_track_count = 0;

    Xbit_Cleanup_MMAP();

    for (i = 0; i < rulestruct[rule_position].xbit_count; i++) {

        /*******************
         *      UNSET      *
         *******************/

        if ( rulestruct[rule_position].xbit_type[i] == 2 ) {

            /* Xbits & (ie - bit1&bit2) */

            strlcpy(tmp, rulestruct[rule_position].xbit_name[i], sizeof(tmp));
            tmp_xbit_name = strtok_r(tmp, "&", &tok);

            while( tmp_xbit_name != NULL ) {


                for (a = 0; a < counters_ipc->xbit_count; a++) {
                    // Short circuit if no selector match
                    if (
                            (NULL == selector && xbit_ipc[a].selector[0] != '\0') || 
                            (NULL != selector && 0 != strcmp(selector, xbit_ipc[a].selector))
                       ) {

                            continue;
                    }


                    if ( !strcmp(tmp_xbit_name, xbit_ipc[a].xbit_name )) {

                        /* direction: none */

                        if ( rulestruct[rule_position].xbit_direction[i] == 0 ) {

                            if ( debug->debugxbit) {
                                Sagan_Log(S_DEBUG, "[%s, line %d] \"unset\" xbit \"%s\" (direction: \"none\"). (any -> any)", __FILE__, __LINE__, xbit_ipc[a].xbit_name);
                            }


                            File_Lock(config->shm_xbit);
                            pthread_mutex_lock(&Xbit_Mutex);

                            xbit_ipc[a].xbit_state = false;

                            pthread_mutex_unlock(&Xbit_Mutex);
                            File_Unlock(config->shm_xbit);

                            xbit_unset_match = 1;

                        }


                        /* direction: both */

                        else if ( rulestruct[rule_position].xbit_direction[i] == 1 &&
                                  has_ip_src && has_ip_dst && 
                                  0 == memcmp(xbit_ipc[a].ip_src, ip_src, sizeof(ip_src)) &&
                                  0 == memcmp(xbit_ipc[a].ip_dst, ip_dst, sizeof(ip_dst)) ) {

                            if ( debug->debugxbit) {
                                Sagan_Log(S_DEBUG, "[%s, line %d] \"unset\" xbit \"%s\" (direction: \"both\"). (%s -> %s)", __FILE__, __LINE__, xbit_ipc[a].xbit_name, ip_src_char, ip_dst_char);
                            }

                            File_Lock(config->shm_xbit);
                            pthread_mutex_lock(&Xbit_Mutex);

                            xbit_ipc[a].xbit_state = false;

                            pthread_mutex_unlock(&Xbit_Mutex);
                            File_Unlock(config->shm_xbit);

                            xbit_unset_match = 1;

                        }

                        /* direction: by_src */

                        else if ( rulestruct[rule_position].xbit_direction[i] == 2 &&
                                  has_ip_src && 
                                  0 == memcmp(xbit_ipc[a].ip_src, ip_src, sizeof(ip_src)) )

                        {

                            if ( debug->debugxbit) {
                                Sagan_Log(S_DEBUG, "[%s, line %d] \"unset\" xbit \"%s\" (direction: \"by_src\"). (%s -> any)", __FILE__, __LINE__, xbit_ipc[a].xbit_name, ip_src_char);
                            }

                            File_Lock(config->shm_xbit);
                            pthread_mutex_lock(&Xbit_Mutex);

                            xbit_ipc[a].xbit_state = false;

                            pthread_mutex_unlock(&Xbit_Mutex);
                            File_Unlock(config->shm_xbit);

                            xbit_unset_match = 1;

                        }


                        /* direction: by_dst */

                        else if ( rulestruct[rule_position].xbit_direction[i] == 3 &&
                                  has_ip_dst && 
                                  0 == memcmp(xbit_ipc[a].ip_dst, ip_dst, sizeof(ip_dst)) ) {

                            if ( debug->debugxbit) {
                                Sagan_Log(S_DEBUG, "[%s, line %d] \"unset\" xbit \"%s\" (direction: \"by_dst\"). (any -> %s)", __FILE__, __LINE__, xbit_ipc[a].xbit_name, ip_dst_char);
                            }

                            File_Lock(config->shm_xbit);
                            pthread_mutex_lock(&Xbit_Mutex);

                            xbit_ipc[a].xbit_state = false;

                            pthread_mutex_unlock(&Xbit_Mutex);
                            File_Unlock(config->shm_xbit);

                            xbit_unset_match = 1;

                        }

                        /* direction: reverse */

                        else if ( rulestruct[rule_position].xbit_direction[i] == 4 &&
                                  has_ip_src && has_ip_dst && 
                                  0 == memcmp(xbit_ipc[a].ip_dst, ip_src, sizeof(ip_src)) &&
                                  0 == memcmp(xbit_ipc[a].ip_src, ip_dst, sizeof(ip_dst)) )

                        {

                            if ( debug->debugxbit) {
                                Sagan_Log(S_DEBUG, "[%s, line %d] \"unset\" xbit \"%s\" (direction: \"reverse\"). (%s -> %s)", __FILE__, __LINE__, xbit_ipc[a].xbit_name, ip_dst_char, ip_src_char);
                            }

                            File_Lock(config->shm_xbit);
                            pthread_mutex_lock(&Xbit_Mutex);

                            xbit_ipc[a].xbit_state = false;

                            pthread_mutex_unlock(&Xbit_Mutex);
                            File_Unlock(config->shm_xbit);

                            xbit_unset_match = 1;

                        }

                        /* direction: src_xbitdst */

                        else if ( rulestruct[rule_position].xbit_direction[i] == 5 &&
                                  has_ip_src && 
                                  0 == memcmp(xbit_ipc[a].ip_dst, ip_src, sizeof(ip_src)) ) {

                            if ( debug->debugxbit) {
                                Sagan_Log(S_DEBUG, "[%s, line %d] \"unset\" xbit \"%s\" (direction: \"src_xbitdst\"). (any -> %s)", __FILE__, __LINE__, xbit_ipc[a].xbit_name, ip_src_char);
                            }

                            File_Lock(config->shm_xbit);
                            pthread_mutex_lock(&Xbit_Mutex);

                            xbit_ipc[a].xbit_state = 0;

                            pthread_mutex_unlock(&Xbit_Mutex);
                            File_Unlock(config->shm_xbit);

                            xbit_unset_match = 1;

                        }

                        /* direction: dst_xbitsrc */

                        else if ( rulestruct[rule_position].xbit_direction[i] == 6 &&
                                  has_ip_dst && 
                                  0 == memcmp(xbit_ipc[a].ip_src, ip_dst, sizeof(ip_dst)) ) {

                            if ( debug->debugxbit) {
                                Sagan_Log(S_DEBUG, "[%s, line %d] \"unset\" xbit \"%s\" (direction: \"dst_xbitsrc\"). (any -> %s)", __FILE__, __LINE__, xbit_ipc[a].xbit_name, ip_dst_char);
                            }

                            File_Lock(config->shm_xbit);
                            pthread_mutex_lock(&Xbit_Mutex);

                            xbit_ipc[a].xbit_state = 0;

                            pthread_mutex_unlock(&Xbit_Mutex);
                            File_Unlock(config->shm_xbit);

                            xbit_unset_match = 1;

                        }

                        /* direction: both_p */

                        else if ( rulestruct[rule_position].xbit_direction[i] == 7 &&
                                  has_ip_src && has_ip_dst &&
                                  0 == memcmp(xbit_ipc[a].ip_src, ip_src, sizeof(ip_src)) &&
                                  0 == memcmp(xbit_ipc[a].ip_dst, ip_dst, sizeof(ip_dst)) &&
                                  xbit_ipc[a].src_port == src_port &&
                                  xbit_ipc[a].dst_port == dst_port ) {

                            if ( debug->debugxbit) {
                                Sagan_Log(S_DEBUG, "[%s, line %d] \"unset\" xbit \"%s\" (direction: \"both_p\"). (%s -> %s)", __FILE__, __LINE__, xbit_ipc[a].xbit_name, ip_src_char, ip_dst_char);
                            }

                            File_Lock(config->shm_xbit);
                            pthread_mutex_lock(&Xbit_Mutex);

                            xbit_ipc[a].xbit_state = 0;

                            pthread_mutex_unlock(&Xbit_Mutex);
                            File_Unlock(config->shm_xbit);

                            xbit_unset_match = 1;

                        }

                        /* direction: by_src_p */

                        else if ( rulestruct[rule_position].xbit_direction[i] == 8 &&
                                  has_ip_src &&
                                  0 == memcmp(xbit_ipc[a].ip_src, ip_src, sizeof(ip_src)) &&
                                  xbit_ipc[a].src_port == src_port )

                        {

                            if ( debug->debugxbit) {
                                Sagan_Log(S_DEBUG, "[%s, line %d] \"unset\" xbit \"%s\" (direction: \"by_src_p\"). (%s -> any)", __FILE__, __LINE__, xbit_ipc[a].xbit_name, ip_src_char);
                            }

                            File_Lock(config->shm_xbit);
                            pthread_mutex_lock(&Xbit_Mutex);

                            xbit_ipc[a].xbit_state = 0;

                            pthread_mutex_unlock(&Xbit_Mutex);
                            File_Unlock(config->shm_xbit);

                            xbit_unset_match = 1;

                        }


                        /* direction: by_dst_p */

                        else if ( rulestruct[rule_position].xbit_direction[i] == 9 &&
                                  has_ip_dst && 
                                  0 == memcmp(xbit_ipc[a].ip_dst, ip_dst, sizeof(ip_dst)) &&
                                  xbit_ipc[a].dst_port == dst_port ) {

                            if ( debug->debugxbit) {
                                Sagan_Log(S_DEBUG, "[%s, line %d] \"unset\" xbit \"%s\" (direction: \"by_dst\"). (any -> %s)", __FILE__, __LINE__, xbit_ipc[a].xbit_name, ip_dst_char);
                            }

                            File_Lock(config->shm_xbit);
                            pthread_mutex_lock(&Xbit_Mutex);

                            xbit_ipc[a].xbit_state = 0;

                            pthread_mutex_unlock(&Xbit_Mutex);
                            File_Unlock(config->shm_xbit);

                            xbit_unset_match = 1;

                        }

                        /* direction: reverse_p */

                        else if ( rulestruct[rule_position].xbit_direction[i] == 10 &&
                                  has_ip_dst &&  has_ip_src &&
                                  0 == memcmp(xbit_ipc[a].ip_dst, ip_src, sizeof(ip_src)) &&
                                  0 == memcmp(xbit_ipc[a].ip_src, ip_dst, sizeof(ip_dst)) &&
                                  xbit_ipc[a].src_port == dst_port &&
                                  xbit_ipc[a].dst_port == src_port )

                        {

                            if ( debug->debugxbit) {
                                Sagan_Log(S_DEBUG, "[%s, line %d] \"unset\" xbit \"%s\" (direction: \"reverse_p\"). (%s -> %s)", __FILE__, __LINE__, xbit_ipc[a].xbit_name, ip_dst_char, ip_src_char);
                            }

                            File_Lock(config->shm_xbit);
                            pthread_mutex_lock(&Xbit_Mutex);

                            xbit_ipc[a].xbit_state = 0;

                            pthread_mutex_unlock(&Xbit_Mutex);
                            File_Unlock(config->shm_xbit);

                            xbit_unset_match = 1;

                        }

                        /* direction: src_xbitdst_p */

                        else if ( rulestruct[rule_position].xbit_direction[i] == 11 &&
                                  has_ip_src && 
                                  0 == memcmp(xbit_ipc[a].ip_dst, ip_src, sizeof(ip_src)) &&
                                  xbit_ipc[a].dst_port == src_port ) {

                            if ( debug->debugxbit) {
                                Sagan_Log(S_DEBUG, "[%s, line %d] \"unset\" xbit \"%s\" (direction: \"src_xbitdst_p\"). (any -> %s)", __FILE__, __LINE__, xbit_ipc[a].xbit_name, ip_src_char);
                            }

                            File_Lock(config->shm_xbit);
                            pthread_mutex_lock(&Xbit_Mutex);

                            xbit_ipc[a].xbit_state = 0;

                            pthread_mutex_unlock(&Xbit_Mutex);
                            File_Unlock(config->shm_xbit);

                            xbit_unset_match = 1;

                        }

                        /* direction: dst_xbitsrc_p */

                        else if ( rulestruct[rule_position].xbit_direction[i] == 12 &&
                                  has_ip_dst &&
                                  0 == memcmp(xbit_ipc[a].ip_src, ip_dst, sizeof(ip_dst)) &&
                                  xbit_ipc[a].src_port == dst_port ) {

                            if ( debug->debugxbit) {
                                Sagan_Log(S_DEBUG, "[%s, line %d] \"unset\" xbit \"%s\" (direction: \"dst_xbitsrc_p\"). (any -> %s)", __FILE__, __LINE__, xbit_ipc[a].xbit_name, ip_dst_char);
                            }

                            File_Lock(config->shm_xbit);
                            pthread_mutex_lock(&Xbit_Mutex);

                            xbit_ipc[a].xbit_state = 0;

                            pthread_mutex_unlock(&Xbit_Mutex);
                            File_Unlock(config->shm_xbit);

                            xbit_unset_match = 1;

                        }

                    }
                }

                if ( debug->debugxbit && xbit_unset_match == 0 ) {
                    Sagan_Log(S_DEBUG, "[%s, line %d] No xbit found to \"unset\" for %s.", __FILE__, __LINE__, tmp_xbit_name);
                }

                tmp_xbit_name = strtok_r(NULL, "&", &tok);
            }
        } /* While & xbits (ie - bit1&bit2) */

        /*******************
         *      SET        *
        *******************/

        else if ( rulestruct[rule_position].xbit_type[i] == 1 ) {

            xbit_match = false;

            /* Xbits & (ie - bit1&bit2) */

            strlcpy(tmp, rulestruct[rule_position].xbit_name[i], sizeof(tmp));
            tmp_xbit_name = strtok_r(tmp, "&", &tok);

            while( tmp_xbit_name != NULL ) {

                for (a = 0; a < counters_ipc->xbit_count; a++) {
                    // Short circuit if no selector match
                    if (
                            (NULL == selector && xbit_ipc[a].selector[0] != '\0') || 
                            (NULL != selector && 0 != strcmp(selector, xbit_ipc[a].selector))
                       ) {

                            continue;
                    }

                    /* Do we have the xbit already in memory?  If so,  update the information */

                    if (!strcmp(xbit_ipc[a].xbit_name, tmp_xbit_name) &&
                        0 == memcmp(xbit_ipc[a].ip_src, ip_src, sizeof(ip_src)) &&
                        0 == memcmp(xbit_ipc[a].ip_dst, ip_dst, sizeof(ip_src)) &&
                        xbit_ipc[a].src_port == config->sagan_port &&
                        xbit_ipc[a].dst_port == config->sagan_port ) {

                        File_Lock(config->shm_xbit);
                        pthread_mutex_lock(&Xbit_Mutex);

                        xbit_ipc[a].xbit_date = atol(timet);
                        xbit_ipc[a].xbit_expire = atol(timet) + rulestruct[rule_position].xbit_timeout[i];
                        xbit_ipc[a].xbit_state = true;

                        if ( debug->debugxbit) {
                            Sagan_Log(S_DEBUG, "[%s, line %d] [%d] Updated via \"set\" for xbit \"%s\", [%d].  New expire time is %d (%d) [0x%.08X%.08X%.08X%.08X -> 0x%.08X%.08X%.08X%.08X] (%s).", __FILE__, __LINE__, a, tmp_xbit_name, i, xbit_ipc[i].xbit_expire, rulestruct[rule_position].xbit_timeout[i], 
                                    htonl(((unsigned int *)&xbit_ipc[a].ip_src)[0]), 
                                    htonl(((unsigned int *)&xbit_ipc[a].ip_src)[1]), 
                                    htonl(((unsigned int *)&xbit_ipc[a].ip_src)[2]), 
                                    htonl(((unsigned int *)&xbit_ipc[a].ip_src)[3]), 
                                    htonl(((unsigned int *)&xbit_ipc[a].ip_dst)[0]), 
                                    htonl(((unsigned int *)&xbit_ipc[a].ip_dst)[1]), 
                                    htonl(((unsigned int *)&xbit_ipc[a].ip_dst)[2]), 
                                    htonl(((unsigned int *)&xbit_ipc[a].ip_dst)[3]),
                                    xbit_ipc[a].selector
                            );

                        }

                        pthread_mutex_unlock(&Xbit_Mutex);
                        File_Unlock(config->shm_xbit);

                        xbit_match = true;
                    }

                }


                /* If the xbit isn't in memory,  store it to be created later */

                if ( xbit_match == false ) {

                    xbit_track = ( _Sagan_Xbit_Track * ) realloc(xbit_track, (xbit_track_count+1) * sizeof(_Sagan_Xbit_Track));

                    if ( xbit_track == NULL ) {
                        Sagan_Log(S_ERROR, "[%s, line %d] Failed to reallocate memory for xbit_track. Abort!", __FILE__, __LINE__);
                    }

                    strlcpy(xbit_track[xbit_track_count].xbit_name, tmp_xbit_name, sizeof(xbit_track[xbit_track_count].xbit_name));
                    xbit_track[xbit_track_count].xbit_timeout = rulestruct[rule_position].xbit_timeout[i];
                    xbit_track[xbit_track_count].xbit_srcport = config->sagan_port;
                    xbit_track[xbit_track_count].xbit_dstport = config->sagan_port;
                    xbit_track_count++;

                }

                tmp_xbit_name = strtok_r(NULL, "&", &tok);

            } /* While & xbits (ie - bit1&bit2) */

        } /* if xbit_type == 1 */

        /***************************
         *      SET_SRCPORT        *
        ****************************/

        else if ( rulestruct[rule_position].xbit_type[i] == 5 ) {

            xbit_match = false;

            /* Xbits & (ie - bit1&bit2) */

            strlcpy(tmp, rulestruct[rule_position].xbit_name[i], sizeof(tmp));
            tmp_xbit_name = strtok_r(tmp, "&", &tok);

            while( tmp_xbit_name != NULL ) {

                for (a = 0; a < counters_ipc->xbit_count; a++) {
                    // Short circuit if no selector match
                    if (
                            (NULL == selector && xbit_ipc[a].selector[0] != '\0') || 
                            (NULL != selector && 0 != strcmp(selector, xbit_ipc[a].selector))
                       ) {

                            continue;
                    }
 
                    /* Do we have the xbit already in memory?  If so,  update the information */

                    if (!strcmp(xbit_ipc[a].xbit_name, tmp_xbit_name) &&
                        0 == memcmp(xbit_ipc[a].ip_src, ip_src, sizeof(ip_src)) &&
                        0 == memcmp(xbit_ipc[a].ip_dst, ip_dst, sizeof(ip_dst)) &&
                        xbit_ipc[a].src_port == src_port &&
                        xbit_ipc[a].dst_port == config->sagan_port ) {

                        File_Lock(config->shm_xbit);
                        pthread_mutex_lock(&Xbit_Mutex);

                        xbit_ipc[a].xbit_date = atol(timet);
                        xbit_ipc[a].xbit_expire = atol(timet) + rulestruct[rule_position].xbit_timeout[i];
                        xbit_ipc[a].xbit_state = true;

                        if ( debug->debugxbit) {
                           Sagan_Log(S_DEBUG, "[%s, line %d] [%d] Updated via \"set_srcport\" for xbit \"%s\", [%d].  New expire time is %d (%d) [0x%.08X%.08X%.08X%.08X -> 0x%.08X%.08X%.08X%.08X] (%s).", __FILE__, __LINE__, a, tmp_xbit_name, i, xbit_ipc[i].xbit_expire, rulestruct[rule_position].xbit_timeout[i],
                                    htonl(((unsigned int *)&xbit_ipc[a].ip_src)[0]),
                                    htonl(((unsigned int *)&xbit_ipc[a].ip_src)[1]),
                                    htonl(((unsigned int *)&xbit_ipc[a].ip_src)[2]),
                                    htonl(((unsigned int *)&xbit_ipc[a].ip_src)[3]),
                                    htonl(((unsigned int *)&xbit_ipc[a].ip_dst)[0]),
                                    htonl(((unsigned int *)&xbit_ipc[a].ip_dst)[1]),
                                    htonl(((unsigned int *)&xbit_ipc[a].ip_dst)[2]),
                                    htonl(((unsigned int *)&xbit_ipc[a].ip_dst)[3]),
                                    xbit_ipc[a].selector);
                        }

                        pthread_mutex_unlock(&Xbit_Mutex);
                        File_Unlock(config->shm_xbit);

                        xbit_match = true;
                    }

                }


                /* If the xbit isn't in memory,  store it to be created later */

                if ( xbit_match == false ) {

                    xbit_track = ( _Sagan_Xbit_Track * ) realloc(xbit_track, (xbit_track_count+1) * sizeof(_Sagan_Xbit_Track));

                    if ( xbit_track == NULL ) {
                        Sagan_Log(S_ERROR, "[%s, line %d] Failed to reallocate memory for xbit_track. Abort!", __FILE__, __LINE__);
                    }

                    strlcpy(xbit_track[xbit_track_count].xbit_name, tmp_xbit_name, sizeof(xbit_track[xbit_track_count].xbit_name));
                    xbit_track[xbit_track_count].xbit_timeout = rulestruct[rule_position].xbit_timeout[i];
                    xbit_track[xbit_track_count].xbit_srcport = src_port;
                    xbit_track[xbit_track_count].xbit_dstport = config->sagan_port;
                    xbit_track_count++;

                }

                tmp_xbit_name = strtok_r(NULL, "&", &tok);

            } /* While & xbits (ie - bit1&bit2) */

        } /* if xbit_type == 5 */

        /***************************
         *      SET_DSTPORT        *
        ****************************/

        else if ( rulestruct[rule_position].xbit_type[i] == 6 ) {

            xbit_match = false;

            /* Xbits & (ie - bit1&bit2) */

            strlcpy(tmp, rulestruct[rule_position].xbit_name[i], sizeof(tmp));
            tmp_xbit_name = strtok_r(tmp, "&", &tok);

            while( tmp_xbit_name != NULL ) {

                for (a = 0; a < counters_ipc->xbit_count; a++) {
                    // Short circuit if no selector match
                    if (
                            (NULL == selector && xbit_ipc[a].selector[0] != '\0') || 
                            (NULL != selector && 0 != strcmp(selector, xbit_ipc[a].selector))
                       ) {

                            continue;
                    }
 
                    /* Do we have the xbit already in memory?  If so,  update the information */

                    if (!strcmp(xbit_ipc[a].xbit_name, tmp_xbit_name) &&
                        0 == memcmp(xbit_ipc[a].ip_src, ip_src, sizeof(ip_src)) &&
                        0 == memcmp(xbit_ipc[a].ip_dst, ip_dst, sizeof(ip_dst)) &&
                        xbit_ipc[a].src_port == config->sagan_port &&
                        xbit_ipc[a].dst_port == dst_port ) {

                        File_Lock(config->shm_xbit);
                        pthread_mutex_lock(&Xbit_Mutex);

                        xbit_ipc[a].xbit_date = atol(timet);
                        xbit_ipc[a].xbit_expire = atol(timet) + rulestruct[rule_position].xbit_timeout[i];
                        xbit_ipc[a].xbit_state = true;

                        if ( debug->debugxbit) {
                           Sagan_Log(S_DEBUG, "[%s, line %d] [%d] Updated via \"set_dstport\" for xbit \"%s\", [%d].  New expire time is %d (%d) [0x%.08X%.08X%.08X%.08X -> 0x%.08X%.08X%.08X%.08X] (%s).", __FILE__, __LINE__, a, tmp_xbit_name, i, xbit_ipc[i].xbit_expire, rulestruct[rule_position].xbit_timeout[i],
                                    htonl(((unsigned int *)&xbit_ipc[a].ip_src)[0]),
                                    htonl(((unsigned int *)&xbit_ipc[a].ip_src)[1]),
                                    htonl(((unsigned int *)&xbit_ipc[a].ip_src)[2]),
                                    htonl(((unsigned int *)&xbit_ipc[a].ip_src)[3]),
                                    htonl(((unsigned int *)&xbit_ipc[a].ip_dst)[0]),
                                    htonl(((unsigned int *)&xbit_ipc[a].ip_dst)[1]),
                                    htonl(((unsigned int *)&xbit_ipc[a].ip_dst)[2]),
                                    htonl(((unsigned int *)&xbit_ipc[a].ip_dst)[3]),
                                    xbit_ipc[a].selector);
                        }

                        pthread_mutex_unlock(&Xbit_Mutex);
                        File_Unlock(config->shm_xbit);

                        xbit_match = true;
                    }

                }


                /* If the xbit isn't in memory,  store it to be created later */

                if ( xbit_match == false ) {

                    xbit_track = ( _Sagan_Xbit_Track * ) realloc(xbit_track, (xbit_track_count+1) * sizeof(_Sagan_Xbit_Track));

                    if ( xbit_track == NULL ) {
                        Sagan_Log(S_ERROR, "[%s, line %d] Failed to reallocate memory for xbit_track. Abort!", __FILE__, __LINE__);
                    }

                    strlcpy(xbit_track[xbit_track_count].xbit_name, tmp_xbit_name, sizeof(xbit_track[xbit_track_count].xbit_name));
                    xbit_track[xbit_track_count].xbit_timeout = rulestruct[rule_position].xbit_timeout[i];
                    xbit_track[xbit_track_count].xbit_srcport = config->sagan_port;
                    xbit_track[xbit_track_count].xbit_dstport = dst_port;
                    xbit_track_count++;

                }

                tmp_xbit_name = strtok_r(NULL, "&", &tok);

            } /* While & xbits (ie - bit1&bit2) */

        } /* if xbit_type == 6 */

        /*************************
         *      SET_PORTS        *
        **************************/

        else if ( rulestruct[rule_position].xbit_type[i] == 7 ) {

            xbit_match = false;

            /* Xbits & (ie - bit1&bit2) */

            strlcpy(tmp, rulestruct[rule_position].xbit_name[i], sizeof(tmp));
            tmp_xbit_name = strtok_r(tmp, "&", &tok);

            while( tmp_xbit_name != NULL ) {

                for (a = 0; a < counters_ipc->xbit_count; a++) {
                    // Short circuit if no selector match
                    if (
                            (NULL == selector && xbit_ipc[a].selector[0] != '\0') || 
                            (NULL != selector && 0 != strcmp(selector, xbit_ipc[a].selector))
                       ) {

                            continue;
                    }

                    /* Do we have the xbit already in memory?  If so,  update the information */

                    if (!strcmp(xbit_ipc[a].xbit_name, tmp_xbit_name) &&
                        0 == memcmp(xbit_ipc[a].ip_src, ip_src, sizeof(ip_src)) &&
                        0 == memcmp(xbit_ipc[a].ip_dst, ip_dst, sizeof(ip_dst)) &&
                        xbit_ipc[a].src_port == src_port &&
                        xbit_ipc[a].dst_port == dst_port ) {

                        File_Lock(config->shm_xbit);
                        pthread_mutex_lock(&Xbit_Mutex);

                        xbit_ipc[a].xbit_date = atol(timet);
                        xbit_ipc[a].xbit_expire = atol(timet) + rulestruct[rule_position].xbit_timeout[i];
                        xbit_ipc[a].xbit_state = true;

                        if ( debug->debugxbit) {
                           Sagan_Log(S_DEBUG, "[%s, line %d] [%d] Updated via \"set_ports\" for xbit \"%s\", [%d].  New expire time is %d (%d) [0x%.08X%.08X%.08X%.08X -> 0x%.08X%.08X%.08X%.08X] (%s).", __FILE__, __LINE__, a, tmp_xbit_name, i, xbit_ipc[i].xbit_expire, rulestruct[rule_position].xbit_timeout[i],
                                    htonl(((unsigned int *)&xbit_ipc[a].ip_src)[0]),
                                    htonl(((unsigned int *)&xbit_ipc[a].ip_src)[1]),
                                    htonl(((unsigned int *)&xbit_ipc[a].ip_src)[2]),
                                    htonl(((unsigned int *)&xbit_ipc[a].ip_src)[3]),
                                    htonl(((unsigned int *)&xbit_ipc[a].ip_dst)[0]),
                                    htonl(((unsigned int *)&xbit_ipc[a].ip_dst)[1]),
                                    htonl(((unsigned int *)&xbit_ipc[a].ip_dst)[2]),
                                    htonl(((unsigned int *)&xbit_ipc[a].ip_dst)[3]),
                                    xbit_ipc[a].selector);
                        }

                        pthread_mutex_unlock(&Xbit_Mutex);
                        File_Unlock(config->shm_xbit);

                        xbit_match = true;
                    }

                }


                /* If the xbit isn't in memory,  store it to be created later */

                if ( xbit_match == false ) {

                    xbit_track = ( _Sagan_Xbit_Track * ) realloc(xbit_track, (xbit_track_count+1) * sizeof(_Sagan_Xbit_Track));

                    if ( xbit_track == NULL ) {
                        Sagan_Log(S_ERROR, "[%s, line %d] Failed to reallocate memory for xbit_track. Abort!", __FILE__, __LINE__);
                    }

                    strlcpy(xbit_track[xbit_track_count].xbit_name, tmp_xbit_name, sizeof(xbit_track[xbit_track_count].xbit_name));
                    xbit_track[xbit_track_count].xbit_timeout = rulestruct[rule_position].xbit_timeout[i];
                    xbit_track[xbit_track_count].xbit_srcport = src_port;
                    xbit_track[xbit_track_count].xbit_dstport = dst_port;
                    xbit_track_count++;

                }

                tmp_xbit_name = strtok_r(NULL, "&", &tok);

            } /* While & xbits (ie - bit1&bit2) */

        } /* if xbit_type == 7 */

    } /* Out of for i loop */

    /* Do we have any xbits in memory that need to be created?  */

    if ( xbit_track_count != 0 ) {

        for (i = 0; i < xbit_track_count; i++) {

            if ( Clean_IPC_Object(XBIT) == 0 ) {

                File_Lock(config->shm_xbit);
                pthread_mutex_lock(&Xbit_Mutex);

                memcpy(xbit_ipc[counters_ipc->xbit_count].ip_src, ip_src, sizeof(ip_src));
                memcpy(xbit_ipc[counters_ipc->xbit_count].ip_dst, ip_dst, sizeof(ip_dst));
                NULL == selector ? xbit_ipc[counters_ipc->xbit_count].selector[0] = '\0' : strlcpy(xbit_ipc[counters_ipc->xbit_count].selector, selector, MAXSELECTOR);
                xbit_ipc[counters_ipc->xbit_count].src_port = xbit_track[i].xbit_srcport;
                xbit_ipc[counters_ipc->xbit_count].dst_port = xbit_track[i].xbit_dstport;
                xbit_ipc[counters_ipc->xbit_count].xbit_date = atol(timet);
                xbit_ipc[counters_ipc->xbit_count].xbit_expire = atol(timet) + xbit_track[i].xbit_timeout;
                xbit_ipc[counters_ipc->xbit_count].xbit_state = true;
                xbit_ipc[counters_ipc->xbit_count].expire = xbit_track[i].xbit_timeout;

                strlcpy(xbit_ipc[counters_ipc->xbit_count].xbit_name, xbit_track[i].xbit_name, sizeof(xbit_ipc[counters_ipc->xbit_count].xbit_name));

                if ( debug->debugxbit) {
                    Sagan_Log(S_DEBUG, "[%s, line %d] [%d] Created xbit \"%s\" via \"set, set_srcport, set_dstport, or set_ports\" [%s:%d -> %s:%d],", __FILE__, __LINE__, counters_ipc->xbit_count, xbit_ipc[counters_ipc->xbit_count].xbit_name, ip_src_char, xbit_track[i].xbit_srcport, ip_dst_char, xbit_track[i].xbit_dstport);
                }

                File_Lock(config->shm_counters);

                counters_ipc->xbit_count++;

                File_Unlock(config->shm_counters);
                File_Unlock(config->shm_xbit);

                pthread_mutex_unlock(&Xbit_Mutex);

            }
        }
    }

    free(xbit_track);

} /* End of Xbit_Set */

/*****************************************************************************
 * Xbit_Cleanup - Find "expired" xbits and toggle the "state"
 * to "off"
 *****************************************************************************/

void Xbit_Cleanup_MMAP(void)
{

    int i = 0;

    time_t t;
    struct tm *now;
    char  timet[20];

    t = time(NULL);
    now=localtime(&t);
    strftime(timet, sizeof(timet), "%s",  now);


    for (i=0; i<counters_ipc->xbit_count; i++) {
        if (  xbit_ipc[i].xbit_state == true && atol(timet) >= xbit_ipc[i].xbit_expire ) {
            if (debug->debugxbit) {
                Sagan_Log(S_DEBUG, "[%s, line %d] Setting xbit %s to \"expired\" state.", __FILE__, __LINE__, xbit_ipc[i].xbit_name);
            }
            xbit_ipc[i].xbit_state = false;
        }
    }

}
