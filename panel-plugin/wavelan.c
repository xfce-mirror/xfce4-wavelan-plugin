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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include <libxfce4util/i18n.h>
#include <libxfcegui4/dialogs.h>
#include <panel/plugins.h>
#include <panel/xfce.h>

#include <wi.h>

typedef struct
{
  gchar *interface;
  struct wi_device *device;
  guint timer_id;
	GtkWidget	*ebox;
  GtkWidget *box;
	GtkWidget	*indicator;
  GtkTooltips *tooltips;
} t_wavelan;

static gboolean
wavelan_timer(gpointer data)
{
  struct wi_stats stats;
  char *tip = NULL;
  t_wavelan *wavelan = (t_wavelan *)data;

  if (wavelan->device != NULL) {
    int result;

    if ((result = wi_query(wavelan->device, &stats)) != WI_OK) {
      /* reset quality indicator */
      gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(wavelan->indicator), 0.0);
      gtk_widget_set_sensitive(wavelan->indicator, FALSE);

      if (result == WI_NOCARRIER) {
        tip = g_strdup_printf(_(
              "Interface: %s\n"
              "Vendor:    %s\n"
              "No carrier signal"),
            wavelan->interface,
            stats.ws_vendor);
      }
      else {
        /* set error */
        tip = g_strdup_printf(_(
              "Interface: %s\n"
              "%s"),
            wavelan->interface,
            wi_strerror(result));
      }
    }
    else {
      gtk_widget_set_sensitive(wavelan->indicator, TRUE);
      gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(wavelan->indicator),
          (double)stats.ws_quality / 100.0);

      tip = g_strdup_printf(_(
            "Interface: %s\n"
            "Vendor:    %s\n"
            "Quality:   %d%%\n"
            "Network:   %s"),
          wavelan->interface,
          stats.ws_vendor,
          stats.ws_quality,
          stats.ws_netname);
    }
  }
  else {
    tip = g_strdup(_("No device configured"));
  }

  /* activate new tooltip */
  if (tip != NULL) {
    gtk_tooltips_set_tip(wavelan->tooltips, wavelan->ebox, tip, NULL);
    g_free(tip);
  }

  /* keep the timeout running */
  return(TRUE);
}

static t_wavelan *
wavelan_new(void)
{
	t_wavelan *wavelan;

	wavelan = g_new0(t_wavelan, 1);

	wavelan->ebox = gtk_event_box_new();
	gtk_widget_show(wavelan->ebox);

  wavelan->box = gtk_hbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(wavelan->box), border_width);
	gtk_widget_show(wavelan->box);
	gtk_container_add(GTK_CONTAINER(wavelan->ebox), wavelan->box);

  wavelan->indicator = gtk_progress_bar_new();
	gtk_widget_show(wavelan->indicator);
	gtk_container_add(GTK_CONTAINER(wavelan->box), wavelan->indicator);

  /* create tooltips */
  wavelan->tooltips = gtk_tooltips_new();

  /* update the gui */
  wavelan_timer(wavelan);

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

	g_return_if_fail(ctrl != NULL);
	g_return_if_fail(ctrl->data != NULL);

	wavelan = (t_wavelan *)ctrl->data;

  /* free tooltips */
  g_object_unref(G_OBJECT(wavelan->tooltips));

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
  if (settings.orientation == HORIZONTAL) {
    gtk_widget_set_size_request(GTK_WIDGET(wavelan->indicator),
        6 + 2 * size, icon_size[size]);
  }
  else {
    gtk_widget_set_size_request(GTK_WIDGET(wavelan->indicator),
        icon_size[size], 6 + 2 * size);
  }
}

static void
wavelan_set_orientation(Control *ctrl, int orientation)
{
  t_wavelan *wavelan = (t_wavelan *)ctrl->data;
  GtkWidget *box;

  if (orientation == HORIZONTAL) {
    gtk_progress_bar_set_orientation(GTK_PROGRESS_BAR(wavelan->indicator),
        GTK_PROGRESS_BOTTOM_TO_TOP);
    box = gtk_hbox_new(FALSE, 0);
  }
  else {
    gtk_progress_bar_set_orientation(GTK_PROGRESS_BAR(wavelan->indicator),
        GTK_PROGRESS_LEFT_TO_RIGHT);
    box = gtk_vbox_new(FALSE, 0);
  }

  gtk_container_set_border_width(GTK_CONTAINER(box), border_width);
  gtk_object_ref(GTK_OBJECT(wavelan->indicator));
  gtk_container_remove(GTK_CONTAINER(wavelan->box), wavelan->indicator);
  gtk_container_add(GTK_CONTAINER(box), wavelan->indicator);
  gtk_container_remove(GTK_CONTAINER(wavelan->ebox), wavelan->box);
  gtk_container_add(GTK_CONTAINER(wavelan->ebox), box);
  gtk_object_unref(GTK_OBJECT(wavelan->indicator));
  gtk_widget_show(box);
  wavelan->box = box;
}

/* interface changed callback */
static void
wavelan_interface_changed(GtkWidget *widget, gpointer data)
{
  t_wavelan *wavelan = (t_wavelan *)data;
  if (wavelan->interface != NULL)
    g_free(wavelan->interface);
  wavelan->interface = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));
  wavelan_configure(wavelan);
}

/* options dialog */
static void
wavelan_create_options (Control *ctrl, GtkContainer *con, GtkWidget *done)
{
  t_wavelan *wavelan = (t_wavelan *)ctrl->data;
  GtkWidget *hbox, *label, *interface;

  hbox = gtk_hbox_new(FALSE, 2);
  gtk_widget_show(hbox);
  label = gtk_label_new(_("Interface"));
  gtk_widget_show(label);
  interface = gtk_entry_new();
  gtk_entry_set_max_length(GTK_ENTRY(interface), 5);
  if (wavelan->interface != NULL)
    gtk_entry_set_text(GTK_ENTRY(interface), wavelan->interface);
  g_signal_connect(interface, "changed", G_CALLBACK(wavelan_interface_changed),
      wavelan);
  gtk_widget_show(interface);
  gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, FALSE, 1);
  gtk_box_pack_start(GTK_BOX(hbox), interface, TRUE, FALSE, 1);
  gtk_container_add(GTK_CONTAINER(con), hbox);
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
