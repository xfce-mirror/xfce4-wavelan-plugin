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

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4panel/xfce-panel-convenience.h>
#include <libxfcegui4/libxfcegui4.h>
#include <libxfce4panel/xfce-panel-plugin.h>
#include <libxfce4panel/xfce-hvbox.h>

#include <wi.h>
#include "inline-icons.h"

#include <string.h>
#include <ctype.h>

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
  gboolean autohide_missing;
  gboolean square_icon;

  int size;
  GtkOrientation orientation;
  
  GdkPixbuf *pb[7];

  GtkWidget *box;
  GtkWidget *ebox;
	GtkWidget	*image;
  GtkWidget *button;

  GtkTooltips *tooltips;

  XfcePanelPlugin *plugin;
  
} t_wavelan;

static GdkPixbuf *
load_and_scale(const guint8 *data, int dstw, int dsth)
{
  GdkPixbuf *pb, *pb_scaled;
  int pb_w, pb_h;

  TRACE ("Entered load_and_scale");
  
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
  int n, w, h, second_dim;

  TRACE ("Entered wavelan_load_pixbufs, size = %d", wavelan->size);

  /*
   * free old pixbufs first
   */
  for (n = 0; n < 7; ++n) {
    if (wavelan->pb[n] != NULL)
      g_object_unref(G_OBJECT(wavelan->pb[n]));
  }

  /*
   * Make it square if desired.
   */
  if (wavelan->square_icon) {
    second_dim = wavelan->size;
  }
  else {
    second_dim = -1;
  }

  /*
   * Determine dimension
   */
  if (wavelan->orientation == GTK_ORIENTATION_HORIZONTAL) {
    w = second_dim;
    h = wavelan->size;
  }
  else {
    w = wavelan->size;
    h = second_dim;
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

  TRACE ("Entered wavelan_set_state, state = %u", state);
  
  if (state > STATE_LINK5)
    state = STATE_LINK5;

  wavelan->state = state;
  gtk_image_set_from_pixbuf(GTK_IMAGE(wavelan->image), wavelan->pb[state]);

  if (wavelan->autohide && state == STATE_LINK0)
    gtk_widget_hide(wavelan->ebox);
  else if (wavelan->autohide_missing && state == STATE_ERROR)
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

  TRACE ("Entered wavelan_timer");
  
  if (wavelan->device != NULL) {
    int result;

    if ((result = wi_query(wavelan->device, &stats)) != WI_OK) {
      TRACE ("result = %d", result);
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
        tip = g_strdup_printf("%s: %d%% at %dMb/s", stats.ws_netname, stats.ws_quality, stats.ws_rate / 1000000);
      else
        tip = g_strdup_printf("%d%% at %dMb/s", stats.ws_quality, stats.ws_rate / 1000000);
    }
  }
  else {
    tip = g_strdup(_("No device configured"));
    wavelan_set_state(wavelan, STATE_ERROR);
  }

  /* activate new tooltip */
  if (tip != NULL) {
    gtk_tooltips_set_tip(wavelan->tooltips, GTK_WIDGET (wavelan->plugin), 
                         tip, NULL);
    g_free(tip);
  }

  /* keep the timeout running */
  return(TRUE);
}

static void
wavelan_reset(t_wavelan *wavelan)
{
  TRACE ("Entered wavelan_reset");
  
  if (wavelan->timer_id != 0) {
    g_source_remove(wavelan->timer_id);
    wavelan->timer_id = 0;
  }

  if (wavelan->device != NULL) {
    wi_close(wavelan->device);
    wavelan->device = NULL;
  }
  TRACE ("Using interface %s", wavelan->interface);
  if (wavelan->interface != NULL) {
    /* open the WaveLAN device */
    if ((wavelan->device = wi_open(wavelan->interface)) != NULL) {
      /* register the update timer */
      TRACE ("Opened device");
      wavelan->timer_id = g_timeout_add(250, wavelan_timer, wavelan);
    }
  }
}

/* query installed devices */
static GList*
wavelan_query_interfaces (void)
{
  GList *interfaces = NULL;
  gchar  line[1024];
  FILE  *fp;
  gint   n;

  TRACE ("Entered wavelan_query_interface");
  
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

static void
wavelan_read_config(XfcePanelPlugin *plugin, t_wavelan *wavelan)
{
  char *file;
  XfceRc *rc;
  const char *s;
  
  TRACE ("Entered wavelan_read_config");
  
  if ((file = xfce_panel_plugin_lookup_rc_file (plugin)) != NULL)
  {
    rc = xfce_rc_simple_open (file, TRUE);
    g_free (file);

    if (rc != NULL)
    {
      if ((s = xfce_rc_read_entry (rc, "Interface", NULL)) != NULL) 
      {
        wavelan->interface = g_strdup (s);
      } 
      
      wavelan->autohide = xfce_rc_read_bool_entry (rc, "Autohide", FALSE);
      wavelan->autohide_missing = xfce_rc_read_bool_entry(rc, "AutohideMissing", FALSE);
      wavelan->square_icon = xfce_rc_read_bool_entry(rc, "SquareIcon", FALSE);
    }
  }

  if (wavelan->interface == NULL) {
    GList *interfaces = wavelan_query_interfaces();
    wavelan->interface = g_list_first(interfaces)->data;
    g_list_free(interfaces);
  }
  
  wavelan_reset(wavelan);
}

static t_wavelan *
wavelan_new(XfcePanelPlugin *plugin)
{
	t_wavelan *wavelan;
  XfceScreenPosition screen_position;
  
  TRACE ("Entered wavelan_new");
  
	wavelan = g_new0(t_wavelan, 1);

  wavelan->autohide = FALSE;
  wavelan->autohide_missing = FALSE;

  wavelan->square_icon = FALSE;

  wavelan->plugin = plugin;
  
  wavelan->size = xfce_panel_plugin_get_size (plugin);
  screen_position = xfce_panel_plugin_get_screen_position (plugin);
  wavelan->orientation = xfce_panel_plugin_get_orientation (plugin);
 
  wavelan->ebox = gtk_event_box_new();
  gtk_container_add (GTK_CONTAINER (plugin), wavelan->ebox);
  
  if (xfce_screen_position_is_horizontal (screen_position))
      wavelan->box = xfce_hvbox_new (GTK_ORIENTATION_HORIZONTAL, FALSE, 0);
  else
      wavelan->box = xfce_hvbox_new (GTK_ORIENTATION_VERTICAL, FALSE, 0);
  
  gtk_widget_show(wavelan->box);
  gtk_container_add (GTK_CONTAINER (wavelan->ebox), wavelan->box);

  /*wavelan->button = xfce_create_panel_button ();
  gtk_widget_show (wavelan->button);
	gtk_box_pack_start(GTK_BOX (wavelan->box), wavelan->button, TRUE, TRUE, 0);*/
  gtk_widget_set_size_request (wavelan->box, -1, -1);
  
  xfce_panel_plugin_add_action_widget (plugin, wavelan->box);

  
  wavelan->image = gtk_image_new();
	gtk_widget_show(wavelan->image);
  gtk_container_add (GTK_CONTAINER (wavelan->box), wavelan->image);

  /* create tooltips */
  wavelan->tooltips = gtk_tooltips_new();
  g_object_ref (wavelan->tooltips);
  gtk_object_sink (GTK_OBJECT (wavelan->tooltips));

  wavelan_load_pixbufs(wavelan);
  
  wavelan_read_config(plugin, wavelan);

  wavelan_set_state(wavelan, wavelan->state);

  gtk_widget_show_all(wavelan->ebox);
  
  return(wavelan);
}

static void
wavelan_free(t_wavelan *wavelan)
{
  int n;

  TRACE ("Entered wavelan_free");
  
  /* free tooltips */
  g_object_unref(G_OBJECT(wavelan->tooltips));

  /* free pixbufs */
  for (n = 0; n < 7; ++n)
    if (wavelan->pb[n] != NULL)
      g_object_unref(G_OBJECT(wavelan->pb[n]));

  /* unregister the timer */
  if (wavelan->timer_id != 0)
    g_source_remove(wavelan->timer_id);

  /* free the device info */
  if (wavelan->device != NULL)
    wi_close(wavelan->device);

  if (wavelan->interface != NULL)
    g_free(wavelan->interface);
	g_free(wavelan);
}

static void
wavelan_write_config(XfcePanelPlugin *plugin, t_wavelan *wavelan)
{
  char *file;
  XfceRc *rc;
  
  TRACE ("Entered wavelan_write_config");
  
  if (!(file = xfce_panel_plugin_save_location (plugin, TRUE)))
  {
    return;
  }
   
  rc = xfce_rc_simple_open (file, FALSE);
  
  g_free (file);

  if (!rc)
    return;

  if (wavelan->interface)
  {
    xfce_rc_write_entry (rc, "Interface", wavelan->interface);
  }
  xfce_rc_write_bool_entry (rc, "Autohide", wavelan->autohide);
  xfce_rc_write_bool_entry (rc, "AutohideMissing", wavelan->autohide_missing);
  xfce_rc_write_bool_entry (rc, "SquareIcon", wavelan->square_icon);

  xfce_rc_close(rc);
  
}

static void
wavelan_set_orientation(t_wavelan *wavelan, GtkOrientation orientation)
{
  wavelan->orientation = orientation;
  xfce_hvbox_set_orientation (XFCE_HVBOX (wavelan->box), orientation);
}

static void
wavelan_set_size(t_wavelan *wavelan, int size)
{
  wavelan->size = size;
  wavelan_load_pixbufs(wavelan);
  gtk_widget_set_size_request (wavelan->box, -1, -1);
}

static void
wavelan_set_square_icon(t_wavelan *wavelan, gboolean square_icon)
{
  wavelan->square_icon = square_icon;
  wavelan_load_pixbufs(wavelan);
  gtk_widget_set_size_request (wavelan->box, -1, -1);
}

/* interface changed callback */
static void
wavelan_interface_changed(GtkEntry *entry, t_wavelan *wavelan)
{
    if (wavelan->interface != NULL)
          g_free(wavelan->interface);
    wavelan->interface = g_strdup(gtk_entry_get_text(entry));
    wavelan_reset(wavelan);
}

/* autohide toggled callback */
static void
wavelan_autohide_changed(GtkToggleButton *button, t_wavelan *wavelan)
{
  TRACE ("Entered wavelan_autohide_changed");
  wavelan->autohide = gtk_toggle_button_get_active(button);
  wavelan_set_state(wavelan, wavelan->state);
}

/* autohide on missing callback */
static void 
wavelan_autohide_missing_changed(GtkToggleButton *button, t_wavelan *wavelan)
{
  TRACE ("Entered wavelan_autohide_missing_changed");
  wavelan->autohide_missing = gtk_toggle_button_get_active(button);
  wavelan_set_state(wavelan, wavelan->state);
}

/* square icon callback */
static void 
wavelan_square_icon_changed(GtkToggleButton *button, t_wavelan *wavelan)
{
  TRACE ("Entered wavelan_square_icon_changed");
  wavelan_set_square_icon(wavelan, gtk_toggle_button_get_active(button));
}

static void
wavelan_dialog_response (GtkWidget *dlg, int response, t_wavelan *wavelan)
{
    g_object_set_data (G_OBJECT (wavelan->plugin), "dialog", NULL);

    gtk_widget_destroy (dlg);
/*    xfce_panel_plugin_unblock_menu (wavelan->plugin); */
    wavelan_write_config (wavelan->plugin, wavelan);
}

/* options dialog */
static void
wavelan_create_options (XfcePanelPlugin *plugin, t_wavelan *wavelan)
{
  GtkWidget *dlg, *hbox, *label, *interface, *vbox, *autohide;
  GtkWidget *autohide_missing, *header, *warn_label, *square_icon;
  GtkWidget *combo;
  GList     *interfaces, *lp;

  TRACE ("Entered wavelan_create_options");
  
  dlg = gtk_dialog_new_with_buttons (_("Properties"),
              GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (plugin))),
              GTK_DIALOG_DESTROY_WITH_PARENT | 
              GTK_DIALOG_NO_SEPARATOR,
              GTK_STOCK_CLOSE, GTK_RESPONSE_OK,
              NULL);

  g_object_set_data (G_OBJECT (plugin), "dialog", dlg);

  gtk_window_set_position (GTK_WINDOW (dlg), GTK_WIN_POS_CENTER);

  g_signal_connect (dlg, "response", G_CALLBACK (wavelan_dialog_response),
                    wavelan);
  
  gtk_container_set_border_width (GTK_CONTAINER (dlg), 2);

  header = xfce_create_header (NULL, _("Wavelan Plugin Options"));
  gtk_widget_set_size_request (GTK_BIN (header)->child, 200, 32);
  gtk_container_set_border_width (GTK_CONTAINER (header), 6);
  gtk_widget_show (header);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox), header,
                      FALSE, TRUE, 0);
              
  vbox = gtk_vbox_new(FALSE, 8);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
  gtk_widget_show(vbox);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox), vbox,
                      TRUE, TRUE, 0);
  
  hbox = gtk_hbox_new(FALSE, 8);
  gtk_widget_show(hbox);
  
  label = gtk_label_new(_("Interface"));
  gtk_widget_show(label);

  interfaces = wavelan_query_interfaces ();
  combo = gtk_combo_new ();
  gtk_combo_set_popdown_strings (GTK_COMBO (combo), interfaces);
  gtk_widget_show (combo);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 1);

  interface = GTK_COMBO (combo)->entry;
  gtk_entry_set_max_length(GTK_ENTRY(interface), 10);
  if (wavelan->interface != NULL)
    gtk_entry_set_text(GTK_ENTRY(interface), wavelan->interface);
  g_signal_connect(interface, "changed", G_CALLBACK(wavelan_interface_changed),
      wavelan);
  gtk_widget_show(interface);

  gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, FALSE, 1);
  gtk_box_pack_start(GTK_BOX(hbox), combo, TRUE, FALSE, 1);

  hbox = gtk_hbox_new(FALSE, 2);
  gtk_widget_show(hbox);
  autohide = gtk_check_button_new_with_mnemonic(_("_Autohide when offline"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(autohide), wavelan->autohide);
  g_signal_connect(autohide, "toggled", G_CALLBACK(wavelan_autohide_changed),
      wavelan);
  gtk_widget_show(autohide);
  gtk_box_pack_start(GTK_BOX(hbox), autohide, TRUE, TRUE, 1);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 1);
  
  hbox = gtk_hbox_new(FALSE, 2);
  gtk_widget_show(hbox);
  autohide_missing = gtk_check_button_new_with_label(_("Autohide when no hardware present"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(autohide_missing), 
      wavelan->autohide_missing);
  g_signal_connect(autohide_missing, "toggled", 
      G_CALLBACK(wavelan_autohide_missing_changed), wavelan);
  gtk_widget_show(autohide_missing);
  gtk_box_pack_start(GTK_BOX(hbox), autohide_missing, TRUE, TRUE, 1);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 1);

  hbox = gtk_hbox_new(FALSE, 2);
  gtk_widget_show(hbox);
  warn_label = gtk_label_new(_("Note: This will make it difficult to remove or configure the plugin if there is no device detected."));
  gtk_label_set_line_wrap(GTK_LABEL(warn_label), TRUE);
  gtk_widget_show(warn_label);
  gtk_box_pack_start(GTK_BOX(hbox), warn_label, TRUE, TRUE, 1);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 1);

  hbox = gtk_hbox_new(FALSE, 2);
  gtk_widget_show(hbox);
  square_icon = gtk_check_button_new_with_label(_("Use a square icon"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(square_icon), 
      wavelan->square_icon);
  g_signal_connect(square_icon, "toggled", 
      G_CALLBACK(wavelan_square_icon_changed), wavelan);
  gtk_widget_show(square_icon);
  gtk_box_pack_start(GTK_BOX(hbox), square_icon, TRUE, TRUE, 1);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 1);

  for (lp = interfaces; lp != NULL; lp = lp ->next)
    g_free (lp->data);
  g_list_free (interfaces);

  gtk_widget_show (dlg);
  
}

static void
wavelan_set_screen_position (t_wavelan *wavelan, 
                             XfceScreenPosition position)
{

  /* Do I really need anything here? */
  
}

/*static void
wavelan_screen_position_changed (XfcePanelPlugin *plugin,
                                 XfceScreenPosition position,
                                 t_wavelan *wavelan)
{
    wavelan_set_screen_postion (wavelan, position); 
}*/

static void
wavelan_orientation_changed (XfcePanelPlugin *plugin,
                             GtkOrientation orientation,
                             t_wavelan *wavelan)
{
    wavelan_set_orientation(wavelan, orientation);
}
  

static void
wavelan_size_changed(XfcePanelPlugin *plugin,
                     int size,
                     t_wavelan *wavelan)
{
    wavelan_set_size(wavelan, size);
}

static void 
wavelan_free_data (XfcePanelPlugin *plugin, t_wavelan *wavelan)
{
  wavelan_free(wavelan);
}

void
wavelan_save (XfcePanelPlugin *plugin, t_wavelan *wavelan)
{
  wavelan_write_config(plugin, wavelan);
}

static void
wavelan_configure (XfcePanelPlugin *plugin, t_wavelan *wavelan)
{
    wavelan_create_options(plugin, wavelan);
}

static void
wavelan_construct (XfcePanelPlugin *plugin)
{
  
  TRACE ("Entered wavelan_construct");
  
  t_wavelan *wavelan = wavelan_new(plugin);

/*  g_signal_connect (plugin, "screen-position-changed",
                    G_CALLBACK (wavelan_screen_position_changed), wavelan);
*/
  g_signal_connect (plugin, "orientation-changed",
                    G_CALLBACK (wavelan_orientation_changed), wavelan);

  g_signal_connect (plugin, "size-changed",
                    G_CALLBACK (wavelan_size_changed), wavelan);
 
  g_signal_connect (plugin, "free-data",
                    G_CALLBACK (wavelan_free_data), wavelan);

  g_signal_connect (plugin, "save",
                    G_CALLBACK (wavelan_save), wavelan);
  
  xfce_panel_plugin_menu_show_configure (plugin);
  g_signal_connect (plugin, "configure-plugin",
                    G_CALLBACK (wavelan_configure), wavelan);
  
  wavelan_set_screen_position (wavelan,
          xfce_panel_plugin_get_screen_position (plugin));
  
}

XFCE_PANEL_PLUGIN_REGISTER_EXTERNAL(wavelan_construct);

