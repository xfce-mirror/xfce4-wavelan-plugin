/* Copyright (c) 2003 Benedikt Meurer <benny@xfce.org>
 *               2008 Landry Breuil <landry@xfce.org>
 *                    (OpenBSD support)
 *               2008 Pietro Cerutti <gahr@gahr.ch>
 *                    (FreeBSD > 700000 adaptation)
 *               2014 J.R. Oldroyd <fbsd@opal.com> 
 *                    (Enhance FreeBSD support)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/param.h>

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
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <sys/endian.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <net80211/ieee80211_ioctl.h>
#else
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
#include <net/if_media.h>
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
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
static int _wi_vendor(const struct wi_device *, char *, size_t);
static int _wi_getval(const struct wi_device *, struct ieee80211req_scan_result *);
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

#if defined(__OpenBSD__)
  strlcpy(stats->ws_qunit, "%", 2);
#else
  strlcpy(stats->ws_qunit, "dBm", 4);
#endif
  /* check vendor (independent of carrier state) */
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
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

const struct ifmedia_baudrate ifm_baudrate_descriptions[] =
    IFM_BAUDRATE_DESCRIPTIONS;

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
  /* nr_rssi is in dBm, convert to % via
   https://stackoverflow.com/questions/15797920/how-to-convert-wifi-signal-strength-from-quality-percent-to-rssi-dbm
  */
  else if (nr.nr_rssi <= -100)
    *quality = 0;
  else if (nr.nr_rssi >= -50)
    *quality = 100;
  else
    *quality = 2 * (100 + nr.nr_rssi);

  return(WI_OK);
}


static int
_wi_rate(const struct wi_device *device, int *rate)
{
  const struct ifmedia_baudrate *desc;
  struct ifmediareq ifmr;
  uint64_t *media_list;
  int result;

  *rate = 0;
  (void) memset(&ifmr, 0, sizeof(ifmr));
  (void) strlcpy(ifmr.ifm_name, device->interface, sizeof(ifmr.ifm_name));

  // get the amount of media types
  if ((result = ioctl(device->socket, SIOCGIFMEDIA, (caddr_t) & ifmr)) != WI_OK) {
    return (result);
  }
  if (ifmr.ifm_count == 0) {
    return -1;
  }
  media_list = calloc(ifmr.ifm_count, sizeof(*media_list));
  if (media_list == NULL) {
    return -1;
  }
  ifmr.ifm_ulist = media_list;
  if ((result = ioctl(device->socket, SIOCGIFMEDIA, (caddr_t) & ifmr)) != WI_OK) {
    return (result);
  }

  for (desc = ifm_baudrate_descriptions; desc->ifmb_word != 0; desc++) {
    if (IFM_TYPE_MATCH(desc->ifmb_word, ifmr.ifm_active) &&
        IFM_SUBTYPE(desc->ifmb_word) == IFM_SUBTYPE(ifmr.ifm_active)) {
      *rate = desc->ifmb_baudrate / 1000000;
      return(WI_OK);
    }
  }
  return(WI_OK);
}
#endif

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
static int
_wi_vendor(const struct wi_device *device, char *buffer, size_t len)
{
   /*
    * We use sysctl to get a device description
    */
   char mib[WI_MAXSTRLEN];
   char dev_name[WI_MAXSTRLEN];
   char *c = dev_name;
   int  dev_number;

   /*
    * Dirty hack to split the device name into name and number
    */
   strncpy(dev_name, device->interface, WI_MAXSTRLEN);
   while(!isdigit(*c)) c++;
   dev_number = (int)strtol(c, NULL, 10);
   *c = '\0';

   /*
    * Also, wlan doesn't present dev.wlan.N.%desc, need to use
    * underlying driver name, instead: dev.ath.0.%desc, so we
    * need to get the parent's name.
    */
   if(strcmp(dev_name, "wlan") == 0) {
      snprintf(mib, sizeof(mib), "net.%s.%d.%%parent", dev_name, dev_number);
      if(sysctlbyname(mib, dev_name, &len, NULL, 0) == -1)
         return (WI_NOSUCHDEV);

      c = dev_name;
      while(!isdigit(*c)) c++;
      dev_number = (int)strtol(c, NULL, 10);
      *c = '\0';
      len = WI_MAXSTRLEN;
   }

   snprintf(mib, sizeof(mib), "dev.%s.%d.%%desc", dev_name, dev_number);
   if(sysctlbyname(mib, buffer, &len, NULL, 0) == -1)
      return (WI_NOSUCHDEV);

  return(WI_OK);
}

static int
_wi_getval(const struct wi_device *device, struct ieee80211req_scan_result *scan)
{
   char buffer[24 * 1024];
   const uint8_t *bp;
   struct ieee80211req ireq;
   size_t len;
   bzero(&ireq, sizeof(ireq));
   strlcpy(ireq.i_name, device->interface, sizeof(ireq.i_name));

   ireq.i_type = IEEE80211_IOC_SCAN_RESULTS;
   ireq.i_data = buffer;
   ireq.i_len = sizeof(buffer);

   if(ioctl(device->socket, SIOCG80211, &ireq) < 0)
      return(WI_NOSUCHDEV);

   if(ireq.i_len < sizeof(struct ieee80211req_scan_result))
      return(WI_NOSUCHDEV);

   memcpy(scan, buffer, sizeof(struct ieee80211req_scan_result));

   return(WI_OK);
}
#endif

#if defined(__NetBSD__)
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
#endif

#if defined(__NetBSD__) || defined(__FreeBSD__) ||  defined(__FreeBSD_kernel__)

static int
_wi_netname(const struct wi_device *device, char *buffer, size_t len)
{
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
   struct ieee80211req ireq;

   memset(&ireq, 0, sizeof(ireq));
   strncpy(ireq.i_name, device->interface, sizeof(ireq.i_name));
   ireq.i_type = IEEE80211_IOC_SSID;
   ireq.i_val = -1;
   ireq.i_data = buffer;
   ireq.i_len = len; 
   if (ioctl(device->socket, SIOCG80211, &ireq) < 0) 
      return WI_NOSUCHDEV;
#elif defined(__NetBSD__)
  struct wi_req wr;
  int result;

  bzero((void *)&wr, sizeof(wr));
  wr.wi_len = WI_MAX_DATALEN;
  wr.wi_type = WI_RID_CURRENT_SSID;

  if ((result = _wi_getval(device, &wr)) != WI_OK)
    return(result);

  strlcpy(buffer, (char *)&wr.wi_val[1], MIN(len, le16toh(wr.wi_val[0]) + 1));
#endif

  return(WI_OK);
}

static int
_wi_quality(const struct wi_device *device, int *quality)
{
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
   struct ieee80211req_scan_result req;
   int result;
   bzero(&req, sizeof(req));

   if((result = _wi_getval(device, &req)) != WI_OK)
      return (result);

   /*
    * FreeBSD's wlan stats:
    *	signal (in dBm) = rssi * 2 + noise;
    *	quality_bars    = 4 * (signal - noise);
    * or
    *	quality_bars    = rssi * 8;
    * but, per wi_query(), above, we need to return strength in dBm, so... 
    */
   *quality = req.isr_rssi * 2 + req.isr_noise;
#elif defined(__NetBSD__)
  struct wi_req wr;
  int result;

  bzero((void *)&wr, sizeof(wr));
  wr.wi_len = WI_MAX_DATALEN;
  wr.wi_type = WI_RID_COMMS_QUALITY;

  if ((result = _wi_getval(device, &wr)) != WI_OK)
    return(result);

  /* according to various implementation (conky, ifconfig) :
     wi_val[0] = quality, wi_val[1] = signal, wi_val[2] = noise
     but my ral only shows a value for signal, and it seems it's a dB value */
  *quality = le16toh(wr.wi_val[1]);
#endif

  return(WI_OK);
}

static int
_wi_rate(const struct wi_device *device, int *rate)
{
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
   struct ieee80211req_scan_result req;
   int result, i, high;
   bzero(&req, sizeof(req));

   if((result = _wi_getval(device, &req)) != WI_OK)
      return (result);

   for(i=0, high=-1; i<req.isr_nrates; i++)
      if((req.isr_rates[i] & IEEE80211_RATE_VAL) > high)
         high = req.isr_rates[i] & IEEE80211_RATE_VAL;
   
   *rate = high / 2;
#elif defined(__NetBSD__)
  struct wi_req wr;
  int result;

  bzero((void *)&wr, sizeof(wr));
  wr.wi_len = WI_MAX_DATALEN;
  wr.wi_type = WI_RID_CUR_TX_RATE;

  if ((result = _wi_getval(device, &wr)) != WI_OK)
    return(result);

  *rate = le16toh(wr.wi_val[0]);
#endif

  return(WI_OK);
}

#endif  /* defined(__NetBSD__) || || defined(__FreeBSD__) || defined(__FreeBSD_kernel__) */
#endif
