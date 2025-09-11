#include "../files/files.h"
#include <gio/gio.h>
#include <glib.h>
#include <mpv/client.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>

#ifndef MPV_AUDIO
#define MPV_AUDIO

// different states of the player
// needed for control over the player itself
typedef enum {
  MPV_STATUS_IDLE,         // created but not initialized or closed/shutdown
  MPV_STATUS_NOT_PLAYING,  // created and initialized but nothing is playing
  MPV_STATUS_INITIALIZING, // creating the handler and assigning properties
  MPV_STATUS_READY,        // initialized ready to play music
  MPV_STATUS_PLAYING,
  MPV_STATUS_PAUSED,
  MPV_STATUS_CLOSING, // being shutdown
  MPV_STATUS_ERROR    // error during initialization
} mpv_status_t;
#endif

#ifndef MPRIS
#define MPRIS

typedef struct {
  gchar *playback_status; // "Playing", "Paused", "Stopped"
  gchar *loop_status;
  gfloat volume; // using a different volume because playerctl ranges 0.0 - 1.0
  gint64 position; // passed time playing
  GHashTable *metadata;
  GDBusConnection *connection;
  GError *error;
  guint registration_id;
  guint owner_id;
} mpris_player_t;

extern mpris_player_t *mpris_ctx;
#endif

// initializes the mpv_handle and sets all the parameters needed by the player
void *init_player(void *arg);
// to be called after a new queue is created, it starts playing the queue from
// the first song
void start_new_playback();
// checks for errors during initialization and sets the error flag
static inline void check_error(int status);
int get_volume();
// cycles play and pause
void play_pause();
// pauses
void pause_player();
// plays
void play_player();
// returns the status of the player
mpv_status_t get_mpv_status();
// closes the mpv instance gracefully
void shutdown_player();
/*
  async volume operations, values chnaged by 5 manually limited,
  as mpv doesnt have a built in limit for volume, if values go below/over
  0/100 it's manually set to 0 or 100
*/
void volume_up();
void volume_down();
// volume limited to 0-100
void set_volume(int volume);
// skips to the next song
void skip_song();
// starts api call to stream song matching id
void play_song(char *id);
// plays previous song in queue
void previous_song();
// return the current time in milliseconds
uint64_t current_time_millis();

/* MPRIS functions */
// initializes the connection
mpris_player_t *init_mpris_player();
// frees memory
void cleanup_player();
// acquired connection to bus
void on_name_acquired(GDBusConnection *connection, const gchar *name,
                      gpointer user_data);
// closed connection to bus
void on_name_lost(GDBusConnection *connection, const gchar *name,
                  gpointer user_data);
void on_bus_acquired(GDBusConnection *connection, const gchar *name,
                     gpointer user_data);
// converts the mpv_status_t into status for mpris
const char *convert_status(mpv_status_t status);
// updates the status of mpris based on user inputs inside the app
void update_mpris_status(const char *status);
// updates the mpris volume
gfloat update_mpris_volume(int volume);
// updates position metadata
void update_mpris_position(double time);
// converts from loop_status_t to char
const char *convert_loop_status(loop_status_t status);
// updates the loop status in mpris
void update_mpris_loop_status(const char *status);
