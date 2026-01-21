/*
    callback functions that update the ui called by events in the mpv player
*/
#ifndef MPV_CALLBACK
#define MPV_CALLBACK
#endif

#include "../data/data_struct.h"
#include "mpv_audio.h"

// gets passed the id of the song in the queue that is playing
// called every time the song changes
void currently_playing(Song *song);
// gets passed the playback status (PAUSED, PLAYING, IDLE)
// called only when one of these is changes, it's for ui only
void playback_status(mpv_status_t status);
// gets passed the time played of the song
void playback_time(int time);
// updates the ui when the volume changes
void volume_update(int volume);
// updates ui loop status, deletes previous and prints
void update_loop_status_ui();
