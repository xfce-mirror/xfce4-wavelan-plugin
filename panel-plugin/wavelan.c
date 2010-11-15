/* Copyright (c) 2003,2004 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2010 Florian Rivoal <frivoal@xfce.org>
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

#include "wi.h"

#include <string.h>
#include <ctype.h>

#define BORDER 8
typedef struct
{
  gchar *interface;
  struct wi_device *device;
  guint timer_id;

  guint state;

  gboolean autohide;
  gboolean autohide_missing;
  gboolean signal_colors;

  int size;
  GtkOrientation orientation;

  GtkWidget *box;
  GtkWidget *ebox;
  GtkWidget *image;
  GtkWidget *signal;
  GtkWidget *tooltip_text;

  XfcePanelPlugin *plugin;
  
} t_wavelan;

static void
wavelan_set_state(t_wavelan *wavelan, gint state)
{  
  /* state = 0 -> no link, =-1 -> error */
  TRACE ("Entered wavelan_set_state, state = %u", state);

  GtkRcStyle *rc = NULL;
  GdkColor color;

  gchar signal_color_bad[] = "#e00000";
  gchar signal_color_weak[] = "#e05200";
  gchar signal_color_good[] = "#e6ff00";
  gchar signal_color_strong[] = "#06c500";
  
  if(state > 100)
    state = 100;

  wavelan->state = state;

  if (state >= 1) {
   gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(wavelan->signal), (gdouble) state / 100);

   if (wavelan->signal_colors) {
    /* set color */
    rc = gtk_widget_get_modifier_style(GTK_WIDGET(wavelan->signal));
    if (rc) {
     rc->color_flags[GTK_STATE_PRELIGHT] |= GTK_RC_BG;
     rc->color_flags[GTK_STATE_SELECTED] |= GTK_RC_BASE;
     if (state > 70)
      gdk_color_parse(signal_color_strong, &color);
     else if (state > 55)
      gdk_color_parse(signal_color_good, &color);
     else if (state > 40)
      gdk_color_parse(signal_color_weak, &color);
     else
      gdk_color_parse(signal_color_bad, &color);
     rc->bg[GTK_STATE_PRELIGHT] = color;
     rc->base[GTK_STATE_SELECTED] = color;
     gtk_widget_modify_style(GTK_WIDGET(wavelan->signal), rc);
     }
    }
   else {
    rc = gtk_rc_style_new();
    gtk_widget_modify_style(GTK_WIDGET(wavelan->signal), rc);
    g_object_unref(rc);
    }

   }
  else
   gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(wavelan->signal), 0.0);

  /* hide if no network & autohide or if no card found */
  if (wavelan->autohide && state == 0)
    gtk_widget_hide(wavelan->ebox);
  else if (wavelan->autohide_missing && state == -1)
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
        tip = g_strdup(_("No carrier signal"));
        wavelan_set_state(wavelan, 0);
      }
      else {
        /* set error */
        tip = g_strdup(wi_strerror(result));
        wavelan_set_state(wavelan, -1);
      }
    }
    else {
      wavelan_set_state(wavelan, stats.ws_quality);

      if (strlen(stats.ws_netname) > 0)
        tip = g_strdup_printf("%s: %d%s at %dMb/s", stats.ws_netname, stats.ws_quality, stats.ws_qunit, stats.ws_rate);
      else
        tip = g_strdup_printf("%d%s at %dMb/s", stats.ws_quality, stats.ws_qunit, stats.ws_rate);
    }
  }
  else {
    tip = g_strdup(_("No device configured"));
    wavelan_set_state(wavelan, -1);
  }

  /* set new tooltip */
  if (tip != NULL) {
    gtk_label_set_text(GTK_LABEL(wavelan->tooltip_text), tip);
    g_free(tip);
  }

  /* keep the timeout running */
  return(TRUE);
}

inline guint
timeout_add_seconds(guint interval, GSourceFunc function, gpointer data)
{
#if GLIB_CHECK_VERSION( 2,14,0 )
  return g_timeout_add_seconds(interval, function, data);
#else
  return g_timeout_add(interval*1000, function, data);
#endif
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
      wavelan->timer_id = timeout_add_seconds(1, wavelan_timer, wavelan);
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
      wavelan->signal_colors = xfce_rc_read_bool_entry(rc, "SignalColors", FALSE);
    }
  }

  if (wavelan->interface == NULL) {
    GList *interfaces = wavelan_query_interfaces();
    wavelan->interface = g_list_first(interfaces)->data;
    g_list_free(interfaces);
  }
  
  wavelan_reset(wavelan);
}

static gboolean tooltip_cb( GtkWidget *widget, gint x, gint y, gboolean keyboard, GtkTooltip * tooltip, t_wavelan *wavelan)
{
	gtk_tooltip_set_custom( tooltip, wavelan->tooltip_text );
	return TRUE;
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

  wavelan->signal_colors = TRUE;

  wavelan->plugin = plugin;
  
  wavelan->size = xfce_panel_plugin_get_size (plugin);
  screen_position = xfce_panel_plugin_get_screen_position (plugin);
  wavelan->orientation = xfce_panel_plugin_get_orientation (plugin);
 
  wavelan->ebox = gtk_event_box_new();
  gtk_widget_set_has_tooltip(wavelan->ebox, TRUE);
  g_signal_connect(wavelan->ebox, "query-tooltip", G_CALLBACK(tooltip_cb), wavelan);
  xfce_panel_plugin_add_action_widget(plugin, wavelan->ebox);
  gtk_container_add(GTK_CONTAINER(plugin), wavelan->ebox);

  wavelan->tooltip_text = gtk_label_new(NULL);
  g_object_ref( wavelan->tooltip_text );

  /* create box for img & progress bar */
  if (wavelan->orientation == GTK_ORIENTATION_HORIZONTAL)
    wavelan->box = xfce_hvbox_new(GTK_ORIENTATION_HORIZONTAL, FALSE, 0);
  else
    wavelan->box = xfce_hvbox_new(GTK_ORIENTATION_VERTICAL, FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(wavelan->box), BORDER / 2);

  /* setup progressbar */
  wavelan->signal = gtk_progress_bar_new();
  if (wavelan->orientation == GTK_ORIENTATION_HORIZONTAL)
  {
    gtk_progress_bar_set_orientation(GTK_PROGRESS_BAR(wavelan->signal), GTK_PROGRESS_BOTTOM_TO_TOP);
    gtk_widget_set_size_request(wavelan->signal, 8, -1);
  } else {
    gtk_progress_bar_set_orientation(GTK_PROGRESS_BAR(wavelan->signal), GTK_PROGRESS_LEFT_TO_RIGHT);
    gtk_widget_set_size_request(wavelan->signal, -1, 8);
  }

  wavelan->image = gtk_image_new();
  gtk_image_set_from_pixbuf(GTK_IMAGE(wavelan->image), gtk_icon_theme_load_icon(gtk_icon_theme_get_default(), "network-wireless", wavelan->size-6, 0, NULL));

  gtk_box_pack_start(GTK_BOX(wavelan->box), GTK_WIDGET(wavelan->image), FALSE, FALSE, 2);
  gtk_box_pack_start(GTK_BOX(wavelan->box), GTK_WIDGET(wavelan->signal), FALSE, FALSE, 2);

  gtk_widget_show_all(wavelan->box);
  gtk_container_add(GTK_CONTAINER(wavelan->ebox), GTK_WIDGET(wavelan->box));
  gtk_widget_show_all(wavelan->ebox);
  if (wavelan->orientation == GTK_ORIENTATION_HORIZONTAL) 
    gtk_widget_set_size_request(wavelan->ebox, -1, wavelan->size);
  else
    gtk_widget_set_size_request(wavelan->ebox, wavelan->size, -1);
  
  wavelan_read_config(plugin, wavelan);

  wavelan_set_state(wavelan, wavelan->state);

  return(wavelan);
}

static void
wavelan_free(t_wavelan *wavelan)
{
  TRACE ("Entered wavelan_free");
  
  /* free tooltips */
  g_object_unref(G_OBJECT(wavelan->tooltip_text));

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
  xfce_rc_write_bool_entry (rc, "SignalColors", wavelan->signal_colors);

  xfce_rc_close(rc);
  
}

static void
wavelan_set_orientation(t_wavelan *wavelan, GtkOrientation orientation)
{
  wavelan->orientation = orientation;
  xfce_hvbox_set_orientation(XFCE_HVBOX(wavelan->box), orientation);
  if (orientation == GTK_ORIENTATION_HORIZONTAL) {
   gtk_progress_bar_set_orientation(GTK_PROGRESS_BAR(wavelan->signal), GTK_PROGRESS_BOTTOM_TO_TOP);
   gtk_widget_set_size_request(wavelan->signal, 8, -1);
   gtk_widget_set_size_request(wavelan->ebox, -1, wavelan->size);
   }
  else {
   gtk_progress_bar_set_orientation(GTK_PROGRESS_BAR(wavelan->signal), GTK_PROGRESS_LEFT_TO_RIGHT);
   gtk_widget_set_size_request(wavelan->signal, -1, 8);
   gtk_widget_set_size_request(wavelan->ebox, wavelan->size, -1);
   }
}

static void
wavelan_set_size(t_wavelan *wavelan, int size)
{
  wavelan->size = size;
  gtk_image_set_from_pixbuf(GTK_IMAGE(wavelan->image), gtk_icon_theme_load_icon(gtk_icon_theme_get_default(), "network-wireless", wavelan->size-6, 0, NULL));
  if (wavelan->orientation == GTK_ORIENTATION_HORIZONTAL)
   gtk_widget_set_size_request(wavelan->ebox, -1, wavelan->size);
  else
   gtk_widget_set_size_request(wavelan->ebox, wavelan->size, -1);
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

/* signal colors callback */
static void
wavelan_signal_colors_changed(GtkToggleButton *button, t_wavelan *wavelan)
{
  TRACE ("Entered wavelan_signal_colors_changed");
  wavelan->signal_colors = gtk_toggle_button_get_active(button);
  wavelan_set_state(wavelan, wavelan->state);
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
  GtkWidget *autohide_missing, *header, *warn_label, *signal_colors;
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
  signal_colors = gtk_check_button_new_with_label(_("Enable signal quality colors"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(signal_colors), 
      wavelan->signal_colors);
  g_signal_connect(signal_colors, "toggled", 
      G_CALLBACK(wavelan_signal_colors_changed), wavelan);
  gtk_widget_show(signal_colors);
  gtk_box_pack_start(GTK_BOX(hbox), signal_colors, TRUE, TRUE, 1);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 1);

  for (lp = interfaces; lp != NULL; lp = lp ->next)
    g_free (lp->data);
  g_list_free (interfaces);

  gtk_widget_show (dlg);
  
}

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

  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

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
  
}

#if 0
int main(int argc, char** argv)
{
	struct wi_device *device;
	struct wi_stats stats;
	if ((device = wi_open(argv[1])) == NULL)
		errx(1, "failed to open %s\n", argv[1]);


	if (wi_query(device, &stats) != WI_OK)
		errx(2, "wi_query failed\n");

	printf("NWID:%s, quality:%d%%, rate:%dMb/s\n", stats.ws_netname, stats.ws_quality, stats.ws_rate);
	wi_close(device);
	return 0;
}
#else
XFCE_PANEL_PLUGIN_REGISTER_EXTERNAL(wavelan_construct);
#endif
