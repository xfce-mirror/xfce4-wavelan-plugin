/*
 * Copyright (c) 2003 Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de>
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

#if defined(__NetBSD__) || defined(__FreeBSD__)

#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_media.h>
#ifdef __FreeBSD__
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
#include <dev/pcmcia/if_wavelan_ieee.h>
#endif
#endif

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
static int _wi_getval(const struct wi_device *, struct wi_req *);
static int _wi_vendor(const struct wi_device *, char *, size_t);
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
  if ((result = _wi_vendor(device, stats->ws_vendor, WI_MAXSTRLEN)) != WI_OK)
    return(result);

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

const char *
wi_strerror(int error)
{
  switch (error) {
  case WI_NOCARRIER:
    return("No carrier signal");

  case WI_NOSUCHDEV:
    return("No such WaveLAN device");

  case WI_INVAL:
    return("Invalid parameter");

  default:
    return("Unknown error");
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

static int
_wi_getval(const struct wi_device *device, struct wi_req *wr)
{
  struct ifreq ifr;
  int sd;

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
    if (strcmp((char *)device,"ath")){	// For the Atheros, IDENTITY dus not work.
    }else {
      return(result);
    }
  }else if (wr.wi_len < 4)
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

  if (strcmp((char *)device,"ath") == 0) {	/* For the Atheros Cards */
    *quality = le16toh(wr.wi_val[1]);
  }else{
    *quality = le16toh(wr.wi_val[0]);
  }

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

#endif  /* !defined(__NetBSD__) && !defined(__FreeBSD__) */

