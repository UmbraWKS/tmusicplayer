#include "../files/files.h"
#include "glib.h"
#include "mpv_audio.h"
#include <string.h>

// TODO: print all g_printerr on error windows

static const gchar *mpris_root_xml =
    "<node>"
    "  <interface name='org.mpris.MediaPlayer2'>"
    "    <method name='Raise'/>"
    "    <method name='Quit'/>"
    "    <property name='CanQuit' type='b' access='read'/>"
    "    <property name='CanRaise' type='b' access='read'/>"
    "    <property name='HasTrackList' type='b' access='read'/>"
    "    <property name='Identity' type='s' access='read'/>"
    "    <property name='SupportedUriSchemes' type='as' access='read'/>"
    "    <property name='SupportedMimeTypes' type='as' access='read'/>"
    "  </interface>"
    "</node>";

static const gchar *mpris_player_xml =
    "<node>"
    "  <interface name='org.mpris.MediaPlayer2.Player'>"
    "    <method name='Next'/>"
    "    <method name='Previous'/>"
    "    <method name='Pause'/>"
    "    <method name='PlayPause'/>"
    "    <method name='Stop'/>"
    "    <method name='Play'/>"
    "    <property name='PlaybackStatus' type='s' access='read'/>"
    "    <property name='Metadata' type='a{sv}' access='read'/>"
    "    <property name='Volume' type='d' access='readwrite'/>"
    "    <property name='Position' type='x' access='read'/>"
    "    <property name='CanGoNext' type='b' access='read'/>"
    "    <property name='CanGoPrevious' type='b' access='read'/>"
    "    <property name='CanPlay' type='b' access='read'/>"
    "    <property name='CanPause' type='b' access='read'/>"
    "    <property name='CanSeek' type='b' access='read'/>"
    "    <property name='CanControl' type='b' access='read'/>"
    "    <signal name='Seeked'>"
    "      <arg name='Position' type='x'/>"
    "    </signal>"
    "  </interface>"
    "</node>";

mpris_player_t *mpris_ctx = NULL;

static GDBusNodeInfo *mpris_root_node_info = NULL;
static GDBusNodeInfo *mpris_player_node_info = NULL;

static const GDBusInterfaceVTable mpris_root_vtable = {NULL, NULL, NULL};
static const GDBusInterfaceVTable mpris_player_vtable = {NULL, NULL, NULL};

mpris_player_t *init_mpris_player() {
  mpris_player_t *player = g_new0(mpris_player_t, 1);
  player->playback_status = g_strdup("Stopped");
  player->volume = update_mpris_volume(settings->volume);
  player->position = 0; // just an initialization, the updating is dynamic
  player->metadata = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                           (GDestroyNotify)g_variant_unref);
  player->connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &player->error);
  if (!player->connection) {
    g_printerr("Failed to connect to session bus: %s\n",
               player->error->message);
    g_error_free(player->error);
    return NULL;
  }

  return player;
}

void cleanup_player() {
  if (mpris_ctx) {
    g_free(mpris_ctx->playback_status);
    g_hash_table_destroy(mpris_ctx->metadata);
    g_free(mpris_ctx);
    mpris_ctx = NULL;
  }
}

void on_name_acquired(GDBusConnection *connection, const gchar *name,
                      gpointer user_data) {}

void on_name_lost(GDBusConnection *connection, const gchar *name,
                  gpointer user_data) {
  g_main_loop_quit((GMainLoop *)user_data);
}

static void
handle_root_method_call(GDBusConnection *connection, const gchar *sender,
                        const gchar *object_path, const gchar *interface_name,
                        const gchar *method_name, GVariant *parameters,
                        GDBusMethodInvocation *invocation, gpointer user_data) {

  if (g_strcmp0(method_name, "Raise") == 0) {
    // TODO: implement raise
    g_dbus_method_invocation_return_value(invocation, NULL);
  } else if (g_strcmp0(method_name, "Quit") == 0) {
    // TODO: implement quit
    g_dbus_method_invocation_return_value(invocation, NULL);
    g_main_loop_quit((GMainLoop *)user_data);
  }
}

// callback from system calls to player functions
static void handle_player_method_call(
    GDBusConnection *connection, const gchar *sender, const gchar *object_path,
    const gchar *interface_name, const gchar *method_name, GVariant *parameters,
    GDBusMethodInvocation *invocation, gpointer user_data) {

  if (g_strcmp0(method_name, "Play") == 0)
    play_player();
  else if (g_strcmp0(method_name, "Pause") == 0)
    pause_player();
  else if (g_strcmp0(method_name, "PlayPause") == 0)
    play_pause();
  else if (g_strcmp0(method_name, "Next") == 0)
    skip_song();
  else if (g_strcmp0(method_name, "Previous") == 0)
    previous_song();

  g_dbus_method_invocation_return_value(invocation, NULL);
}

// properties getters
static GVariant *handle_root_get_property(GDBusConnection *connection,
                                          const gchar *sender,
                                          const gchar *object_path,
                                          const gchar *interface_name,
                                          const gchar *property_name,
                                          GError **error, gpointer user_data) {

  if (g_strcmp0(property_name, "CanQuit") == 0) {
    return g_variant_new_boolean(TRUE);
  } else if (g_strcmp0(property_name, "CanRaise") == 0) {
    return g_variant_new_boolean(TRUE);
  } else if (g_strcmp0(property_name, "HasTrackList") == 0) {
    return g_variant_new_boolean(FALSE);
  } else if (g_strcmp0(property_name, "Identity") == 0) {
    return g_variant_new_string("TMusicPlayer");
  } else if (g_strcmp0(property_name, "SupportedUriSchemes") == 0) {
    const gchar *schemes[] = {"file", "http", "https", NULL};
    return g_variant_new_strv(schemes, -1);
  } else if (g_strcmp0(property_name, "SupportedMimeTypes") == 0) {
    // since subsonic api can return basically all formats i just include the
    // most popular
    const gchar *types[] = {"audio/mpeg", "audio/ogg", "audio/flac",
                            "audio/wav",  "audio/aac", "audio/mp4",
                            "audio/webm", NULL};
    return g_variant_new_strv(types, -1);
  }

  return NULL;
}

static GVariant *handle_player_get_property(
    GDBusConnection *connection, const gchar *sender, const gchar *object_path,
    const gchar *interface_name, const gchar *property_name, GError **error,
    gpointer user_data) {

  // TODO: add loop status
  if (g_strcmp0(property_name, "PlaybackStatus") == 0) {
    return g_variant_new_string(mpris_ctx->playback_status);
  } else if (g_strcmp0(property_name, "Volume") == 0) {
    return g_variant_new_double(mpris_ctx->volume);
  } else if (g_strcmp0(property_name, "Position") == 0) {
    return g_variant_new_int64(mpris_ctx->position);
  } else if (g_strcmp0(property_name, "CanGoNext") == 0) {
    return g_variant_new_boolean(TRUE);
  } else if (g_strcmp0(property_name, "CanGoPrevious") == 0) {
    return g_variant_new_boolean(TRUE);
  } else if (g_strcmp0(property_name, "CanPlay") == 0) {
    return g_variant_new_boolean(TRUE);
  } else if (g_strcmp0(property_name, "CanPause") == 0) {
    return g_variant_new_boolean(TRUE);
  } else if (g_strcmp0(property_name, "CanSeek") == 0) {
    return g_variant_new_boolean(FALSE);
  } else if (g_strcmp0(property_name, "CanControl") == 0) {
    return g_variant_new_boolean(TRUE);
  } else if (g_strcmp0(property_name, "Metadata") == 0) {

    // building metadata for mpris
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));

    if (user_selection.playing_song) {
      size_t len = strlen(user_selection.playing_song->id) +
                   strlen("/tmusicplayer/track/") + 1;
      char *track_path = malloc(len);
      snprintf(track_path, len, "/tmusicplayer/track/%s",
               user_selection.playing_song->id);
      g_variant_builder_add(&builder, "{sv}", "mpris:trackid",
                            g_variant_new_object_path(track_path));
      g_variant_builder_add(
          &builder, "{sv}", "xesam:title",
          g_variant_new_string(user_selection.playing_song->title));
      g_variant_builder_add(
          &builder, "{sv}", "xesam:length",
          g_variant_new_int64(user_selection.playing_song->duration));

      char *call_param = malloc(
          strlen("&id=") + strlen(user_selection.playing_song->cover_art) + 1);
      sprintf(call_param, "&id=%s", user_selection.playing_song->cover_art);
      char *art_url;
      if (call_param)
        art_url = url_formatter(server, "getCoverArt", call_param);
      if (art_url) {
        g_variant_builder_add(&builder, "{sv}", "mpris:artUrl",
                              g_variant_new_string(art_url));
        free(call_param);
        free(art_url);
      }
      free(track_path);
    }
    if (user_selection.album)
      g_variant_builder_add(&builder, "{sv}", "xesam:album",
                            g_variant_new_string(user_selection.album->title));
    if (user_selection.artist)
      g_variant_builder_add(&builder, "{sv}", "xesam:artist",
                            g_variant_new_string(user_selection.artist->name));

    return g_variant_builder_end(&builder);
  }

  return NULL;
}

// properties setters
static gboolean handle_player_set_property(
    GDBusConnection *connection, const gchar *sender, const gchar *object_path,
    const gchar *interface_name, const gchar *property_name, GVariant *value,
    GError **error, gpointer user_data) {

  if (g_strcmp0(property_name, "Volume") == 0) {
    int volume = g_variant_get_double(value) * 100;
    set_volume(volume);
    return TRUE;
  }

  return FALSE;
}

// interface vtables
static const GDBusInterfaceVTable root_interface_vtable = {
    handle_root_method_call, handle_root_get_property,
    NULL, // No settable properties
};

static const GDBusInterfaceVTable player_interface_vtable = {
    handle_player_method_call,
    handle_player_get_property,
    handle_player_set_property,
};

void on_bus_acquired(GDBusConnection *connection, const gchar *name,
                     gpointer user_data) {
  GDBusNodeInfo *root_node_info, *player_node_info;
  GMainLoop *loop = (GMainLoop *)user_data;

  // parsing interface definitions
  root_node_info =
      g_dbus_node_info_new_for_xml(mpris_root_xml, &mpris_ctx->error);
  if (mpris_ctx->error) {
    g_printerr("Failed to parse root interface: %s\n",
               mpris_ctx->error->message);
    g_error_free(mpris_ctx->error);
    return;
  }

  player_node_info =
      g_dbus_node_info_new_for_xml(mpris_player_xml, &mpris_ctx->error);
  if (mpris_ctx->error) {
    g_printerr("Failed to parse player interface: %s\n",
               mpris_ctx->error->message);
    g_error_free(mpris_ctx->error);
    g_dbus_node_info_unref(root_node_info);
    return;
  }

  // registering root interface
  g_dbus_connection_register_object(
      connection, "/org/mpris/MediaPlayer2", root_node_info->interfaces[0],
      &root_interface_vtable, loop, NULL, &mpris_ctx->error);
  if (mpris_ctx->error) {
    g_printerr("Failed to register root interface: %s\n",
               mpris_ctx->error->message);
    g_error_free(mpris_ctx->error);
  }

  mpris_ctx->error = NULL;

  // registering player interface
  mpris_ctx->registration_id = g_dbus_connection_register_object(
      connection, "/org/mpris/MediaPlayer2", player_node_info->interfaces[0],
      &player_interface_vtable, loop, NULL, &mpris_ctx->error);

  if (mpris_ctx->registration_id == 0) {
    if (mpris_ctx->error != NULL) {
      g_printerr("Failed to register player interface: %s\n",
                 mpris_ctx->error->message);
      g_error_free(mpris_ctx->error);
      mpris_ctx->error = NULL;
    } else
      g_printerr("Failed to register player interface: unknown reason\n");

    return;
  }
  mpris_ctx->connection = connection;

  g_dbus_node_info_unref(root_node_info);
  g_dbus_node_info_unref(player_node_info);
}

const char *convert_status(mpv_status_t status) {
  switch (status) {
  case MPV_STATUS_PLAYING:
    return "Playing";
  case MPV_STATUS_PAUSED:
    return "Paused";
  default:
    return "Stopped";
  }
}

void update_mpris_status(const char *status) {
  g_free(mpris_ctx->playback_status);
  mpris_ctx->playback_status = g_strdup(status);
}

gfloat update_mpris_volume(int volume) { return (float)volume / 100; }

void update_mpris_position(double time) {
  mpris_ctx->position = (gint64)(time * 1000000);
}
