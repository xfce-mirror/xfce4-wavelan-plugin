/* Copyright (c) 2003,2004 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2004 An-Cheng Huang <pach@cs.cmu.edu>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if defined(__linux__)

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libxfce4util/libxfce4util.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

/* On newer linux headers these need to be
 * included first. It is probably a losing
 * battle though.
 */
#include <linux/types.h>
#include <linux/if.h>

/* Require wireless extensions */
#include <linux/wireless.h> 

#include <wi.h>

struct wi_device
{
  char interface[WI_MAXSTRLEN];
  int socket;
};

struct wi_device *
wi_open(const char *interface)
{
  struct wi_device *device;
  int sock;

  g_return_val_if_fail(interface != NULL, NULL);

  if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
    TRACE ("Failed to open socket, %d", sock);
    return(NULL);
  }

  TRACE ("Socket Open for interface %s, %d",interface,sock);
  
  device = g_new0(struct wi_device, 1);
  device->socket = sock;
  g_strlcpy(device->interface, interface, WI_MAXSTRLEN);

  return(device);
}

static void
close(int fd)
{
  shutdown(fd, SHUT_RDWR);
}

void
wi_close(struct wi_device *device)
{
  g_return_if_fail(device != NULL);

  close(device->socket);
  g_free(device);
}

static double
wi_get_max_quality(struct wi_device *device)
{
  struct iwreq wreq;
  double max_qual = 92.0;
  char range_buf[sizeof(struct iw_range) * 2]; // wireless tools says it is
                                              // large enough.
  int result;

  /* Set interface name */
  strncpy(wreq.ifr_name, device->interface, IFNAMSIZ);

  memset(range_buf, 0, sizeof(range_buf));

  wreq.u.data.pointer = (caddr_t) range_buf;
  wreq.u.data.length = sizeof(range_buf);
  wreq.u.data.flags = 0;
  if ((result = ioctl(device->socket, SIOCGIWRANGE, &wreq)) < 0) {
    TRACE ("Couldn't get range information, taking default.");
  } else {
    struct iw_range *range = (struct iw_range *) range_buf;
    max_qual = range->max_qual.qual;
    if (max_qual <= 0) {
      TRACE ("Got a negative value for max_qual, returning to default.");
      max_qual = 92.0;
    }
  }

  return max_qual;
}

int
wi_query(struct wi_device *device, struct wi_stats *stats)
{
#if WIRELESS_EXT <= 11
  char buffer[1024];
  char *bp;
  FILE *fp;
#endif
  double link;
  long level;
  int result;
  double max_qual = 92.0;

  struct iwreq wreq;
  struct iw_statistics wstats;
  char essid[IW_ESSID_MAX_SIZE + 1];

  g_return_val_if_fail(device != NULL, WI_INVAL);
  g_return_val_if_fail(stats != NULL, WI_INVAL);

  /* FIXME */
  g_strlcpy(stats->ws_qunit, "%", 2);
  g_strlcpy(stats->ws_vendor, _("Unknown"), WI_MAXSTRLEN);

  /* Set interface name */
  strncpy(wreq.ifr_name, device->interface, IFNAMSIZ);

  /* Get ESSID */
  wreq.u.essid.pointer = (caddr_t) essid;
  wreq.u.essid.length = IW_ESSID_MAX_SIZE + 1;
  wreq.u.essid.flags = 0;
  if ((result = ioctl(device->socket, SIOCGIWESSID, &wreq) < 0)) {
    TRACE ("Couldn't get ESSID");
    g_strlcpy(stats->ws_netname, "", WI_MAXSTRLEN);
  } else {
    /* ESSID is possibly NOT null terminated but we know its length */
    essid[wreq.u.essid.length] = 0;
    TRACE ("ESSID is %s", essid);
    g_strlcpy(stats->ws_netname, essid, WI_MAXSTRLEN);
  }

  /* Get bit rate */
  if ((result = ioctl(device->socket, SIOCGIWRATE, &wreq) < 0)) {
    TRACE ("Couldn't get bit-rate");
    stats->ws_rate = 0;
  } else {
    TRACE ("Bit-rate is %d", wreq.u.bitrate.value);
    /* bitrate is in b/s, transform to Mb/s */
    stats->ws_rate = wreq.u.bitrate.value / (1000 * 1000);
  }

#if WIRELESS_EXT > 11
  /* Get interface stats through ioctl */
  wreq.u.data.pointer = (caddr_t) &wstats;
  wreq.u.data.length = sizeof(struct iw_statistics);
  wreq.u.data.flags = 1;
  if ((result = ioctl(device->socket, SIOCGIWSTATS, &wreq)) < 0) {
    TRACE ("Returning NOSUCHDEV, got %d for socket %d", result, device->socket);
    return(WI_NOSUCHDEV);
  }
  level = wstats.qual.level;
  link = wstats.qual.qual;

  max_qual = wi_get_max_quality(device);

#else /* WIRELESS_EXT <= 11 */
  /* Get interface stats through /proc/net/wireless */
  if ((fp = fopen("/proc/net/wireless", "r")) == NULL) {
    return(WI_NOSUCHDEV);
  }

  for (;;) {
    if (fgets(buffer, sizeof(buffer), fp) == NULL)
      return(WI_NOSUCHDEV);

    if (buffer[6] == ':') {
      buffer[6] = '\0';
      for (bp = buffer; isspace(*bp); ++bp);

      if (strcmp(bp, device->interface) != 0)
        continue;

      /* we found our device, read the stats now */
      bp = buffer + 12;
      link = strtod(bp, &bp);
      level = strtol(bp + 1, &bp, 10);
      break;
    }
  }
  fclose(fp);
#endif

  /* check if we have a carrier signal */
  /* FIXME: does 0 mean no carrier? */
  if (level <= 0)
    return(WI_NOCARRIER);

  /* calculate link quality */
  if (link <= 0)
    stats->ws_quality = 0;
  else {
    /* thanks to google and wireless tools for this hint */
    stats->ws_quality = (int)rint(log(link) / log(max_qual) * 100.0);
    TRACE ("Quality: %2f, max quality: %2f", link, max_qual);
  }

  return(WI_OK);
}

#endif  /* !defined(__linux__) */
