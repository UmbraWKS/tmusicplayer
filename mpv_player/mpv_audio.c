#include "mpv_audio.h"
#include "../data/data_struct.h"
#include "../files/files.h"
#include "../program_data.h"
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

void *play_queue(void *arg) {
  Queue *queue = (Queue *)arg;
  pthread_mutex_lock(&mpv_mutex);
  status = MPV_STATUS_INITIALIZING;
  ctx = mpv_create();
  if (!ctx) {
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
  // enabling playlist mode in mpv
  check_error(mpv_set_option_string(ctx, "keep-open", "yes"));
  check_error(mpv_set_option_string(ctx, "idle", "yes"));
  // initializing the player
  check_error(mpv_initialize(ctx));
  status = MPV_STATUS_READY;
  pthread_mutex_unlock(&mpv_mutex);
  // loading songs into playlist
  Song *current_song = queue->songs;
  char *params, *url;
  for (int i = 0; i < queue->queue_size; i++) {
    const char *mode = (i == 0) ? "replace" : "append";
    params = song_params(current_song->id);
    url = url_formatter(server, "stream", params);
    const char *cmd[] = {"loadfile", url, mode, NULL};
    check_error(mpv_command(ctx, cmd));
    current_song = current_song->next;
    free(url);
    free(params);
  }
  // listening to property changes in the playslist
  mpv_observe_property(ctx, 0, "playlist-pos", MPV_FORMAT_INT64);
  // listening to time advancement in song
  mpv_observe_property(ctx, 1, "time-pos", MPV_FORMAT_DOUBLE);
  // player loop
  while (1) {
    mpv_event *event = mpv_wait_event(ctx, 1000);

    if (event->event_id == MPV_EVENT_SHUTDOWN) {
      break;
    } else if (event->event_id == MPV_EVENT_PLAYBACK_RESTART) {
      // playback has started
      status = MPV_STATUS_PLAYING;
      status_changed = true;
    } else if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
      mpv_event_property *prop = (mpv_event_property *)event->data;
      if (strcmp(prop->name, "playlist-pos") == 0) {
        if (prop->format == MPV_FORMAT_INT64 && prop->data) {
          int64_t play_pos = *(int64_t *)prop->data;
          currenyly_playing((int)play_pos);
        }
      } else if (strcmp(prop->name, "time-pos") == 0 &&
                 status == MPV_STATUS_PLAYING) {
        if (prop->format == MPV_FORMAT_DOUBLE && prop->data) {
          double time_stamp = *(double *)prop->data;
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

mpv_status_t get_mpv_status() {
  mpv_status_t current_status;
  pthread_mutex_lock(&mpv_mutex);
  current_status = status;
  pthread_mutex_unlock(&mpv_mutex);
  return current_status;
}
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

void pause_toggle() {
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

void shutdown_player() {
  pthread_mutex_lock(&mpv_mutex);
  if (ctx) {
    const char *cmd[] = {"quit", NULL};
    mpv_command_async(ctx, 0, cmd);
  }
  // no need to set ctx to NULL, it happens when exiting the main loop
  pthread_mutex_unlock(&mpv_mutex);
}

static inline void check_error(int status) {
  if (status < 0) {
    printf("mpv API error: %s\n", mpv_error_string(status));
    status = MPV_STATUS_ERROR;
    exit(1);
  }
}
