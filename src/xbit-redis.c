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

#ifdef HAVE_LIBHIREDIS

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>

#include "sagan.h"
#include "sagan-defs.h"
#include "sagan-config.h"

#include "rules.h"

#include "xbit.h"
#include "xbit-mmap.h"
#include "xbit-redis.h"
#include "parsers/parsers.h"
#include "redis.h"

struct _SaganConfig *config;
struct _Rule_Struct *rulestruct;
struct _SaganDebug *debug;
struct _SaganCounters *counters;

pthread_mutex_t CounterRedisWriterThreadsDrop=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t RedisMutex=PTHREAD_MUTEX_INITIALIZER;

int redis_msgslot = 0;
pthread_cond_t SaganRedisDoWork=PTHREAD_COND_INITIALIZER;
pthread_mutex_t SaganRedisWorkMutex=PTHREAD_MUTEX_INITIALIZER;

struct _Sagan_Redis *SaganRedis;

/****************************************************************
   README * README * README * README * README * README * README
   README * README * README * README * README * README * README
 ****************************************************************

 This is very PoC (proof of concept) code and is NOT production
 ready.  This is to test the functionality of using Redis as a
 backend to store "xbits" (making them "global" xbits.

 NOTES:
	 src/dst do not need to be part of the HMSET.
         Do we event need "active" in HMSET?

         How to deal with ZADD to index? Manually delete? ignore?
	 "clean up?" evevy "x" times?

	 What if HGET is null during unset?
         Not very good config checks being done.  You can set
         xbit-storage to "redis", but NOT have a redis-server:
	 sections!



 ****************************************************************/

/*****************************************************************************
 Xbit_Condition_Redis - Test the condition of xbits.  For example,  "isset"
 and "isnotset"
 *****************************************************************************/

sbool Xbit_Condition_Redis(int rule_position, char *ip_src_char, char *ip_dst_char, int src_port, int dst_port, char *selector )
{

    int i;
    int j;

    int xbit_total_match = 0;

    time_t t;
    struct tm *now;
    char  timet[20];

    redisReply *reply;

    char redis_tmp[256] = { 0 };

    char redis_command[256] = { 0 };
    char redis_reply[16] = { 0 };

    char tmp[128];
    char *tmp_xbit_name = NULL;
    char *tok = NULL;

    uint32_t djb2_hash;

    t = time(NULL);
    now=localtime(&t);
    strftime(timet, sizeof(timet), "%s",  now);

    int and_or = false;

    char *src_or_dst = NULL;
    char *src_or_dst_type = NULL;

    if ( debug->debugredis ) {
        Sagan_Log(S_DEBUG, "[%s, line %d] Redis Xbit Condition.", __FILE__, __LINE__);
    }

    /* Cycle through xbits in the rule */

    for (i = 0; i < rulestruct[rule_position].xbit_count; i++) {

        /* Only dealing with isset and isnotset */

        if ( rulestruct[rule_position].xbit_type[i] == 3 || rulestruct[rule_position].xbit_type[i] == 4 ) {

            strlcpy(tmp, rulestruct[rule_position].xbit_name[i], sizeof(tmp));

            /* Determine if there are any | or &. If so,  we'll cycle through
                   all xbits */

            if (Sagan_strstr(rulestruct[rule_position].xbit_name[i], "|")) {

                tmp_xbit_name = strtok_r(tmp, "|", &tok);
                and_or = true;

            } else {

                tmp_xbit_name = strtok_r(tmp, "&", &tok);
                and_or = false;

            }

            /* Cycle through all xbits,  if needed */

            while (tmp_xbit_name != NULL ) {

                /* direction: none - may add support for this later. */

                if ( rulestruct[rule_position].xbit_direction[i] == 0 ) {

                    {
                        Sagan_Log(S_WARN, "[%s, line %d] Call for \"isset\" or \"isnotset\" xbit \"%s\" with Redis is not supported! \"unset\" needs an IP source or destination", __FILE__, __LINE__, tmp_xbit_name);
                    }

                }

                /*****************************************************************/
                /* direction: both - this is the easiest as we have all the data */
                /*****************************************************************/

                else if ( rulestruct[rule_position].xbit_direction[i] == 1 ) {

                    if ( debug->debugxbit ) {
                        Sagan_Log(S_DEBUG, "[%s, line %d] \"isset\" xbit \"%s\" (direction: \"both\"). (%s -> %s)", __FILE__, __LINE__, tmp_xbit_name, ip_src_char, ip_dst_char);
                    }


                    /* Since we have source, destination and xbit name,  we can generate the
                       DJB2 hash without having to dig to far into Redis */

                    snprintf(redis_tmp, sizeof(redis_tmp), "%s-%s-%s-%s", tmp_xbit_name, selector, ip_src_char, ip_dst_char);
                    djb2_hash = Djb2_Hash(redis_tmp);

                    snprintf(redis_command, sizeof(redis_command), "HGET %u name", djb2_hash);
                    Redis_Reader(redis_command, redis_reply, sizeof(redis_reply));

                    /* If the xbit is found ... */

                    if ( redis_reply[0] != '\0' ) {

                        /* isset */

                        if ( rulestruct[rule_position].xbit_type[i] == 3 ) {

                            if ( debug->debugxbit ) {
                                Sagan_Log(S_DEBUG, "[%s, line %d] Found xbit '%s' for 'isset'.", __FILE__, __LINE__, tmp_xbit_name );
                            }

                            /* The rule has a |, we can short circuit here */

                            if ( and_or == true ) {

                                if ( debug->debugxbit ) {
                                    Sagan_Log(S_DEBUG, "[%s, line %d] '|' set or only one xbit used, returning TRUE", __FILE__, __LINE__, tmp_xbit_name );
                                }

                                return(true);
                            }

                            /* No | in the rule,  so increment the match counter */

                            xbit_total_match++;

                        } /* End of rulestruct[rule_position].xbit_type[i] == 3 */

                    } else {  /* End of reply->str != NULL */

                        /* No match was found */

                        /* isnotset */

                        if ( rulestruct[rule_position].xbit_type[i] == 4 ) {

                            if ( debug->debugxbit ) {
                                Sagan_Log(S_DEBUG, "[%s, line %d] Did not find xbit '%s' for 'isnotset'.", __FILE__, __LINE__, tmp_xbit_name );
                            }

                            /* If the run contains &'s we can short circuit here */

                            if ( and_or == false ) {

                                if ( debug->debugxbit ) {
                                    Sagan_Log(S_DEBUG, "[%s, line %d] AND in isnotset, returning TRUE.", __FILE__, __LINE__, tmp_xbit_name );
                                }

                                return(true);

                            }

                            /* The rule contain no &,  so increment the match counter */

                            xbit_total_match++;

                        } /* End of rulestruct[rule_position].xbit_type[i] == 4 */

                    } /* End of else reply->str != NULL */

                } /* End of if (rulestruct[rule_position].xbit_direction[i] == 1 || both ) */

                /*******************************/
                /* direction: by_src || by_dst */
                /*******************************/

                /* Since by_src and by_dst similar Redis queries,  we handle both
                           here */

                if ( rulestruct[rule_position].xbit_direction[i] == 2 ||
                     rulestruct[rule_position].xbit_direction[i] == 3 ) {

                    pthread_mutex_lock(&RedisMutex);

                    if ( rulestruct[rule_position].xbit_direction[i] == 2 ) {

                        src_or_dst = "xbit_src_index";
                        src_or_dst_type = "ip_src";

                        snprintf( redis_tmp, sizeof(redis_tmp), "%s-%s", selector, ip_src_char);
                    } else {

                        src_or_dst = "xbit_dst_index";
                        src_or_dst_type = "ip_dst";

                        snprintf( redis_tmp, sizeof(redis_tmp), "%s-%s", selector, ip_dst_char);
                    }

                    djb2_hash = Djb2_Hash(redis_tmp);
                    snprintf(redis_command, sizeof(redis_command), "ZRANGEBYSCORE %s %u %u", src_or_dst, djb2_hash, djb2_hash);

                    /* Look for the source or destination in our "index" */

                    reply = redisCommand(config->c_reader_redis, redis_command);

                    if ( debug->debugredis ) {
                        Sagan_Log(S_DEBUG, "[%s, line %d] Redis Command: \"%s\"", __FILE__, __LINE__, redis_command);
                        Sagan_Log(S_DEBUG, "[%s, line %d] Redis Reply: \"%s\"", __FILE__, __LINE__, reply->str);
                    }

                    /**************************************************************/
                    /* If nothing is found,  we can stop a lot of processing here.*/
                    /**************************************************************/

                    if ( reply->elements == 0 ) {

                        /* "isset" - If nothing is found then no need to continue */

                        if ( rulestruct[rule_position].xbit_type[i] == 3 ) {

                            if ( debug->debugxbit ) {

                                Sagan_Log(S_DEBUG, "[%s, line %d] No xbits found, returning FALSE", __FILE__, __LINE__ );
                            }

                            pthread_mutex_unlock(&RedisMutex);
                            return(false);

                        }

                        /* isnotset .... */

                        else if ( rulestruct[rule_position].xbit_type[i] == 4 ) {

                            /* If we are looking for flowbit1&flowbit2 and flowbit1 is
                               not set,  we can short circuit now */

                            if ( and_or == false ) {

                                if ( debug->debugxbit ) {
                                    Sagan_Log(S_DEBUG, "[%s, line %d] '&' found in xbit set. Returning TRUE", __FILE__, __LINE__ );
                                }

                                pthread_mutex_unlock(&RedisMutex);
                                return(true);

                            }
                        }

                    } /* if ( reply->elements == 0 ) */


                    /******************************************************************/
                    /* If we've made it this far,  we have data/xbits to work with.   */
                    /* We start pulling Redis records to compare if it's what our     */
                    /* rule is looking for                                            */
                    /******************************************************************/

                    for (j = 0; j < reply->elements; j++) {

                        snprintf(redis_command, sizeof(redis_command), "HGET %s name", reply->element[j]->str);
                        Redis_Reader(redis_command, redis_reply, sizeof(redis_reply));

                        /* Does the flowbit in the rule match what is in Redis? */

                        if ( !strcmp(tmp_xbit_name, redis_reply ) ) {

                            /* The xbit in the rule and Redis match */

                            if ( debug->debugxbit ) {
                                Sagan_Log(S_DEBUG, "[%s, line %d] Found xbit '%s' by '%s' key %s. ", __FILE__, __LINE__, tmp_xbit_name, src_or_dst_type, reply->element[j]->str);
                            }

                            /* If "isset" ....  */

                            if ( rulestruct[rule_position].xbit_type[i] == 3 ) {

                                /* And the rule has a |, we can go ahead an return true
                                   and short circuit */

                                if ( and_or == true ) {

                                    if ( debug->debugxbit ) {
                                        Sagan_Log(S_DEBUG, "[%s, line %d] | set, returning TRUE", __FILE__, __LINE__, tmp_xbit_name );
                                    }

                                    pthread_mutex_unlock(&RedisMutex);
                                    return(true);
                                }

                                /* If the rule doesn't have | we increment the match
                                   counter */

                                xbit_total_match++;

                            }

                            /* if "isnotset" .... */

                            else if ( rulestruct[rule_position].xbit_type[i] == 4 ) {

                                /* And the rule has a &, we can go ahead an return true
                                   and short circuit */

                                if ( and_or == false ) {

                                    if ( debug->debugxbit ) {
                                        Sagan_Log(S_DEBUG, "[%s, line %d] '&' found in xbit set. Returning FALSE", __FILE__, __LINE__ );
                                    }

                                    pthread_mutex_unlock(&RedisMutex);
                                    return(false);
                                }

                                /* If the rule doesn't have & we increment the match
                                   counter */

                                xbit_total_match++;
                            }

                        } /* if ( !strcmp(tmp_xbit_name, reply_2->str ) */

                    } /* for (j = 0; j < reply->elements; j++ */

                    pthread_mutex_unlock(&RedisMutex);

                } /* rulestruct[rule_position].xbit_direction[i] == 2 || 3 */

                /************************************/
                /* If needed, move to the next xbit */
                /************************************/

                if ( and_or == true ) {
                    tmp_xbit_name = strtok_r(NULL, "|", &tok);
                } else {
                    tmp_xbit_name = strtok_r(NULL, "&", &tok);
                }

            } /* while (tmp_xbit_name != NULL */

        } /* rulestruct[rule_position].xbit_type[i] == 3 | 4 */

    } /* for (i = 0; .... */

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
        Sagan_Log(S_DEBUG, "[%s, line %d] Condition of xbit returning FALSE. Needed %d but got %d.", __FILE__, __LINE__, rulestruct[rule_position].xbit_condition_count, xbit_total_match);
    }

    return(false);
}

/*****************************************************************************
 * Xbit_Set_Redis - This will "set" and "unset" xbits in Redis
 *****************************************************************************/

void Xbit_Set_Redis(int rule_position, char *ip_src_char, char *ip_dst_char, int src_port, int dst_port, char *selector )
{

    time_t t;
    struct tm *now;
    char  timet[20];
    int i;
    int j;

    char *tmp_xbit_name = NULL;
    char tmp[128] = { 0 };
    char *tok = NULL;

    redisReply *reply;
    redisReply *reply_2;

    char redis_tmp[256] = { 0 };
    char redis_command[256] = { 0 };

    uint32_t djb2_hash;
    uint32_t djb2_hash_src;
    uint32_t djb2_hash_dst;


    if ( debug->debugredis ) {
        Sagan_Log(S_DEBUG, "[%s, line %d] Redis Xbit Xbit_Set_Redis()", __FILE__, __LINE__);
    }

    snprintf( redis_tmp, sizeof(redis_tmp), "%s-%s-%s-%s", tmp_xbit_name, selector, ip_src_char, ip_dst_char);
    djb2_hash = Djb2_Hash(redis_tmp);

    snprintf( redis_tmp, sizeof(redis_tmp), "%s-%s", selector, ip_src_char);
    djb2_hash_src = Djb2_Hash(redis_tmp);

    snprintf( redis_tmp, sizeof(redis_tmp), "%s-%s", selector, ip_dst_char);
    djb2_hash_dst = Djb2_Hash(redis_tmp);


    for (i = 0; i < rulestruct[rule_position].xbit_count; i++) {

        /* xbit SET */

        if ( rulestruct[rule_position].xbit_type[i] == 1 ) {

            t = time(NULL);
            now=localtime(&t);
            strftime(timet, sizeof(timet), "%s",  now);

            strlcpy(tmp, rulestruct[rule_position].xbit_name[i], sizeof(tmp));
            tmp_xbit_name = strtok_r(tmp, "&", &tok);

            while( tmp_xbit_name != NULL ) {

                 if ( redis_msgslot < config->redis_max_writer_threads ) {

                    snprintf(SaganRedis[redis_msgslot].redis_command, sizeof(SaganRedis[redis_msgslot].redis_command),

                             "HMSET %u-%s active 1 name %s selector %s src_ip %s dst_ip %s timestamp %s expire %d sensor %s;"
                             "EXPIRE %u %d;"
                             "ZADD xbit_src_index %u %u;"
                             "ZADD xbit_dst_index %u %u;",

                             djb2_hash, selector, tmp_xbit_name, selector, ip_src_char, ip_dst_char,  timet, rulestruct[rule_position].xbit_timeout[i],
                             config->sagan_sensor_name,

                             djb2_hash, rulestruct[rule_position].xbit_timeout[i],

                             djb2_hash, djb2_hash_src,
                             djb2_hash, djb2_hash_dst );

                    redis_msgslot++;

                    pthread_cond_signal(&SaganRedisDoWork);
                    pthread_mutex_unlock(&SaganRedisWorkMutex);

                } else {

                    Sagan_Log(S_WARN, "Out of Redis 'writer' threads for 'set'.  Skipping!");

                    pthread_mutex_lock(&CounterRedisWriterThreadsDrop);
                    counters->redis_writer_threads_drop++;
                    pthread_mutex_unlock(&CounterRedisWriterThreadsDrop);

                }

                tmp_xbit_name = strtok_r(NULL, "&", &tok);
            }
        }

        /* xbit UNSET */

        else if ( rulestruct[rule_position].xbit_type[i] == 2 ) {

            /* Xbits & (ie - bit1&bit2) */

            strlcpy(tmp, rulestruct[rule_position].xbit_name[i], sizeof(tmp));
            tmp_xbit_name = strtok_r(tmp, "&", &tok);

            while( tmp_xbit_name != NULL ) {

                /* direction: none */

                if ( rulestruct[rule_position].xbit_direction[i] == 0 ) {

                    {
                        Sagan_Log(S_WARN, "[%s, line %d] Call for \"unset\" xbit \"%s\" with Redis is not supported! \"unset\" needs an IP source or destination", __FILE__, __LINE__, tmp_xbit_name);
                    }

                }

                /* direction: both - This should be easiest since we have all
                   the data we need */

                else if ( rulestruct[rule_position].xbit_direction[i] == 1 ) {

                    snprintf(redis_tmp, sizeof(redis_tmp), "%s-%s-%s-%s", tmp_xbit_name, selector, ip_src_char, ip_dst_char);
                    djb2_hash = Djb2_Hash(redis_tmp);

                    pthread_mutex_lock(&RedisMutex);

                    snprintf(redis_command, sizeof(redis_command), "HMGET %u active", djb2_hash);

                    reply = redisCommand(config->c_reader_redis, redis_command);

                    if ( debug->debugredis ) {
                        Sagan_Log(S_DEBUG, "[%s, line %d] Redis Command: \"%s\"", __FILE__, __LINE__, redis_command);
                        Sagan_Log(S_DEBUG, "[%s, line %d] Redis Reply: \"%s\"", __FILE__, __LINE__, reply->element[0]->str);
                    }

                    /* If the values aren't there,  then no reason to delete */

                    if ( reply->element[0]->str != NULL ) {

                        if ( redis_msgslot < config->redis_max_writer_threads ) {

                            snprintf(SaganRedis[redis_msgslot].redis_command, sizeof(SaganRedis[redis_msgslot].redis_command),

                                     "ZREMRANGEBYSCORE xbit_dst_index %u %u;"
                                     "ZREMRANGEBYSCORE xbit_dst_index %u %u;"
                                     "DEL %u",

                                     djb2_hash_src, djb2_hash_src,
                                     djb2_hash_dst, djb2_hash_dst,
                                     djb2_hash );

                            redis_msgslot++;

                            pthread_cond_signal(&SaganRedisDoWork);
                            pthread_mutex_unlock(&SaganRedisWorkMutex);

                        } else {

                            Sagan_Log(S_WARN, "Out of Redis 'writer' threads for 'unset' by 'both'.  Skipping!");

                            pthread_mutex_lock(&CounterRedisWriterThreadsDrop);
                            counters->redis_writer_threads_drop++;
                            pthread_mutex_unlock(&CounterRedisWriterThreadsDrop);

                        }

                    }

                    pthread_mutex_unlock(&RedisMutex);

                } /* End of unset / both */

                /* direction: ip_src */

                else if ( rulestruct[rule_position].xbit_direction[i] == 2 ) {

                    pthread_mutex_lock(&RedisMutex);

                    snprintf(redis_command, sizeof(redis_command), "ZRANGEBYSCORE xbit_src_index %u %u", djb2_hash_src, djb2_hash_src);

                    reply = redisCommand(config->c_reader_redis, redis_command);

                    if ( debug->debugredis ) {
                        Sagan_Log(S_DEBUG, "[%s, line %d] Redis Command: \"%s\"", __FILE__, __LINE__, redis_command);
                        Sagan_Log(S_DEBUG, "[%s, line %d] Redis Reply: \"%s\"", __FILE__, __LINE__, reply->str);
                    }

                    if ( reply->elements == 0 ) {

                        if ( debug->debugxbit ) {
                            Sagan_Log(S_DEBUG, "[%s, line %d] \"unset\" by \"ip_src\" returned NULL result from Redis.  Exit function", __FILE__, __LINE__);
                        }

                        pthread_mutex_unlock(&RedisMutex);
                        return;
                    }


                    for (j = 0; j < reply->elements; j++) {

                        snprintf(redis_command, sizeof(redis_command), "HGET %s name", reply->element[j]->str);

                        reply_2 = redisCommand(config->c_reader_redis, redis_command);

                        if ( debug->debugredis ) {
                            Sagan_Log(S_DEBUG, "[%s, line %d] Redis Command: \"%s\"", __FILE__, __LINE__, redis_command);
                            Sagan_Log(S_DEBUG, "[%s, line %d] Redis Reply: \"%s\"", __FILE__, __LINE__, reply_2->str);
                        }

                        if ( !strcmp(tmp_xbit_name, reply_2->str ) ) {

                            if ( debug->debugxbit ) {
                                Sagan_Log(S_DEBUG, "[%s, line %d] Delete by 'ip_src' key %s", __FILE__, __LINE__, reply->element[j]->str);
                            }

                            if ( redis_msgslot < config->redis_max_writer_threads ) {

                                snprintf(SaganRedis[redis_msgslot].redis_command, sizeof(SaganRedis[redis_msgslot].redis_command),

                                         "ZREM xbit_src_index %s;"
                                         "ZREM xbit_dst_index %s;"
                                         "DEL %s",

                                         reply->element[j]->str,
                                         reply->element[j]->str,
                                         reply->element[j]->str );

                                redis_msgslot++;

                                pthread_cond_signal(&SaganRedisDoWork);
                                pthread_mutex_unlock(&SaganRedisWorkMutex);

                            } else {

                                Sagan_Log(S_WARN, "Out of Redis 'writer' threads for 'unset' by 'ip_src'.  Skipping!");

                                pthread_mutex_lock(&CounterRedisWriterThreadsDrop);
                                counters->redis_writer_threads_drop++;
                                pthread_mutex_unlock(&CounterRedisWriterThreadsDrop);

                            }

                        }
                    }

                    pthread_mutex_unlock(&RedisMutex);
                }

                /* direction: ip_dst */

                else if ( rulestruct[rule_position].xbit_direction[i] == 3 ) {

                    pthread_mutex_lock(&RedisMutex);

                    snprintf(redis_command, sizeof(redis_command), "ZRANGEBYSCORE xbit_dst_index %u %u", djb2_hash_dst, djb2_hash_dst);

                    reply = redisCommand(config->c_reader_redis, redis_command);

                    if ( debug->debugredis ) {
                        Sagan_Log(S_DEBUG, "[%s, line %d] Redis Command: \"%s\"", __FILE__, __LINE__, redis_command);
                        Sagan_Log(S_DEBUG, "[%s, line %d] Redis Reply: \"%s\"", __FILE__, __LINE__, reply->str);
                    }

                    if ( reply->elements == 0 ) {

                        if ( debug->debugxbit ) {
                            Sagan_Log(S_DEBUG, "[%s, line %d] \"unset\" by \"ip_dst\" returned NULL result from Redis.  Exit function", __FILE__, __LINE__);
                        }

                        pthread_mutex_unlock(&RedisMutex);
                        return;
                    }

                    for (j = 0; j < reply->elements; j++) {

                        snprintf(redis_command, sizeof(redis_command), "HGET %s name", reply->element[j]->str);

                        reply_2 = redisCommand(config->c_reader_redis, redis_command);

                        if ( debug->debugredis ) {
                            Sagan_Log(S_DEBUG, "[%s, line %d] Redis Command: \"%s\"", __FILE__, __LINE__, redis_command);
                            Sagan_Log(S_DEBUG, "[%s, line %d] Redis Reply: \"%s\"", __FILE__, __LINE__, reply_2->str);
                        }

                        if ( !strcmp(tmp_xbit_name, reply_2->str ) ) {

                            if ( debug->debugxbit ) {
                                Sagan_Log(S_DEBUG, "[%s, line %d] Delete by 'ip_dst' key %s", __FILE__, __LINE__, reply->element[j]->str);
                            }

                            if ( redis_msgslot < config->redis_max_writer_threads ) {

                                snprintf(SaganRedis[redis_msgslot].redis_command, sizeof(SaganRedis[redis_msgslot].redis_command),

                                         "ZREM xbit_dst_index %s;"
                                         "ZREM xbit_src_index %s;"
                                         "DEL %s",

                                         reply->element[j]->str,
                                         reply->element[j]->str,
                                         reply->element[j]->str );

                                redis_msgslot++;

                                pthread_cond_signal(&SaganRedisDoWork);
                                pthread_mutex_unlock(&SaganRedisWorkMutex);

                            } else {

                                Sagan_Log(S_WARN, "Out of Redis 'writer' threads for 'unset' by 'ip_dst'.  Skipping!");

                                pthread_mutex_lock(&CounterRedisWriterThreadsDrop);
                                counters->redis_writer_threads_drop++;
                                pthread_mutex_unlock(&CounterRedisWriterThreadsDrop);

                            }

                        }
                    }

                    pthread_mutex_unlock(&RedisMutex);
                }

                tmp_xbit_name = strtok_r(NULL, "&", &tok);

            } /* while( tmp_xbit_name != NULL ) */
        } /* else if ( rulestruct[rule_position].xbit_type[i] == 2 ) UNSET */
    } /* for (i = 0; i < rulestruct[rule_position].xbit_count; i++) */
}


#endif
