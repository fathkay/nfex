/*
 * nfex.h - top level header
 *
 * 2009, 2010 Mike Schiffman <mschiffm@cisco.com> 
 *
 * Copyright (c) 2010 by Cisco Systems, Inc.
 * All rights reserved.
 * Based off of tcpxtract by Nicholas Harbour.
 */

#ifndef NFEX_H
#define NFEX_H

#define _BSD_SOURCE 1

#include <pcap.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <termios.h>
#include "sessionlist.h"
#include "config.h"

#if (HAVE_GEOIP)
#include <GeoIP.h>
#include <GeoIPCity.h>
#ifndef NFEX_GEOIP_CONFIG_FILE
#define NFEX_GEOIP_CONFIG_FILE "/usr/local/etc/nfex/GeoIPCity.dat"
#endif
#endif

#ifndef NFEX_DEFAULT_CONFIG_FILE
#define NFEX_DEFAULT_CONFIG_FILE "/usr/local/etc/nfex/nfex.conf"
#endif

/** as we add more protocols this needs to change */
#define NFEX_PCAP_FILTER "tcp"

/* BEGIN MACROS */
/** simple way to subtract timeval based timers */
#define PTIMERSUB(tvp, uvp, vvp)                                             \
    do                                                                       \
    {                                                                        \
            (vvp)->tv_sec  = (tvp)->tv_sec  - (uvp)->tv_sec;                 \
            (vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec;                \
            if ((vvp)->tv_usec < 0)                                          \
            {                                                                \
                (vvp)->tv_sec--;                                             \
                (vvp)->tv_usec += 1000000;                                   \
            }                                                                \
    } while (0)

#ifndef MAX
#define MAX( x, y ) ((x) > (y) ? (x) : (y))
#endif
/* END MACROS */

/** statistics */
struct nfex_statistics
{
    u_int32_t total_packets;          /* total packets seen */
    u_int64_t total_bytes;            /* total bytes read */
    u_int32_t total_files;            /* total files extracted */
    u_int32_t packet_errors;          /* packet-level errors */
    u_int32_t extraction_errors;      /* extraction errors */
    struct timeval ts_start;          /* total uptime timestamp */
    struct timeval ts_last;           /* last file extracted timestamp */
    u_int32_t ip_last;                /* last packet seen ip */
};
typedef struct nfex_statistics n_stats_t;

/** monolithic opaque control context, everything imporant is here */
struct nfex_control_context
{
    pcap_t *p;                        /* pcap context */
    int pcap_fd;                      /* pcap fd used to select across */
    char *device;                     /* pcap device */
    slist_t *sessions;                /* master packet session list */
    slist_t *session;                 /* candidate packet session */
    slist_t *tail;                    /* end of the session list */
    uint32_t session_count;           /* number of sessions */
    struct termios term;              /* save terminal info to restore later */
    uint16_t flags;                   /* control context flags */
#define NFEX_VERBOSE       0x0001     /* toggle verbosity */
#define NFEX_GEOIP         0x0002     /* toggle geoIP mode */
#define NFEX_DEBUG_NS      0x0004     /* debug mode: notify session updates */
#define NFEX_SESSIONS_LOCK 0x0008     /* locked, don't go in here */
    FILE *log;                        /* logfile FILE descriptor */
#if (HAVE_GEOIP)
    GeoIP *gi;                        /* geoip database pointer */
    char geoip_data[128];             /* geoip database path */
#endif /** HAVE_GEOIP */
    char yyinfname[128];
    srch_node_t *srch_machine;
    char output_dir[128];             /* output directory prefix */
    u_int16_t filenum;                /* number of files we've written */
    char indexfname[128];
    FILE *indexfp;
    char capfname[128];               /* pcap capture file name */
    off_t capfsize;                   /* size of capfile */
    n_stats_t stats;                  /* stats */
    char errbuf[PCAP_ERRBUF_SIZE];    /* bad things reported here */
};
typedef struct nfex_control_context ncc_t;

/** call back when we have a packet */
void process_packet(u_char *, const struct pcap_pkthdr *, const u_char *);
void quit_signal(int);

/** initialization functions */
ncc_t *control_context_init(char *, char *, char *, char *, char *, uint16_t, 
char *);
void control_context_destroy(ncc_t *);

/** main loop functions */
int the_game(ncc_t *);
int process_keypress(ncc_t *);

/**
 * log_init
 *
 * INPUTS
 * logfile: canonical name of the file to open, should be MMP_LOGFILE
 * ncc: pointer to ncc context
 *
 * RETURNS
 * 1 on success, -1 on failure
 *
 * DISCUSSION
 * opens logfile for appending, will create if it doesn't exist.
 *
 */
int log_init(char *logfile, ncc_t *ncc);

/**
 * log_msg
 *
 * INPUTS
 * priority: the level of the message (see above)
 * ncc: pointer to acc context
 * fmt: as used for printf family of functions
 * ...: as used for printf family of functions
 *
 * RETURNS
 * none
 *
 * DISCUSSION
 * writes a message to the system logfile, has a maximum length of
 * MMP_LOG_MAX (should this be tunable at boot time?)
 *
 *
 */
void log_msg(u_int8_t priority, ncc_t *ncc, char *fmt, ...);
void log_close(ncc_t *ncc);

/** extraction functions */
static void add_extract(extract_list_t **, fileid_t *, slist_t *, int, int,
ncc_t *);
static void set_segment_marks(extract_list_t *, size_t);
static void mark_footer(extract_list_t *, srch_results_t *);
static void extract_segment(extract_list_t *, const uint8_t *, ncc_t *);
static void sweep_extract_list(extract_list_t **);
static  int open_extract(char *, uint32_t src_ip, uint16_t src_prt, 
                         uint32_t dst_ip, uint16_t dst_prt, ncc_t *);
void printip(uint32_t ip, ncc_t *ncc);
void extract(extract_list_t **elist, srch_results_t *results, slist_t *session,
             const uint8_t *data, size_t size, ncc_t *ncc);

/** misc functions */
#define NFEX_STATS_UPDATE   0
#define NFEX_STATS_CLOSEOUT 1
void stats(ncc_t *n, int mode);
void usage(char *);
void quit_signal(int sig);
void print_hex(uint8_t *, uint16_t);
void convert_seconds(uint32_t, uint32_t *, uint32_t *, uint32_t *, 
                     uint32_t *);

/** session list functions */
slist_t *session_add(four_tuple_t *ft, ncc_t *ncc);
slist_t *session_find(four_tuple_t *ft, ncc_t *ncc);
void session_prune(ncc_t *ncc);
uint32_t count_extractions(slist_t *slist);
uint32_t session_count(ncc_t *ncc);
#if (DEBUG_MODE)
extern void session_dump(ncc_t *ncc);
#endif



#endif /** NFEX_H */
/** EOF */
