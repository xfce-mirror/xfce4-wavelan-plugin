/* $Id: wi.h 545 2004-02-09 21:20:54Z benny $ */
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

#ifndef __WI_H__
#define __WI_H__

#define WI_MAXSTRLEN  (512)

struct wi_device;

struct wi_stats
{
  char  ws_netname[WI_MAXSTRLEN]; /* current SSID */
  int   ws_quality;               /* current signal quality (percent or dBm) */
  char  ws_qunit[4];              /* % or dBm ? */
  int   ws_rate;                  /* current rate (Mbps) */
  char  ws_vendor[WI_MAXSTRLEN];  /* device vendor name */
};

enum
{
  WI_OK         =  0,  /* everything ok */
  WI_NOCARRIER  = -1,  /* no carrier signal, some of the stats may be invalid */
  WI_NOSUCHDEV  = -2,  /* device is currently not attached */
  WI_INVAL      = -3,  /* invalid parameters given */
};

extern struct wi_device* wi_open(const char *);
extern void wi_close(struct wi_device *);
extern int wi_query(struct wi_device *, struct wi_stats *);
extern const char *wi_strerror(int);

#endif  /* !__WI_H__ */
