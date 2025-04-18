/* Copyright (c) 2024 Torrekie Gen <me@torrekie.dev>
 *                    (Introduce Darwin support)
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

#include <sys/param.h>

#ifdef __APPLE__

#include <libxfce4util/libxfce4util.h>

#include <sys/cdefs.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <net/if.h>
#include <net/if_media.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if __has_include(<Kernel/IOKit/apple80211/apple80211_ioctl.h>)
#include <Kernel/IOKit/apple80211/apple80211_ioctl.h>
#elif __has_include(<IOKit/apple80211/apple80211_ioctl.h>)
#include <IOKit/apple80211/apple80211_ioctl.h>
#else
#include "apple80211_ioctl.h"
#endif

#include <wi.h>

/* On non-macOS Darwin, signing with ent 'com.apple.wlan.authentication'
 * is required for APPLE80211 */

struct wi_device {
  char interface[WI_MAXSTRLEN];
  int socket;
};

static int _wi_carrier(const struct wi_device*);
static int _wi_getval(const struct wi_device*, struct apple80211req*);

static int _wi_netname(const struct wi_device*, char*, size_t);
static int _wi_quality(const struct wi_device*, int*);
static int _wi_rate(const struct wi_device*, int*);

struct wi_device* wi_open(const char* interface) {
  struct wi_device* device = NULL;

  if (interface != NULL) {
    if ((device = (struct wi_device*)calloc(1, sizeof(*device))) != NULL) {
      strlcpy(device->interface, interface, WI_MAXSTRLEN);

      if ((device->socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        free(device);
        device = NULL;
      }
    }
  }

  return (device);
}

void wi_close(struct wi_device* device) {
  if (device != NULL) {
    close(device->socket);
    free(device);
  }
}

int wi_query(struct wi_device* device, struct wi_stats* stats) {
  int result;

  if (device == NULL || stats == NULL)
    return (WI_INVAL);

  /* clear stats first */
  bzero((void*)stats, sizeof(*stats));

  strlcpy(stats->ws_qunit, "%", 2);

  /* I believe we are able to get vendor by fetching IEEE OUI, but it is more
   * likely accquiring native hardware informations */
  strlcpy(stats->ws_vendor, _("Unknown"), WI_MAXSTRLEN);

  /* check carrier */
  if ((result = _wi_carrier(device)) != WI_OK)
    return (result);
  else {
    /* check netname (depends on carrier state) */
    if ((result = _wi_netname(device, stats->ws_netname, WI_MAXSTRLEN)) !=
        WI_OK)
      return (result);

    /* check quality (depends on carrier state) */
    if ((result = _wi_quality(device, &stats->ws_quality)) != WI_OK)
      return (result);

    /* check rate (depends on carrier state) */
    if ((result = _wi_rate(device, &stats->ws_rate)) != WI_OK)
      return (result);

    /* everything ok, stats are up-to-date */
    return (WI_OK);
  }
}

static int _wi_carrier(const struct wi_device* device) {
  struct ifmediareq ifmr;

  bzero((void*)&ifmr, sizeof(ifmr));
  strlcpy(ifmr.ifm_name, device->interface, sizeof(ifmr.ifm_name));

  if (ioctl(device->socket, SIOCGIFMEDIA, &ifmr) < 0) {
    /*
     * Interface does not support SIOCGIFMEDIA ioctl,
     * assume no such device.
     */

    return (WI_NOSUCHDEV);
  }

  if ((ifmr.ifm_status & IFM_AVALID) == 0) {
    /*
     * Interface doesn't report media-valid status.
     * assume no such device.
     */

    return (WI_NOSUCHDEV);
  }

  /* otherwise, return ok for active, not-ok if not active. */
  return ((ifmr.ifm_status & IFM_ACTIVE) != 0 ? WI_OK : WI_NOCARRIER);
}

static int _wi_getval(const struct wi_device* device,
                      struct apple80211req* areq) {
  strlcpy(areq->if_name, device->interface, IFNAMSIZ);

  if (ioctl(device->socket, SIOCGA80211, areq) < 0)
    return (WI_NOSUCHDEV);

  return (WI_OK);
}

static int _wi_netname(const struct wi_device* device,
                       char* buffer,
                       size_t len) {
  struct apple80211req areq;
  int result;

  bzero((void*)&areq, sizeof(areq));
  areq.type = APPLE80211_IOC_SSID;
  areq.len = APPLE80211_MAX_SSID_LEN;
  areq.data = buffer;

  if ((result = _wi_getval(device, &areq)) != WI_OK)
    return (result);

  return (WI_OK);
}

static int _wi_quality(const struct wi_device* device, int* quality) {
  struct apple80211req areq;
  struct apple80211_rssi_data rssi;
  int result;

  bzero((void*)&areq, sizeof(areq));
  bzero((void*)&rssi, sizeof(rssi));

  areq.type = APPLE80211_IOC_RSSI;
  areq.len = sizeof(rssi);
  areq.data = &rssi;

  if ((result = _wi_getval(device, &areq)) != WI_OK)
    return (result);

  /* areq.rssi gives dBm value, OpenBSD nr.nr_rssi gives dBm too.
   * But OpenBSD impl returns %, so use same method to convert dBm to %
   */

  /* There may have multiple radios, but Darwin already calculated
   * aggregate rssi for us, use this one instead */

  if (rssi.aggregate_rssi <= -100)
    *quality = 0;
  else if (rssi.aggregate_rssi >= -50)
    *quality = 100;
  else
    *quality = 2 * (100 + rssi.aggregate_rssi);

  return (WI_OK);
}

static int _wi_rate(const struct wi_device* device, int* rate) {
  struct apple80211req areq;
  int result;

  bzero((void*)&areq, sizeof(areq));

  areq.type = APPLE80211_IOC_RATE;

  if ((result = _wi_getval(device, &areq)) != WI_OK)
    return (result);

  /* APPLE80211_IOC_RATE directly returns Mbps */
  *rate = areq.result;

  return (WI_OK);
}

#endif
