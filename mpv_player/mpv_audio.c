#include "mpv_audio.h"
#include "../data/data_struct.h"
#include "../files/files.h"
#include "mpv_callback.h"
#include <mpv/client.h>
#include <pthread.h>
/*
    By passing the url it automatically handles buffering and streaming,
    the file is played while it downloads
*/
mpv_handle *ctx = NULL;
pthread_mutex_t mpv_mutex = PTHREAD_MUTEX_INITIALIZER;
mpv_status_t status = MPV_STATUS_IDLE;
// flag determining if the status changed to/from:
// IDLE, PLAYING, PAUSED
bool status_changed = false;
// variable holding the time passed in seconds of the song playing
int time_passed = -1;

void *init_player(void *arg) {
  pthread_mutex_lock(&mpv_mutex);
  status = MPV_STATUS_INITIALIZING;
  ctx = mpv_create();
  if (!ctx) {
    // TODO: print in error window
    perror("Failed creating mpv\n");
    status = MPV_STATUS_ERROR;
    pthread_mutex_unlock(&mpv_mutex);
    return NULL;
  }
  char vol_str[20];
  sprintf(vol_str, "%d", settings->volume);
  check_error(mpv_set_option_string(ctx, "volume", vol_str));
  check_error(mpv_set_option_string(ctx, "input-default-bindings", "no"));
  check_error(mpv_set_option_string(ctx, "input-vo-keyboard", "no"));
  check_error(mpv_set_option_string(ctx, "input-terminal", "no"));
  check_error(mpv_set_option_string(ctx, "vo", "null"));
  // initializing the player
  check_error(mpv_initialize(ctx));
  status = MPV_STATUS_READY;
  pthread_mutex_unlock(&mpv_mutex);

  // listening to time advancement in song
  mpv_observe_property(ctx, 1, "time-pos", MPV_FORMAT_DOUBLE);
  // player loop
  while (1) {
    mpv_event *event = mpv_wait_event(ctx, 1000);

    if (event->event_id == MPV_EVENT_SHUTDOWN) {
      break;
      // when a song ends playing

      // TODO: append the next song and after the end of what is playing check
      // if it changed in the queue and handle it appropriately
    } else if (event->event_id == MPV_EVENT_END_FILE) {
      // not starting the previous song if the current one has been manually
      // replaced
      mpv_event_end_file *eof = (mpv_event_end_file *)event->data;
      if (eof->reason != MPV_END_FILE_REASON_STOP) {
        // finding the song in the list
        if (settings->loop != TRACK) {

          Song *tmp =
              get_song_from_id(queue->songs, user_selection.playing_song->id);
          // playing
          if (tmp && tmp->next) {
            tmp = tmp->next;
            play_song(tmp->id);
          } else {
            // checking loop status
            if (settings->loop == QUEUE && queue->songs)
              play_song(queue->songs->id);
            else if (settings->loop == NONE) {
              status = MPV_STATUS_NOT_PLAYING;
              status_changed = true;
            }
          }
        } else { // loop in TRACK status
          play_song(user_selection.playing_song->id);
        }
      }
    } else if (event->event_id == MPV_EVENT_PLAYBACK_RESTART) {
      // song was skipped while status PAUSED
      if (status == MPV_STATUS_PAUSED)
        play_player();

      status = MPV_STATUS_PLAYING;
      status_changed = true;
    } else if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
      mpv_event_property *prop = (mpv_event_property *)event->data;
      if (strcmp(prop->name, "time-pos") == 0 && status == MPV_STATUS_PLAYING) {
        if (prop->format == MPV_FORMAT_DOUBLE && prop->data) {
          double time_stamp = *(double *)prop->data;

          update_mpris_position(time_stamp);

          // i only care about seconds, so i am limiting function call to second
          // change only
          if ((int)time_stamp != time_passed) {
            playback_time((int)time_stamp);
            time_passed = (int)time_stamp;
          }
        }
      }
    }
    // calling when the status of the song playing changes
    if (status_changed) {
      playback_status(status);
      update_mpris_status(convert_status(status));
      status_changed = false;
    }
  }
  pthread_mutex_lock(&mpv_mutex);
  status = MPV_STATUS_CLOSING;
  mpv_terminate_destroy(ctx);
  ctx = NULL;
  status = MPV_STATUS_IDLE;
  playback_status(status);
  pthread_mutex_unlock(&mpv_mutex);
  return NULL;
}

void start_new_playback() {
  // clearing previous playlist
  const char *cmd[] = {"playlist-clear", NULL};
  // playing the first song of the new queue
  play_song(queue->songs->id);
}

void play_song(char *id) {
  char *params = song_params(id);
  char *url = url_formatter(server, "stream", params);
  const char *cmd[] = {"loadfile", url, "replace", NULL};
  pthread_mutex_lock(&mpv_mutex);
  check_error(mpv_command(ctx, cmd));
  pthread_mutex_unlock(&mpv_mutex);
  if (status != MPV_STATUS_ERROR) {
    Song *song = get_song_from_id(queue->songs, id);
    currently_playing(song);
  }
  free(params);
  free(url);
}

mpv_status_t get_mpv_status() { return status; }
// volume change when ctx has not been created is not possible
void volume_up() {
  const char **cmd;
  pthread_mutex_lock(&mpv_mutex);
  if (ctx && settings->volume + 5 <= 100) {
    cmd = (const char *[]){"add", "volume", "5", NULL};
    settings->volume += 5;
  } else if (ctx && settings->volume + 5 > 100) {
    cmd = (const char *[]){"set", "volume", "100", NULL};
    settings->volume = 100;
  }
  volume_update(settings->volume);
  mpris_ctx->volume = update_mpris_volume(settings->volume);
  mpv_command_async(ctx, 0, cmd);
  pthread_mutex_unlock(&mpv_mutex);
}

void volume_down() {
  const char **cmd;
  pthread_mutex_lock(&mpv_mutex);
  if (ctx && settings->volume - 5 >= 0) {
    cmd = (const char *[]){"add", "volume", "-5", NULL};
    settings->volume -= 5;
  } else if (ctx && settings->volume - 5 < 0) {
    cmd = (const char *[]){"set", "volume", "0", NULL};
    settings->volume = 0;
  }
  volume_update(settings->volume);
  mpris_ctx->volume = update_mpris_volume(settings->volume);
  mpv_command_async(ctx, 0, cmd);
  pthread_mutex_unlock(&mpv_mutex);
}

void set_volume(int volume) {
  pthread_mutex_lock(&mpv_mutex);
  if (ctx && (volume >= 0 && volume <= 100)) {
    char vol_str[16];
    snprintf(vol_str, sizeof(vol_str), "%d", volume);
    const char *cmd[] = {"set", "volume", vol_str, NULL};
    mpv_command_async(ctx, 0, cmd);
  }
  volume_update(volume);
  mpris_ctx->volume = update_mpris_volume(volume);
  settings->volume = volume;
  pthread_mutex_unlock(&mpv_mutex);
}

int get_volume() {
  int64_t volume_64 = -1;
  pthread_mutex_lock(&mpv_mutex);
  if (ctx) {
    int result = mpv_get_property(ctx, "volume", MPV_FORMAT_INT64, &volume_64);
    if (result < 0) {
      printf("Failed to get volume: %s\n", mpv_error_string(result));
      volume_64 = -1;
    }
  }
  pthread_mutex_unlock(&mpv_mutex);
  return (int)volume_64;
}

void play_pause() {
  pthread_mutex_lock(&mpv_mutex);
  if (ctx) {
    const char *cmd[] = {"cycle", "pause", NULL};
    mpv_command_async(ctx, 0, cmd);
  }
  pthread_mutex_unlock(&mpv_mutex);
  if (status == MPV_STATUS_PLAYING)
    status = MPV_STATUS_PAUSED;
  else if (status == MPV_STATUS_PAUSED)
    status = MPV_STATUS_PLAYING;

  status_changed = true;
}

void pause_player() {
  pthread_mutex_lock(&mpv_mutex);
  if (ctx) {
    const char *cmd[] = {"set", "pause", "yes", NULL};
    mpv_command_async(ctx, 0, cmd);
  }
  pthread_mutex_unlock(&mpv_mutex);
  status = MPV_STATUS_PAUSED;
  status_changed = true;
}

void play_player() {
  pthread_mutex_lock(&mpv_mutex);
  if (ctx) {
    const char *cmd[] = {"set", "pause", "no", NULL};
    mpv_command_async(ctx, 0, cmd);
  }
  pthread_mutex_unlock(&mpv_mutex);
  status = MPV_STATUS_PLAYING;
  status_changed = true;
}

void shutdown_player() {
  pthread_mutex_lock(&mpv_mutex);
  if (ctx) {
    const char *cmd[] = {"quit", NULL};
    mpv_command_async(ctx, 0, cmd);
  }
  // no need to set ctx to NULL, it happens when exiting the main loop
  pthread_mutex_unlock(&mpv_mutex);
}

void skip_song() {
  pthread_mutex_lock(&mpv_mutex);
  if (ctx) {
    // making it go to 100%
    const char *cmd[] = {"seek", "100", "absolute-percent", NULL};
    mpv_command_async(ctx, 0, cmd);
  }
  pthread_mutex_unlock(&mpv_mutex);
}

void previous_song() {
  Song *tmp, *prev = NULL;
  tmp = queue->songs;
  while (tmp && strcmp(tmp->id, user_selection.playing_song->id) != 0) {
    prev = tmp;
    tmp = tmp->next;
  }
  // the current has been found and there is one before
  if (tmp && prev)
    play_song(prev->id);
}

static inline void check_error(int status) {
  if (status < 0) {
    // TODO: print to error window
    // mpv_error_string(status);
    status = MPV_STATUS_ERROR;
    exit(1);
  }
}
