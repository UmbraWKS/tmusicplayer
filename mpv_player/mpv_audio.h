#include "../files/files.h"
#include <mpv/client.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <systemd/sd-bus-vtable.h>
#include <systemd/sd-bus.h>
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
  MPV_STATUS_STOPPED,
  MPV_STATUS_CLOSING, // being shutdown
  MPV_STATUS_ERROR    // error during initialization
} mpv_status_t;
#endif

#ifndef MPRIS
#define MPRIS

typedef struct {
  bool playing;
  double position; // microseconds
  int64_t duration;
  char *title;
  char *artist;
  double volume;
  char *loop_status;
} player_state_t;

extern sd_bus *bus;
#endif

// initializes the mpv_handle and sets all the parameters needed by the player
void *init_player(void *arg);
// the loop of the player
void mpv_main_loop();
// to be called after a new queue is created, it starts playing the queue from
// the first song
void start_new_playback(char *id);
// checks for errors during initialization and sets the error flag
static inline void check_error(int status);
int get_volume();
// cycles play and pause
void play_pause();
// pauses
void pause_player();
// plays
void play_player();
// stops the player (mpris definition), the song is reset at 0 and the player
// pauses
void stop_player();
// returns the status of the player
mpv_status_t get_mpv_status();
// closes the mpv instance gracefully
void shutdown_player();
// returns the current position of the song playing
int64_t get_current_position();
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
// seek absolute to passed position
void seek_absolute(int64_t position);

/* MPRIS functions */
// initializes MPRIS
void mpris_init();
void mpris_free();
// main mpris process
void mpris_process();
// converts the mpv_status_t into status for mpris
char *convert_status(mpv_status_t status);
// converts from loop_status_t to char
char *convert_loop_status(loop_status_t status);
// updating mpris data
void mpris_metadata_changed(void);
void mpris_playback_status_changed(void);
void mpris_loop_status_changed(void);
void mpris_shuffle_changed(void);
void mpris_volume_changed(void);
void mpris_position_changed(void);
void mpris_seeked(void);
