#include <mpv/client.h>
#include <pthread.h>
#include <stdbool.h>

#ifndef MPV_AUDIO
#define MPV_AUDIO

// different states of the player
// needed for control over the player itself
typedef enum {
  MPV_STATUS_IDLE,         // created but not initialized or closed/shutdown
  MPV_STATUS_INITIALIZING, // creating the handler and assigning properties
  MPV_STATUS_READY,        // initialized ready to play music
  MPV_STATUS_PLAYING,
  MPV_STATUS_PAUSED,
  MPV_STATUS_CLOSING, // being shutdown
  MPV_STATUS_ERROR    // error during initialization
} mpv_status_t;
#endif

// needs a Queue as argument
void *play_queue(void *arg);
// checks for errors during initialization and sets the error flag
static inline void check_error(int status);
int get_volume();
void pause_toggle();
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
