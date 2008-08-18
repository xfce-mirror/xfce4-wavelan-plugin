/* $Id: wi_bsd.c 562 2004-12-03 18:29:41Z benny $ */
/*-
 * Copyright (c) 2003 Benedikt Meurer <benny@xfce.org>
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

#if defined(__NetBSD__) || defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__OpenBSD__) 

#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_media.h>
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#include <net/if_var.h>
#include <net/ethernet.h>

#include <dev/wi/if_wavelan_ieee.h>
#if __FreeBSD_version >= 500033
#include <sys/endian.h>
#endif
#else
#include <netinet/in.h>
#include <netinet/if_ether.h>
#ifdef __NetBSD__
#include <net80211/ieee80211.h>
#include <net80211/ieee80211_ioctl.h>
#include <dev/ic/wi_ieee.h>
#else
#if !defined(__OpenBSD__)
#include <dev/pcmcia/if_wavelan_ieee.h>
#endif
#endif
#ifdef __OpenBSD__
#include <dev/ic/if_wi_ieee.h>
#include <net80211/ieee80211.h>
#include <net80211/ieee80211_ioctl.h>
#endif
#endif

#if defined(__GLIBC__)

#define  strlcpy(dst, src, size) g_strlcpy(dst, src, size)

#include <byteswap.h>
#include <endian.h>
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define le16toh(x)      ((uint16_t)(x))
#elif __BYTE_ORDER == __BIG_ENDIAN
#define le16toh(x)      bswap16((x))
#else
#error unknown ENDIAN
#endif

#endif /* __GLIBC__ */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>

#include <wi.h>

struct wi_device
{
  char interface[WI_MAXSTRLEN];
  int socket;
};

static int _wi_carrier(const struct wi_device *);
#if defined(__NetBSD__) || defined(__FreeBSD__)
static int _wi_getval(const struct wi_device *, struct wi_req *);
static int _wi_vendor(const struct wi_device *, char *, size_t);
#endif
static int _wi_netname(const struct wi_device *, char *, size_t);
static int _wi_quality(const struct wi_device *, int *);
static int _wi_rate(const struct wi_device *, int *);

struct wi_device *
wi_open(const char *interface)
{
  struct wi_device *device = NULL;

  if (interface != NULL) {
    if ((device = (struct wi_device *)calloc(1, sizeof(*device))) != NULL) {
      strlcpy(device->interface, interface, WI_MAXSTRLEN);

      if ((device->socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        free(device);
        device = NULL;
      }
    }
  }

  return(device);
}

void
wi_close(struct wi_device *device)
{
  if (device != NULL) {
    close(device->socket);
    free(device);
  }
}

int
wi_query(struct wi_device *device, struct wi_stats *stats)
{
  int result;

  if (device == NULL || stats == NULL)
    return(WI_INVAL);

  /* clear stats first */
  bzero((void *)stats, sizeof(*stats));

  /* check vendor (independent of carrier state) */
#if !defined(__OpenBSD__)
  if ((result = _wi_vendor(device, stats->ws_vendor, WI_MAXSTRLEN)) != WI_OK)
    return(result);
#endif

  /* check carrier */
  if ((result = _wi_carrier(device)) != WI_OK)
    return(result);
  else {
    /* check netname (depends on carrier state) */
    if ((result = _wi_netname(device,stats->ws_netname, WI_MAXSTRLEN)) != WI_OK)
      return(result);

    /* check quality (depends on carrier state) */
    if ((result = _wi_quality(device, &stats->ws_quality)) != WI_OK)
      return(result);

    /* check rate (depends on carrier state) */
    if ((result = _wi_rate(device, &stats->ws_rate)) != WI_OK)
      return(result);

    /* everything ok, stats are up-to-date */
    return(WI_OK);
  }
}

static int
_wi_carrier(const struct wi_device *device)
{
  struct ifmediareq ifmr;

  bzero((void*)&ifmr, sizeof(ifmr));
  strlcpy(ifmr.ifm_name, device->interface, sizeof(ifmr.ifm_name));

  if (ioctl(device->socket, SIOCGIFMEDIA, &ifmr) < 0) {
    /*
     * Interface does not support SIOCGIFMEDIA ioctl,
     * assume no such device.
     */
    return(WI_NOSUCHDEV);
  }

	if ((ifmr.ifm_status & IFM_AVALID) == 0) {
		/*
		 * Interface doesn't report media-valid status.
		 * assume no such device.
		 */
		return(WI_NOSUCHDEV);
	}

	/* otherwise, return ok for active, not-ok if not active. */
	return((ifmr.ifm_status & IFM_ACTIVE) != 0 ? WI_OK : WI_NOCARRIER);
}

/* OpenBSD uses net80211 API */
#if defined(__OpenBSD__)
static int
_wi_netname(const struct wi_device *device, char *buffer, size_t len)
{
  int result;
  struct ifreq ifr;
  struct ieee80211_nwid nwid;

  bzero((void *) &ifr, sizeof(ifr));
  ifr.ifr_data = (caddr_t) & nwid;
  strlcpy(ifr.ifr_name, device->interface, sizeof(ifr.ifr_name));
  if ((result = ioctl(device->socket, SIOCG80211NWID, (caddr_t) & ifr)) != WI_OK)
    return (result);

  strlcpy(buffer, (char *) nwid.i_nwid, MIN(len, strlen(nwid.i_nwid) + 1));

  return(WI_OK);
}

static int
_wi_quality(const struct wi_device *device, int *quality)
{
  int result;
  struct ieee80211_nodereq nr;
  struct ieee80211_bssid bssid;

  bzero((void *) &bssid, sizeof(bssid));
  bzero((void *) &nr, sizeof(nr));

  /* get i_bssid from interface */
  strlcpy(bssid.i_name, device->interface, sizeof(bssid.i_name));
  if((result = ioctl(device->socket, SIOCG80211BSSID, (caddr_t) &bssid)) != WI_OK)
    return (result);

  /* put i_bssid into nr_macaddr to get nr_rssi */
  bcopy(bssid.i_bssid, &nr.nr_macaddr, sizeof(nr.nr_macaddr));
  strlcpy(nr.nr_ifname, device->interface, sizeof(nr.nr_ifname));
  if ((result = ioctl(device->socket, SIOCG80211NODE, (caddr_t) & nr)) != WI_OK)
    return (result);

  /* clearly broken, but stolen from ifconfig.c */
  if (nr.nr_max_rssi)
    *quality = IEEE80211_NODEREQ_RSSI(&nr); /* value in percentage */
  else
    *quality = nr.nr_rssi; /* value in decibels */

  return(WI_OK);
}

static int
_wi_rate(const struct wi_device *device, int *rate)
{
  int result;
  struct ieee80211_nodereq nr;
  struct ieee80211_bssid bssid;

  bzero((void *) &bssid, sizeof(bssid));
  bzero((void *) &nr, sizeof(nr));

  /* get i_bssid from interface */
  strlcpy(bssid.i_name, device->interface, sizeof(bssid.i_name));
  if((result = ioctl(device->socket, SIOCG80211BSSID, (caddr_t) &bssid)) != WI_OK)
    return (result);

  /* put i_bssid into nr_macaddr to get nr_rssi */
  bcopy(bssid.i_bssid, &nr.nr_macaddr, sizeof(nr.nr_macaddr));
  strlcpy(nr.nr_ifname, device->interface, sizeof(nr.nr_ifname));
  if ((result = ioctl(device->socket, SIOCG80211NODE, (caddr_t) & nr)) != WI_OK)
    return (result);

  /* stolen from ifconfig.c too, print only higher rate */
  if (nr.nr_nrates)
    *rate = (nr.nr_rates[nr.nr_nrates - 1] & IEEE80211_RATE_VAL) / 2;
  else
    *rate = 0;
  return(WI_OK);
}
#endif

/* NetBSD and FreeBSD use old wi_* API */
#if defined(__NetBSD__) || defined(__FreeBSD__)
static int
_wi_getval(const struct wi_device *device, struct wi_req *wr)
{
  struct ifreq ifr;

  bzero((void*)&ifr, sizeof(ifr));
  strlcpy(ifr.ifr_name, device->interface, sizeof(ifr.ifr_name));
  ifr.ifr_data = (void*)wr;

  if (ioctl(device->socket, SIOCGWAVELAN, &ifr) < 0)
    return(WI_NOSUCHDEV);

  return(WI_OK);
}

static int
_wi_vendor(const struct wi_device *device, char *buffer, size_t len)
{
#define WI_RID_STA_IDENTITY_LUCENT	0x1
#define WI_RID_STA_IDENTITY_PRISMII	0x2
#define WI_RID_STA_IDENTITY_SAMSUNG	0x3
#define WI_RID_STA_IDENTITY_DLINK	0x6
  const char* vendor = "Unknown";
  struct wi_req wr;
  int result;

  bzero((void*)&wr, sizeof(wr));
  wr.wi_len = WI_MAX_DATALEN;
  wr.wi_type = WI_RID_STA_IDENTITY;

  if ((result = _wi_getval(device, &wr)) != WI_OK){
    /* For the Atheros, IDENTITY does not work. */
    if (strcmp(device->interface, "ath") != 0)
      return(result);
  }
  else if (wr.wi_len < 4)
    return(WI_NOSUCHDEV);

  switch (wr.wi_val[1]) {
  case WI_RID_STA_IDENTITY_LUCENT:
    vendor = "Lucent";
    break;

  case WI_RID_STA_IDENTITY_PRISMII:
    vendor = "generic PRISM II";
    break;

  case WI_RID_STA_IDENTITY_SAMSUNG:
		vendor = "Samsung";
		break;
	case WI_RID_STA_IDENTITY_DLINK:
		vendor = "D-Link";
		break;
  }

  snprintf(buffer, len, "%s (ID %d, version %d.%d)", vendor,
      wr.wi_val[0], wr.wi_val[2], wr.wi_val[3]);

  return(WI_OK);
}

static int
_wi_netname(const struct wi_device *device, char *buffer, size_t len)
{
  struct wi_req wr;
  int result;

  bzero((void *)&wr, sizeof(wr));
  wr.wi_len = WI_MAX_DATALEN;
  wr.wi_type = WI_RID_CURRENT_SSID;

  if ((result = _wi_getval(device, &wr)) != WI_OK)
    return(result);

  strlcpy(buffer, (char *)&wr.wi_val[1], MIN(len, le16toh(wr.wi_val[0]) + 1));

  return(WI_OK);
}

static int
_wi_quality(const struct wi_device *device, int *quality)
{
  struct wi_req wr;
  int result;

  bzero((void *)&wr, sizeof(wr));
  wr.wi_len = WI_MAX_DATALEN;
  wr.wi_type = WI_RID_COMMS_QUALITY;

  if ((result = _wi_getval(device, &wr)) != WI_OK)
    return(result);

  *quality = le16toh(wr.wi_val[1]);

  return(WI_OK);
}

static int
_wi_rate(const struct wi_device *device, int *rate)
{
  struct wi_req wr;
  int result;

  bzero((void *)&wr, sizeof(wr));
  wr.wi_len = WI_MAX_DATALEN;
  wr.wi_type = WI_RID_CUR_TX_RATE;

  if ((result = _wi_getval(device, &wr)) != WI_OK)
    return(result);

  *rate = le16toh(wr.wi_val[0]);

  return(WI_OK);
}

#endif  /* defined(__NetBSD__) || defined(__FreeBSD__) */
#endif
