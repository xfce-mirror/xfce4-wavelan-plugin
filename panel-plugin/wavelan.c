/* $Id: wavelan.c,v 1.9 2004/08/03 16:46:39 benny Exp $ */
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include <libxfce4util/i18n.h>
#include <libxfcegui4/dialogs.h>
#include <panel/plugins.h>
#include <panel/xfce.h>

#include <wi.h>
#include "inline-icons.h"

enum
{
  STATE_ERROR = 0,
  STATE_LINK0 = 1,
  STATE_LINK1 = 2,
  STATE_LINK2 = 3,
  STATE_LINK3 = 4,
  STATE_LINK4 = 5,
  STATE_LINK5 = 6
};

typedef struct
{
  gchar *interface;
  struct wi_device *device;
  guint timer_id;

  guint state;

  gboolean autohide;

  int size;
  int orientation;

  GdkPixbuf *pb[7];

	GtkWidget	*ebox;
  GtkWidget *box;
	GtkWidget	*image;

  GtkTooltips *tooltips;
} t_wavelan;

static GdkPixbuf *
load_and_scale(const guint8 *data, int dstw, int dsth)
{
  GdkPixbuf *pb, *pb_scaled;
  int pb_w, pb_h;

  pb = gdk_pixbuf_new_from_inline(-1, data, FALSE, NULL);
  pb_w = gdk_pixbuf_get_width(pb);
  pb_h = gdk_pixbuf_get_height(pb);

  if (dstw == pb_w && dsth == pb_h)
    return(pb);
  else if (dstw < 0)
    dstw = (dsth * pb_w) / pb_h;
  else if (dsth < 0)
    dsth = (dstw * pb_h) / pb_w;

  pb_scaled = gdk_pixbuf_scale_simple(pb, dstw, dsth, GDK_INTERP_HYPER);
  g_object_unref(G_OBJECT(pb));

  return(pb_scaled);
}

static void
wavelan_load_pixbufs(t_wavelan *wavelan)
{
  int n, w, h;

  /*
   * free old pixbufs first
   */
  for (n = 0; n < 7; ++n) {
    if (wavelan->pb[n] != NULL)
      g_object_unref(G_OBJECT(wavelan->pb[n]));
  }

  /*
   * Determine dimension
   */
  if (wavelan->orientation == HORIZONTAL) {
    w = -1;
    h = icon_size[wavelan->size];
  }
  else {
    w = icon_size[wavelan->size];
    h = -1;
  }

  /*
   * Load and scale pixbufs
   */
  wavelan->pb[0] = load_and_scale(error_icon_data, w, h);
  wavelan->pb[1] = load_and_scale(link0_icon_data, w, h);
  wavelan->pb[2] = load_and_scale(link1_icon_data, w, h);
  wavelan->pb[3] = load_and_scale(link2_icon_data, w, h);
  wavelan->pb[4] = load_and_scale(link3_icon_data, w, h);
  wavelan->pb[5] = load_and_scale(link4_icon_data, w, h);
  wavelan->pb[6] = load_and_scale(link5_icon_data, w, h);
}

static void
wavelan_set_state(t_wavelan *wavelan, guint state)
{
  /* this is OK, happens if this function is called too early */
  if (wavelan->pb[0] == NULL)
    return;

  if (state > STATE_LINK5)
    state = STATE_LINK5;

  wavelan->state = state;
  gtk_image_set_from_pixbuf(GTK_IMAGE(wavelan->image), wavelan->pb[state]);

  if (wavelan->autohide && state == STATE_LINK0)
    gtk_widget_hide(wavelan->ebox);
  else
    gtk_widget_show(wavelan->ebox);
}

static gboolean
wavelan_timer(gpointer data)
{
  struct wi_stats stats;
  char *tip = NULL;
  t_wavelan *wavelan = (t_wavelan *)data;

  XFCE_PANEL_LOCK();

  if (wavelan->device != NULL) {
    int result;

    if ((result = wi_query(wavelan->device, &stats)) != WI_OK) {
      /* reset quality indicator */
      if (result == WI_NOCARRIER) {
        tip = g_strdup_printf(_("No carrier signal"));
        wavelan_set_state(wavelan, STATE_LINK0);
      }
      else {
        /* set error */
        tip = g_strdup_printf("%s", wi_strerror(result));
        wavelan_set_state(wavelan, STATE_ERROR);
      }
    }
    else {
      if (stats.ws_quality >= 95)
        wavelan_set_state(wavelan, STATE_LINK5);
      else if (stats.ws_quality >= 73)
        wavelan_set_state(wavelan, STATE_LINK4);
      else if (stats.ws_quality >= 49)
        wavelan_set_state(wavelan, STATE_LINK3);
      else if (stats.ws_quality >= 25)
        wavelan_set_state(wavelan, STATE_LINK2);
      else if (stats.ws_quality >= 1)
        wavelan_set_state(wavelan, STATE_LINK1);
      else
        wavelan_set_state(wavelan, STATE_LINK0);

      if (strlen(stats.ws_netname) > 0)
        tip = g_strdup_printf("%d%% (%s)", stats.ws_quality, stats.ws_netname);
      else
        tip = g_strdup_printf("%d%%", stats.ws_quality);
    }
  }
  else {
    tip = g_strdup(_("No device configured"));
    wavelan_set_state(wavelan, STATE_ERROR);
  }

  /* activate new tooltip */
  if (tip != NULL) {
    gtk_tooltips_set_tip(wavelan->tooltips, wavelan->ebox, tip, NULL);
    g_free(tip);
  }

  XFCE_PANEL_UNLOCK();

  /* keep the timeout running */
  return(TRUE);
}

static t_wavelan *
wavelan_new(void)
{
	t_wavelan *wavelan;

	wavelan = g_new0(t_wavelan, 1);

  wavelan->autohide = FALSE;
  wavelan->size = 1;
  wavelan->orientation = HORIZONTAL;

	wavelan->ebox = gtk_event_box_new();
	gtk_widget_show(wavelan->ebox);

  wavelan->box = gtk_hbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(wavelan->box), border_width);
	gtk_widget_show(wavelan->box);
	gtk_container_add(GTK_CONTAINER(wavelan->ebox), wavelan->box);

  wavelan->image = gtk_image_new();
	gtk_widget_show(wavelan->image);
	gtk_container_add(GTK_CONTAINER(wavelan->box), wavelan->image);

  /* create tooltips */
  wavelan->tooltips = gtk_tooltips_new();

	return(wavelan);
}

static void
wavelan_configure(t_wavelan *wavelan)
{
  if (wavelan->timer_id != 0) {
    gtk_timeout_remove(wavelan->timer_id);
    wavelan->timer_id = 0;
  }

  if (wavelan->device != NULL) {
    wi_close(wavelan->device);
    wavelan->device = NULL;
  }

  if (wavelan->interface != NULL) {
    /* open the WaveLAN device */
    if ((wavelan->device = wi_open(wavelan->interface)) != NULL) {
      /* register the update timer */
      wavelan->timer_id = gtk_timeout_add(250, wavelan_timer, wavelan);
    }
  }
}

static gboolean
wavelan_control_new(Control *ctrl)
{
	t_wavelan *wavelan;

	wavelan = wavelan_new();

	gtk_container_add(GTK_CONTAINER(ctrl->base), wavelan->ebox);

	ctrl->data = (gpointer)wavelan;
	ctrl->with_popup = FALSE;

	gtk_widget_set_size_request(ctrl->base, -1, -1);

	return(TRUE);
}

static void
wavelan_free(Control *ctrl)
{
	t_wavelan *wavelan;
  int n;

	g_return_if_fail(ctrl != NULL);
	g_return_if_fail(ctrl->data != NULL);

	wavelan = (t_wavelan *)ctrl->data;

  /* free tooltips */
  g_object_unref(G_OBJECT(wavelan->tooltips));

  /* free pixbufs */
  for (n = 0; n < 7; ++n)
    if (wavelan->pb[n] != NULL)
      g_object_unref(G_OBJECT(wavelan->pb[n]));

  /* unregister the timer */
  if (wavelan->timer_id != 0)
    gtk_timeout_remove(wavelan->timer_id);

  /* free the device info */
  if (wavelan->device != NULL)
    wi_close(wavelan->device);

  if (wavelan->interface != NULL)
    g_free(wavelan->interface);
	g_free(wavelan);
}

static void
wavelan_read_config(Control *ctrl, xmlNodePtr parent)
{
	t_wavelan *wavelan = (t_wavelan *)ctrl->data;
  xmlNodePtr node;
  xmlChar *value;

  if (parent == NULL || parent->children == NULL)
    return;

  for (node = parent->children; node != NULL; node = node->next) {
    if (!xmlStrEqual(node->name, (const xmlChar*)"WaveLAN"))
      continue;

    if ((value = xmlGetProp(node, (const xmlChar*)"Interface")) != NULL) {
      wavelan->interface = g_strdup((const char *)value);
      xmlFree(value);
    }
    if ((value = xmlGetProp(node, (const xmlChar*)"AutoHide")) != NULL) {
      wavelan->autohide = (strcmp((const char *)value, "true") == 0);
      xmlFree(value);
    }

    break;
  }

  wavelan_configure(wavelan);
}

static void
wavelan_write_config(Control *ctrl, xmlNodePtr parent)
{
	t_wavelan *wavelan = (t_wavelan *)ctrl->data;
  xmlNodePtr root;

  root = xmlNewTextChild(parent, NULL, "WaveLAN", NULL);

  if (wavelan->interface != NULL)
    xmlSetProp(root, "Interface", wavelan->interface);
  xmlSetProp(root, "AutoHide", wavelan->autohide ? "true" : "false");
}

static void
wavelan_attach_callback(Control *ctrl, const gchar *signal, GCallback cb,
		gpointer data)
{
	t_wavelan *wavelan = (t_wavelan *)ctrl->data;
	g_signal_connect(wavelan->ebox, signal, cb, data);
}

static void
wavelan_set_size(Control *ctrl, int size)
{
	t_wavelan *wavelan = (t_wavelan *)ctrl->data;

  wavelan->size = size;
  wavelan_load_pixbufs(wavelan);
  wavelan_set_state(wavelan, wavelan->state);
}

static void
wavelan_set_orientation(Control *ctrl, int orientation)
{
  t_wavelan *wavelan = (t_wavelan *)ctrl->data;
  GtkWidget *box;

  wavelan->orientation = orientation;
  wavelan_load_pixbufs(wavelan);
  wavelan_set_state(wavelan, wavelan->state);
}

/* interface changed callback */
static void
wavelan_interface_changed(GtkEntry *entry, t_wavelan *wavelan)
{
  if (wavelan->interface != NULL)
    g_free(wavelan->interface);
  wavelan->interface = g_strdup(gtk_entry_get_text(entry));
  wavelan_configure(wavelan);
}

/* autohide toggled callback */
static void
wavelan_autohide_changed(GtkToggleButton *button, t_wavelan *wavelan)
{
  wavelan->autohide = gtk_toggle_button_get_active(button);
  wavelan_set_state(wavelan, wavelan->state);
}

/* query installed devices */
static GList*
wavelan_query_interfaces (void)
{
  GList *interfaces = NULL;
  gchar  line[1024];
  FILE  *fp;
  gint   n;

  fp = popen ("/sbin/ifconfig -a", "r");
  if (fp != NULL)
    {
      while (fgets (line, 1024, fp) != NULL)
        {
          if (!isalpha (*line))
            continue;

          for (n = 0; isalnum (line[n]); ++n);
          line[n] = '\0';

          interfaces = g_list_append (interfaces, g_strdup (line));
        }

      pclose (fp);
    }

  return interfaces;
}

/* options dialog */
static void
wavelan_create_options (Control *ctrl, GtkContainer *con, GtkWidget *done)
{
  t_wavelan *wavelan = (t_wavelan *)ctrl->data;
  GtkWidget *hbox, *label, *interface, *vbox, *autohide;
  GtkWidget *combo;
  GList     *interfaces, *lp;

  vbox = gtk_vbox_new(FALSE, 2);
  gtk_widget_show(vbox);
  gtk_container_add(GTK_CONTAINER(con), vbox);

  hbox = gtk_hbox_new(FALSE, 2);
  gtk_widget_show(hbox);
  label = gtk_label_new(_("Interface"));
  gtk_widget_show(label);

  interfaces = wavelan_query_interfaces ();
  combo = gtk_combo_new ();
  gtk_combo_set_popdown_strings (GTK_COMBO (combo), interfaces);
  gtk_widget_show (combo);

  interface = GTK_COMBO (combo)->entry;
  gtk_entry_set_max_length(GTK_ENTRY(interface), 10);
  if (wavelan->interface != NULL)
    gtk_entry_set_text(GTK_ENTRY(interface), wavelan->interface);
  g_signal_connect(interface, "changed", G_CALLBACK(wavelan_interface_changed),
      wavelan);
  gtk_widget_show(interface);

  gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, FALSE, 1);
  gtk_box_pack_start(GTK_BOX(hbox), combo, TRUE, FALSE, 1);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 1);

  hbox = gtk_hbox_new(FALSE, 2);
  gtk_widget_show(hbox);
  autohide = gtk_check_button_new_with_mnemonic(_("_Autohide when offline"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(autohide), wavelan->autohide);
  g_signal_connect(autohide, "toggled", G_CALLBACK(wavelan_autohide_changed),
      wavelan);
  gtk_widget_show(autohide);
  gtk_box_pack_start(GTK_BOX(hbox), autohide, TRUE, TRUE, 1);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 1);

  for (lp = interfaces; lp != NULL; lp = lp ->next)
    g_free (lp->data);
  g_list_free (interfaces);
}

/* initialization */
G_MODULE_EXPORT void
xfce_control_class_init(ControlClass *cc)
{
	/* these are required */
	cc->name		        = "wavelan";
	cc->caption		      = _("WaveLAN plugin");
	cc->create_control	= (CreateControlFunc)wavelan_control_new;
	cc->free		        = wavelan_free; 
	cc->attach_callback	= wavelan_attach_callback;

	/* options; don't define if you don't have any ;) */
	cc->read_config		  = wavelan_read_config;
	cc->write_config	  = wavelan_write_config;
	cc->create_options	= wavelan_create_options;

	/* Don't use this function at all if you want xfce to
	 * do the sizing.
	 * Just define the set_size function to NULL, or rather, don't 
	 * set it to something else.
	 */
	cc->set_size		    = wavelan_set_size;
	cc->set_orientation	= wavelan_set_orientation;

	/* unused in the wavelan:
	 * ->set_theme
	 */
	 
}

/* required! defined in panel/plugins.h */
XFCE_PLUGIN_CHECK_INIT
