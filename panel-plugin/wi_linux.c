/* $Id: wi_linux.c,v 1.1 2004/02/09 21:20:54 benny Exp $ */
/*-
 * Copyright (c) 2003,2004 Benedikt Meurer <benny@xfce.org>
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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <wi.h>

struct wi_device
{
  char interface[WI_MAXSTRLEN];
  FILE *fp;
};

struct wi_device *
wi_open(const char *interface)
{
  struct wi_device *device;
  FILE *fp;

  g_return_val_if_fail(interface != NULL, NULL);

  if ((fp = fopen("/proc/net/wireless", "rt")) == NULL)
    return(NULL);

  device = g_new0(struct wi_device, 1);
  g_strlcpy(device->interface, interface, WI_MAXSTRLEN);

  return(device);
}

void
wi_close(struct wi_device *device)
{
  g_return_if_fail(device != NULL);

  fclose(device->fp);
  g_free(device);
}

int
wi_query(struct wi_device *device, struct wi_stats *stats)
{
  char buffer[1024];
  char *bp;
  double link;
  long level;

  g_return_val_if_fail(device != NULL, WI_INVAL);
  g_return_val_if_fail(stats != NULL, WI_INVAL);

  /* FIXME */
  g_strlcpy(stats->ws_netname, "", WI_MAXSTRLEN);
  stats->ws_rate = 0;
  g_strlcpy(stats->ws_vendor, "Unknown", WI_MAXSTRLEN);

  /* rewind the /proc/net/wireless listing */
  rewind(device->fp);

  for (;;) {
    if (fgets(buffer, sizeof(buffer), device->fp) == NULL)
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

  /* check if we have a carrier signal */
  if (level < 0)
    return(WI_NOCARRIER);

  /* calculate link quality */
  if (link <= 0)
    stats->quality = 0;
  else {
    /* thanks to google for this hint */
    stats->quality = (int)rint(log(link) / log(92.0) * 100.0);
  }
 
  return(WI_OK);
}

#endif  /* !defined(__linux__) */
