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
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4panel/libxfce4panel.h>

#include "wi.h"

#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>

#define BORDER 8
typedef struct
{
  gchar *interface;
  struct wi_device *device;
  guint timer_id;

  gint state; /* can be -1 for disconnected devices */

  gboolean autohide;
  gboolean autohide_missing;
  gboolean signal_colors;
  gboolean show_icon;
  gboolean show_bar;
  gchar *command;

  int size;
  int signal_strength;
  GtkOrientation orientation;

  GtkWidget *box;
  GtkWidget *ebox;
  GtkWidget *image;
  GtkWidget *signal;
  GtkWidget *tooltip_text;
#if GTK_CHECK_VERSION (3, 16, 0)
  GtkCssProvider *css_provider;
#endif

  XfcePanelPlugin *plugin;

} t_wavelan;

enum icon_values {
    OFFLINE = 0,
    EXCELLENT,
    GOOD,
    OK,
    WEAK,
    NONE,
    INIT,
    ICON_NUM
};

char* strength_to_icon[ICON_NUM];

static void wavelan_set_size(XfcePanelPlugin* plugin, int size, t_wavelan *wavelan);
static void wavelan_set_orientation(XfcePanelPlugin* plugin, GtkOrientation orientation, t_wavelan *wavelan);
static void wavelan_refresh_icons(t_wavelan *wavelan);
static void wavelan_update_icon(t_wavelan *wavelan);
static void wavelan_update_signal(t_wavelan *wavelan);

static void
wavelan_refresh_icons(t_wavelan *wavelan)
{
  GtkIconTheme* theme = gtk_icon_theme_get_default();

  if (gtk_icon_theme_has_icon(theme, "network-wireless-signal-excellent-symbolic"))
  {
    strength_to_icon[EXCELLENT] = "network-wireless-signal-excellent-symbolic";
    strength_to_icon[GOOD] = "network-wireless-signal-good-symbolic";
    strength_to_icon[OK] = "network-wireless-signal-ok-symbolic";
    strength_to_icon[WEAK] = "network-wireless-signal-weak-symbolic";
    strength_to_icon[NONE] = "network-wireless-signal-none-symbolic";
    strength_to_icon[OFFLINE] = "network-wireless-offline-symbolic";
  }
  else /* fallback in case symbolic themes aren't present */
  {
    strength_to_icon[EXCELLENT] = "network-wireless-signal-excellent";
    strength_to_icon[GOOD] = "network-wireless-signal-good";
    strength_to_icon[OK] = "network-wireless-signal-weak";
    strength_to_icon[WEAK] = "network-wireless-signal-low";
    strength_to_icon[NONE] = "network-wireless-signal-none";
    strength_to_icon[OFFLINE] = "network-wireless-offline";
  }
  strength_to_icon[INIT] = strength_to_icon[OFFLINE];

  if (wavelan->signal_strength != INIT) /* only wavelan_new sets INIT */
    gtk_image_set_from_icon_name(GTK_IMAGE(wavelan->image), strength_to_icon[wavelan->signal_strength], GTK_ICON_SIZE_BUTTON);
}

static void
wavelan_update_icon(t_wavelan *wavelan)
{
  int signal_strength_prev = wavelan->signal_strength;

  if (!wavelan->show_icon) {
    gtk_widget_hide(wavelan->image);
    return;
  }

  if (wavelan->state > 80)
    wavelan->signal_strength = EXCELLENT;
  else if (wavelan->state > 55)
    wavelan->signal_strength = GOOD;
  else if (wavelan->state > 30)
    wavelan->signal_strength = OK;
  else if (wavelan->state > 5)
    wavelan->signal_strength = WEAK;
  else if (wavelan->state >= 0)
    wavelan->signal_strength = NONE;
  else
    wavelan->signal_strength = OFFLINE; /* also for disconnected interfaces */

  if (signal_strength_prev != wavelan->signal_strength)
    gtk_image_set_from_icon_name(GTK_IMAGE(wavelan->image), strength_to_icon[wavelan->signal_strength], GTK_ICON_SIZE_BUTTON);

  gtk_widget_show(wavelan->image);
}

static void
wavelan_update_signal(t_wavelan *wavelan)
{  
  GdkRGBA color;
  gchar signal_color_bad[] = "#e00000";
  gchar signal_color_weak[] = "#e05200";
  gchar signal_color_good[] = "#e6ff00";
  gchar signal_color_strong[] = "#06c500";
#if GTK_CHECK_VERSION (3, 16, 0)
  gchar *css, *color_str;
#if GTK_CHECK_VERSION (3, 20, 0)
  gchar * cssminsizes = "min-width: 4px; min-height: 0px";
  if(gtk_orientable_get_orientation(GTK_ORIENTABLE(wavelan->signal)) == GTK_ORIENTATION_HORIZONTAL)
    cssminsizes = "min-width: 0px; min-height: 4px";
#endif
#endif

  if (!wavelan->show_bar) {
    gtk_widget_hide(wavelan->signal);
    return;
  }

  if (wavelan->state >= 1)
   gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(wavelan->signal), (gdouble) wavelan->state / 100);
  else
   gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(wavelan->signal), 0.0);

  if (wavelan->signal_colors) {
     /* set color */
   if (wavelan->state > 80)
    gdk_rgba_parse(&color, signal_color_strong);
   else if (wavelan->state > 55)
    gdk_rgba_parse(&color, signal_color_good);
   else if (wavelan->state > 30)
    gdk_rgba_parse(&color, signal_color_weak);
   else
    gdk_rgba_parse(&color, signal_color_bad);

#if GTK_CHECK_VERSION (3, 16, 0)
     color_str = gdk_rgba_to_string(&color);
#if GTK_CHECK_VERSION (3, 20, 0)
     css = g_strdup_printf("progressbar trough { %s } \
                            progressbar progress { %s ; background-color: %s; background-image: none; }",
                           cssminsizes, cssminsizes,
#else
     css = g_strdup_printf(".progressbar { background-color: %s; background-image: none; }",
#endif
                           color_str);
     g_free(color_str);
#else
     gtk_widget_override_background_color(GTK_WIDGET(wavelan->signal),
                             GTK_STATE_PRELIGHT,
                             &color);
     gtk_widget_override_background_color(GTK_WIDGET(wavelan->signal),
                             GTK_STATE_SELECTED,
                             &color);
     gtk_widget_override_color(GTK_WIDGET(wavelan->signal),
                             GTK_STATE_SELECTED,
                             &color);
#endif
  } else {
#if GTK_CHECK_VERSION (3, 20, 0)
     /* only set size... */
     css = g_strdup_printf("progressbar trough { %s } \
                            progressbar progress { %s }",
                           cssminsizes, cssminsizes);
#endif
  }

#if GTK_CHECK_VERSION (3, 16, 0)
  gtk_css_provider_load_from_data (wavelan->css_provider, css, strlen(css), NULL);
  g_free(css);
#endif

  gtk_widget_show(wavelan->signal);
}

static void
wavelan_set_state(t_wavelan *wavelan, gint state)
{
  /* state = 0 -> no link, =-1 -> error */
  DBG ("Entered wavelan_set_state, state = %d", state);

  if(state > 100)
    state = 100;

  wavelan->state = state;

  /* update signal to reflect state */
  wavelan_update_signal(wavelan);

  /* update icon to reflect state */
  wavelan_update_icon(wavelan);

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
        tip = g_strdup(_(wi_strerror(result)));
        wavelan_set_state(wavelan, -1);
      }
    }
    else {
      /*
       * Usual formula is: qual = 4 * (signal - noise)
       * where noise is typically about -96dBm, but we don't have
       * the actual noise value here, so approximate one.
       */
      if (strcmp(stats.ws_qunit, "dBm") == 0)
        wavelan_set_state(wavelan, 4 * (stats.ws_quality - (-96)));
      else
        wavelan_set_state(wavelan, stats.ws_quality);

      if (strlen(stats.ws_netname) > 0)
        /* Translators: net_name: quality quality_unit at rate Mb/s*/
        tip = g_strdup_printf(_("%s: %d%s at %dMb/s"), stats.ws_netname, stats.ws_quality, stats.ws_qunit, stats.ws_rate);
      else
        /* Translators: quality quality_unit at rate Mb/s*/
        tip = g_strdup_printf(_("%d%s at %dMb/s"), stats.ws_quality, stats.ws_qunit, stats.ws_rate);
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
      wavelan->timer_id = g_timeout_add_seconds(1, wavelan_timer, wavelan);
    }
  }
}

/* query installed devices */
static GList*
wavelan_query_interfaces (void)
{
  GList *interfaces = NULL;
  struct ifaddrs *ifaddr, *ifa;

  TRACE ("Entered wavelan_query_interface");

  if (getifaddrs(&ifaddr) == -1) {
    return NULL;
  }
  for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == NULL)
      continue;
#if defined(AF_LINK) /* BSD */
    if (ifa->ifa_addr->sa_family == AF_LINK)
#elif defined(AF_PACKET) /* linux */
    if (ifa->ifa_addr->sa_family == AF_PACKET)
#else
#error "couldnt find a way to get address family on your system"
#endif
      interfaces = g_list_append (interfaces, g_strdup (ifa->ifa_name));
  }
  freeifaddrs(ifaddr);
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
      wavelan->show_icon = xfce_rc_read_bool_entry(rc, "ShowIcon", FALSE);
      wavelan->show_bar = xfce_rc_read_bool_entry(rc, "ShowBar", FALSE);
      if ((s = xfce_rc_read_entry (rc, "Command", NULL)) != NULL)
      {
        wavelan->command = g_strdup (s);
      }
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

static void
wavelan_icon_clicked(GtkWidget *widget, gpointer data,t_wavelan *wavelan)
{
  GError    *error = NULL;
  GtkWidget *message_dialog;

  if (wavelan->command == NULL || strlen(wavelan->command) == 0)
    return;

  if (!xfce_spawn_command_line_on_screen (gtk_widget_get_screen (GTK_WIDGET (widget)),
                                            wavelan->command,
                                          FALSE, FALSE, &error))
    {
      message_dialog = gtk_message_dialog_new_with_markup (NULL,
                                                           GTK_DIALOG_DESTROY_WITH_PARENT,
                                                           GTK_MESSAGE_ERROR,
                                                           GTK_BUTTONS_CLOSE,
                                                           _("<big><b>Failed to execute command \"%s\".</b></big>\n\n%s"),
                                                           wavelan->command,
                                                           error->message);
      gtk_window_set_title (GTK_WINDOW (message_dialog), _("Error"));
      gtk_dialog_run (GTK_DIALOG (message_dialog));
      gtk_widget_destroy (message_dialog);
      g_error_free (error);
    }
}

static t_wavelan *
wavelan_new(XfcePanelPlugin *plugin)
{
  t_wavelan *wavelan;
  GtkSettings* settings;

  TRACE ("Entered wavelan_new");

  wavelan = g_new0(t_wavelan, 1);

  wavelan->autohide = FALSE;
  wavelan->autohide_missing = FALSE;

  wavelan->signal_colors = TRUE;
  wavelan->show_icon = TRUE;
  wavelan->show_bar = TRUE;
  wavelan->command = g_strdup("nm-connection-editor");
  wavelan->state = -2;

  wavelan->plugin = plugin;
  
  wavelan->ebox = gtk_event_box_new();
  gtk_widget_set_has_tooltip(wavelan->ebox, TRUE);
  gtk_event_box_set_visible_window(GTK_EVENT_BOX(wavelan->ebox), FALSE);
  gtk_event_box_set_above_child(GTK_EVENT_BOX(wavelan->ebox), TRUE);
  g_signal_connect(wavelan->ebox, "query-tooltip", G_CALLBACK(tooltip_cb), wavelan);
  g_signal_connect(wavelan->ebox, "button-release-event", G_CALLBACK(wavelan_icon_clicked), wavelan);
  xfce_panel_plugin_add_action_widget(plugin, wavelan->ebox);
  gtk_container_add(GTK_CONTAINER(plugin), wavelan->ebox);

  wavelan->tooltip_text = gtk_label_new(NULL);
  g_object_ref( wavelan->tooltip_text );

  /* create box for img & progress bar */
  wavelan->box = gtk_box_new(wavelan->orientation, 0);

  /* setup progressbar */
  wavelan->signal = gtk_progress_bar_new();
  wavelan->css_provider = gtk_css_provider_new ();
  gtk_style_context_add_provider (
      GTK_STYLE_CONTEXT (gtk_widget_get_style_context (GTK_WIDGET (wavelan->signal))),
      GTK_STYLE_PROVIDER (wavelan->css_provider),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  settings = gtk_settings_get_default();
  g_signal_connect_swapped(settings, "notify::gtk-icon-theme-name", G_CALLBACK(wavelan_refresh_icons), wavelan);
  wavelan->signal_strength = INIT;
  wavelan_refresh_icons(wavelan);
  wavelan->image = gtk_image_new();
  gtk_image_set_from_icon_name (GTK_IMAGE (wavelan->image), strength_to_icon[wavelan->signal_strength], GTK_ICON_SIZE_BUTTON);

  gtk_box_pack_start(GTK_BOX(wavelan->box), GTK_WIDGET(wavelan->image), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(wavelan->box), GTK_WIDGET(wavelan->signal), FALSE, FALSE, 0);


  wavelan_set_size(plugin, xfce_panel_plugin_get_size (plugin), wavelan);
  wavelan_set_orientation(plugin, xfce_panel_plugin_get_orientation (plugin),  wavelan);
  gtk_widget_show_all(wavelan->box);
  gtk_container_add(GTK_CONTAINER(wavelan->ebox), GTK_WIDGET(wavelan->box));
  gtk_widget_show_all(wavelan->ebox);
  
  wavelan_read_config(plugin, wavelan);

  wavelan_set_state(wavelan, wavelan->state);

  return(wavelan);
}

static void
wavelan_free(XfcePanelPlugin* plugin, t_wavelan *wavelan)
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

  if (wavelan->command != NULL)
    g_free(wavelan->command);

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
  xfce_rc_write_bool_entry (rc, "ShowIcon", wavelan->show_icon);
  xfce_rc_write_bool_entry (rc, "ShowBar", wavelan->show_bar);
  if (wavelan->command)
  {
    xfce_rc_write_entry (rc, "Command", wavelan->command);
  }

  xfce_rc_close(rc);
  
}

static void
wavelan_set_orientation(XfcePanelPlugin* plugin, GtkOrientation orientation, t_wavelan *wavelan)
{
  DBG("wavelan_set_orientation(%d)", orientation);
  wavelan->orientation = orientation;
  gtk_orientable_set_orientation(GTK_ORIENTABLE(wavelan->box), orientation);
  gtk_orientable_set_orientation(GTK_ORIENTABLE(wavelan->signal), !orientation);
  gtk_progress_bar_set_inverted(GTK_PROGRESS_BAR(wavelan->signal), (orientation == GTK_ORIENTATION_HORIZONTAL));
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
   gtk_widget_set_size_request(wavelan->ebox, -1, wavelan->size);
  else
   gtk_widget_set_size_request(wavelan->ebox, wavelan->size, -1);
  wavelan_set_state(wavelan, wavelan->state);
}

static void
wavelan_set_size(XfcePanelPlugin* plugin, int size, t_wavelan *wavelan)
{
  int border_width, image_size;
  DBG("wavelan_set_size(%d)", size);
  size /= xfce_panel_plugin_get_nrows(plugin);
  xfce_panel_plugin_set_small (plugin, TRUE);
  border_width = size > 26 ? 2 : 1;
  wavelan->size = size;
  image_size = wavelan->size - (2 * border_width);
  gtk_image_set_pixel_size (GTK_IMAGE (wavelan->image), image_size);
  gtk_container_set_border_width(GTK_CONTAINER(wavelan->box), border_width);
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

/* show icon callback */
static void
wavelan_show_icon_changed(GtkToggleButton *button, t_wavelan *wavelan)
{
  TRACE ("Entered wavelan_show_icon_changed");
  wavelan->show_icon = gtk_toggle_button_get_active(button);
  wavelan_set_state(wavelan, wavelan->state);
}

/* show signal bar callback */
static void
wavelan_show_bar_changed(GtkToggleButton *button, t_wavelan *wavelan)
{
  TRACE ("Entered wavelan_show_bar_changed");
  wavelan->show_bar = gtk_toggle_button_get_active(button);
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

/* command changed callback */
static void
wavelan_command_changed(GtkEntry *entry, t_wavelan *wavelan)
{
  if (wavelan->command != NULL)
    g_free(wavelan->command);
  wavelan->command = g_strdup(gtk_entry_get_text(entry));
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
  GtkWidget *autohide_missing, *warn_label, *signal_colors, *show_icon, *show_bar, *command;
  GtkWidget *combo;
  GList     *interfaces, *lp;

  TRACE ("Entered wavelan_create_options");
  
  dlg = xfce_titled_dialog_new_with_buttons (_("Wavelan Plugin Options"),
              GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (plugin))),
              GTK_DIALOG_DESTROY_WITH_PARENT,
              "gtk-close",
              GTK_RESPONSE_OK,
              NULL);

  gtk_window_set_position (GTK_WINDOW (dlg), GTK_WIN_POS_CENTER);
  gtk_window_set_icon_name (GTK_WINDOW (dlg), "network-wireless");

  g_signal_connect (dlg, "response", G_CALLBACK (wavelan_dialog_response),
                    wavelan);

  xfce_titled_dialog_set_subtitle (XFCE_TITLED_DIALOG (dlg), _("Properties"));

  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
  gtk_widget_show(vbox);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area(GTK_DIALOG (dlg))), vbox,
                      TRUE, TRUE, 0);
  
  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_show(hbox);
  
  label = gtk_label_new(_("Interface"));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_widget_show(label);

  interfaces = wavelan_query_interfaces ();
  combo = gtk_combo_box_text_new_with_entry ();
  for (lp = interfaces; lp != NULL; lp = lp->next)
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), lp->data);
  gtk_widget_show (combo);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  interface = gtk_bin_get_child (GTK_BIN (combo));
  if (wavelan->interface != NULL)
    gtk_entry_set_text(GTK_ENTRY(interface), wavelan->interface);
  g_signal_connect(interface, "changed", G_CALLBACK(wavelan_interface_changed),
      wavelan);
  gtk_widget_show(interface);

  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), combo, TRUE, TRUE, 0);

  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_show(hbox);
  autohide = gtk_check_button_new_with_mnemonic(_("_Autohide when offline"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(autohide), wavelan->autohide);
  g_signal_connect(autohide, "toggled", G_CALLBACK(wavelan_autohide_changed),
      wavelan);
  gtk_widget_show(autohide);
  gtk_box_pack_start(GTK_BOX(hbox), autohide, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  
  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_show(hbox);
  autohide_missing = gtk_check_button_new_with_mnemonic(_("Autohide when no _hardware present"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(autohide_missing), 
      wavelan->autohide_missing);
  g_signal_connect(autohide_missing, "toggled", 
      G_CALLBACK(wavelan_autohide_missing_changed), wavelan);
  gtk_widget_show(autohide_missing);
  gtk_box_pack_start(GTK_BOX(hbox), autohide_missing, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_set_margin_start (GTK_WIDGET (hbox), 12);
  gtk_widget_show(hbox);
  warn_label = gtk_label_new(_("Note: This will make it difficult to remove or configure the plugin if there is no device detected."));
  gtk_label_set_line_wrap(GTK_LABEL(warn_label), TRUE);
  gtk_label_set_xalign (GTK_LABEL (warn_label), 0.0f);
  gtk_widget_show(warn_label);
  gtk_box_pack_start(GTK_BOX(hbox), warn_label, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_show(hbox);
  show_icon = gtk_check_button_new_with_mnemonic(_("Show _icon"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(show_icon), 
      wavelan->show_icon);
  g_signal_connect(show_icon, "toggled", 
      G_CALLBACK(wavelan_show_icon_changed), wavelan);
  gtk_widget_show(show_icon);
  gtk_box_pack_start(GTK_BOX(hbox), show_icon, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_show(hbox);
  show_bar = gtk_check_button_new_with_mnemonic(_("Show signal _bar"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(show_bar),
      wavelan->show_bar);
  g_signal_connect(show_bar, "toggled",
      G_CALLBACK(wavelan_show_bar_changed), wavelan);
  gtk_widget_show(show_bar);
  gtk_box_pack_start(GTK_BOX(hbox), show_bar, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_show(hbox);
  signal_colors = gtk_check_button_new_with_mnemonic(_("Enable sig_nal quality colors"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(signal_colors),
      wavelan->signal_colors);
  g_signal_connect(signal_colors, "toggled",
      G_CALLBACK(wavelan_signal_colors_changed), wavelan);
  gtk_widget_show(signal_colors);
  gtk_box_pack_start(GTK_BOX(hbox), signal_colors, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_show(hbox);
  label = gtk_label_new(_("Wifi Manager Command"));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_widget_show(label);
  command = gtk_entry_new();
  if (wavelan->command != NULL)
    gtk_entry_set_text(GTK_ENTRY(command), wavelan->command);
  g_signal_connect(command, "changed", G_CALLBACK(wavelan_command_changed),
      wavelan);
  gtk_widget_show(command);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), command, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  for (lp = interfaces; lp != NULL; lp = lp ->next)
    g_free (lp->data);
  g_list_free (interfaces);

  gtk_widget_show (dlg);
  
}

static void
wavelan_show_about (XfcePanelPlugin *plugin, t_wavelan *wavelan)
{
   GdkPixbuf *icon;
   const gchar *auth[] = { "Benedikt Meurer <benny at xfce.org>", "Florian Rivoal <frivoal@xfce.org>", NULL };
   icon = xfce_panel_pixbuf_from_source("network-wireless", NULL, 32);
   gtk_show_about_dialog(NULL,
      "logo", icon,
      "license", xfce_get_license_text (XFCE_LICENSE_TEXT_BSD),
      "version", PACKAGE_VERSION,
      "program-name", PACKAGE_NAME,
      "comments", _("View the status of a wireless network"),
      "website", "https://docs.xfce.org/panel-plugins/xfce4-wavelan-plugin",
      "copyright", "Copyright (c) 2003-2004 Benedikt Meurer\n",
      "authors", auth, NULL);

   if(icon)
      g_object_unref(G_OBJECT(icon));
}

static void
wavelan_construct (XfcePanelPlugin *plugin)
{
  t_wavelan *wavelan = wavelan_new(plugin);

  TRACE ("Entered wavelan_construct");

  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  g_signal_connect (plugin, "orientation-changed",
                    G_CALLBACK (wavelan_set_orientation), wavelan);

  g_signal_connect (plugin, "size-changed",
                    G_CALLBACK (wavelan_set_size), wavelan);
 
  g_signal_connect (plugin, "free-data",
                    G_CALLBACK (wavelan_free), wavelan);

  g_signal_connect (plugin, "save",
                    G_CALLBACK (wavelan_write_config), wavelan);
  
  xfce_panel_plugin_menu_show_configure (plugin);
  g_signal_connect (plugin, "configure-plugin",
                    G_CALLBACK (wavelan_create_options), wavelan);
  
  xfce_panel_plugin_menu_show_about(plugin);
  g_signal_connect (plugin, "about", G_CALLBACK (wavelan_show_about), wavelan);
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
XFCE_PANEL_PLUGIN_REGISTER(wavelan_construct);
#endif
