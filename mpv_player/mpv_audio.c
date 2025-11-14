#include "mpv_audio.h"
#include "../data/data_struct.h"
#include "../files/files.h"
#include "mpv_callback.h"
#include <curl/curl.h>
#include <mpv/client.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

void handle_eof_event(mpv_event_end_file *eof);
void handle_playback_restart_event();
void handle_time_pos_event(mpv_event_property *prop);
// seeks to the percentage passed via argument values accepted (0-100)
void seek_by_percent(uint8_t n);

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
uint32_t time_passed = 0;
// variable that tracks if the song playing has called scrobble api
bool has_scrobbled = false;
/*
 * a timer that is set to 5 when the user wants to go to the previous song, if
 * pressed again before getting to 0 plays the previous song
 */
uint8_t prev_timer = 0;

APIResponse scrobble_response;

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
  // setting cache limits
  check_error(mpv_set_option_string(ctx, "cache", "yes"));
  check_error(mpv_set_option_string(ctx, "demuxer-max-bytes", "50M"));
  check_error(mpv_set_option_string(ctx, "demuxer-max-back-bytes", "10M"));
  check_error(mpv_set_option_string(ctx, "cache-secs", "15"));
  // initializing the player
  check_error(mpv_initialize(ctx));
  status = MPV_STATUS_READY;
  pthread_mutex_unlock(&mpv_mutex);

  // listening to time advancement in song
  mpv_observe_property(ctx, 1, "time-pos", MPV_FORMAT_DOUBLE);

  mpv_main_loop();

  // manually sending one last status
  status = MPV_STATUS_IDLE;
  playback_status(status);
  return NULL;
}

void handle_eof_event(mpv_event_end_file *eof) {
  if (eof->reason == MPV_END_FILE_REASON_STOP)
    return;
  if (status == MPV_STATUS_CLOSING)
    return;
  // finding the song in the list
  if (settings->loop == TRACK) {
    play_song(user_selection.playing_song->id);
    return;
  }

  Song *tmp = get_song_from_id(queue->songs, user_selection.playing_song->id);
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
}

void handle_playback_restart_event() {
  // song was skipped while status PAUSED
  if (status == MPV_STATUS_PAUSED)
    play_player();

  status = MPV_STATUS_PLAYING;
  status_changed = true;
}

void handle_time_pos_event(mpv_event_property *prop) {
  if (prop->format != MPV_FORMAT_DOUBLE && !prop->data)
    return;
  double time_stamp = *(double *)prop->data;
  /*
    if the user skips 5 or more seconds this prevents the scrobble from
    being called (this happens when a song is skipped because the seek
    100% triggers time-pos) in case of user-determined seek option (if
    ever implemented) this will not affect it, as the call to the api
    will be made the second after.
    */

  bool jumped;

  update_mpris_position(time_stamp);

  // i only care about seconds, so i am limiting function call to second
  // change only
  if ((int)time_stamp != time_passed) {

    jumped = ((int)time_stamp - time_passed) > 5;

    playback_time((int)time_stamp);
    time_passed = (int)time_stamp;

    // reducing the time on the timer every second
    if (prev_timer != 0)
      prev_timer--;
  }

  if (settings->scrobble && !has_scrobbled &&
      (time_passed >=
       (int)(user_selection.song->duration * settings->scrobble_time) / 100) &&
      !jumped) {
    // leaving the param "submission" defaut(true)
    uint64_t time_in_millis = current_time_millis();
    int time_length =
        snprintf(NULL, 0, "%llu", (unsigned long long)time_in_millis);
    size_t params_size = strlen("&id=") +
                         strlen(user_selection.playing_song->id) +
                         strlen("&time=") + time_length + 1;
    char *params = malloc(params_size);
    snprintf(params, params_size, "&id=%s&time=%llu",
             user_selection.playing_song->id,
             (unsigned long long)time_in_millis);
    CURLcode code = call_api(url_formatter(server, "scrobble", params),
                             &scrobble_response, curl);
    has_scrobbled = true;
    free(params);
    if (code != CURLE_OK) {
      // TODO: print to error window
    }
    // TODO: verify APIResponse
  }
}

void mpv_main_loop() {
  while (1) {

    // calling when the status of the song playing changes
    if (status_changed) {
      playback_status(status);
      update_mpris_status(convert_status(status));
      status_changed = false;
    }

    mpv_event *event = mpv_wait_event(ctx, 1000);

    switch (event->event_id) {
    case MPV_EVENT_END_FILE:
      handle_eof_event((mpv_event_end_file *)event->data);
      break;
    case MPV_EVENT_PLAYBACK_RESTART:
      handle_playback_restart_event();
      break;
    case MPV_EVENT_PROPERTY_CHANGE: {
      mpv_event_property *prop = (mpv_event_property *)event->data;
      if (strcmp(prop->name, "time-pos") == 0 && status == MPV_STATUS_PLAYING)
        handle_time_pos_event(prop);
    } break;
    case MPV_EVENT_SHUTDOWN:
      mpv_terminate_destroy(ctx);
      ctx = NULL;
      return;
    default:
      break;
    }
  }
}

void start_new_playback(char *id) {
  // clearing previous playlist
  const char *cmd[] = {"playlist-clear", NULL};
  // playing the first song of the new queue
  play_song(id);
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
    has_scrobbled = false;
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
    status = MPV_STATUS_CLOSING;
    status_changed = true;
    const char *cmd[] = {"quit", NULL};
    mpv_command_async(ctx, 0, cmd);
  }
  pthread_mutex_unlock(&mpv_mutex);
}

void skip_song() {
  // seeking to 100% triggers eof
  seek_by_percent(100);
  // after skip returning back is allowed straight away
  prev_timer = 5;
}

void previous_song() {
  // just set the playback time to 0%
  if (prev_timer == 0) {
    if (!ctx)
      return;

    seek_by_percent(0);
    // setting the timer
    prev_timer = 5;
  } else {
    Song *tmp, *prev = NULL;
    tmp = queue->songs;
    while (tmp && strcmp(tmp->id, user_selection.playing_song->id) != 0) {
      prev = tmp;
      tmp = tmp->next;
    }
    // the current has been found and there is one before
    if (tmp && prev)
      play_song(prev->id);
    // resetting the timer to allow multiple skips easily
    prev_timer = 5;
  }
}

void seek_by_percent(uint8_t n) {
  if (n < 0 || n > 100)
    return;
  if (!ctx)
    return;
  // converting n to string for the command
  char n_str[4];
  snprintf(n_str, sizeof(n_str), "%u", n);

  pthread_mutex_lock(&mpv_mutex);
  const char *cmd[] = {"seek", n_str, "absolute-percent", NULL};
  mpv_command_async(ctx, 0, cmd);
  pthread_mutex_unlock(&mpv_mutex);
}

static inline void check_error(int status) {
  if (status < 0) {
    // TODO: print to error window
    // mpv_error_string(status);
    status = MPV_STATUS_ERROR;
    exit(1);
  }
}

uint64_t current_time_millis() {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}
