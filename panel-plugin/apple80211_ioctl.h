/*
 * This is an incomplete unofficial apple80211_ioctl.h, but enough for this project
 * See https://github.com/AppleIntelWiFi */

#ifndef _APPLE80211_IOCTL_H_
#define _APPLE80211_IOCTL_H_

#include <sys/types.h>
#include <net/if.h>

struct apple80211req
{
	char		if_name[IFNAMSIZ];
	uint32_t	type;
	uint32_t	result;
	uint64_t	len;
	void		*data;
};

#define SIOCSA80211 _IOW('i', 200, struct apple80211req)
#define SIOCGA80211 _IOWR('i', 201, struct apple80211req)

#define APPLE80211_IOC_SSID	1
#define APPLE80211_IOC_RATE	8
#define APPLE80211_IOC_RSSI	16

#define APPLE80211_MAX_RADIO	4
#define APPLE80211_MAX_SSID_LEN	32

struct apple80211_rssi_data
{
	uint32_t	version;
	uint32_t	num_radios;
	uint32_t	rssi_unit;
	int32_t		rssi[APPLE80211_MAX_RADIO];
	int32_t		aggregate_rssi;
	int32_t		rssi_ext[APPLE80211_MAX_RADIO];
	int32_t		aggregate_rssi_ext;
};

#endif
