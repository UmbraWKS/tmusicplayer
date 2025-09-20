#include "data_struct.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

Artist *add_artist_to_list(Artist *head, Artist *artist) {
  if (head == NULL)
    return artist;

  Artist *temp = head;
  while (temp->next != NULL)
    temp = temp->next;

  temp->next = artist;
  return head;
}

Album *add_album_to_list(Album *head, Album *album) {
  if (head == NULL)
    return album;

  Album *temp = head;
  while (temp->next != NULL)
    temp = temp->next;

  temp->next = album;
  return head;
}

Song *add_song_to_list(Song *head, Song *song) {
  if (head == NULL)
    return song;

  Song *temp = head;
  while (temp->next != NULL)
    temp = temp->next;

  temp->next = song;
  return head;
}

MusicFolder *add_folder_to_list(MusicFolder *head, MusicFolder *node) {
  if (head == NULL)
    return node;

  MusicFolder *temp = head;
  while (temp->next != NULL)
    temp = temp->next;

  temp->next = node;
  return head;
}

Song *remove_song_from_list(Song *head, const char *id) {
  if (head == NULL)
    return NULL;

  Song *tmp = NULL;
  if (strcmp(head->id, id) == 0) {
    tmp = head;
    head = head->next;
    free(tmp);
  } else {
    Song *prev = NULL;
    Song *curr = head;

    while (curr && strcmp(curr->id, id) != 0) {
      prev = curr;
      curr = curr->next;
    }
    if (curr) {
      tmp = curr;
      prev->next = curr->next;
      free_song(tmp);
    }
  }

  return head;
}

int count_songs(Song *head) {
  if (head == NULL)
    return 0;

  Song *tmp = head;
  int count = 0;
  while (tmp != NULL) {
    tmp = tmp->next;
    count++;
  }
  return count;
}

Song *get_song_from_id(Song *head, const char *id) {
  if (head == NULL)
    return NULL;
  Song *tmp = head;
  while (tmp && strcmp(tmp->id, id) != 0)
    tmp = tmp->next;

  if (tmp)
    return tmp;

  return NULL;
}

Song *get_song_from_pos(Song *head, int pos) {
  if (head == NULL)
    return NULL;
  Song *tmp = head;
  for (int i = 0; i < pos && tmp; i++)
    tmp = tmp->next;

  if (tmp)
    return tmp;

  return NULL;
}

Artist *get_artist_from_pos(Artist *head, int pos) {
  if (head == NULL)
    return NULL;
  Artist *tmp = head;
  for (int i = 0; i < pos && tmp; ++i)
    tmp = tmp->next;

  if (tmp)
    return tmp;

  return NULL;
}

Album *get_album_from_pos(Album *head, int pos) {
  if (head == NULL)
    return NULL;
  Album *tmp = head;
  for (int i = 0; i < pos && tmp; ++i)
    tmp = tmp->next;

  if (tmp)
    return tmp;

  return NULL;
}

bool is_song_present(Song *head, const char *id) {
  if (head == NULL)
    return false;

  Song *s = head;
  while (s && strcmp(s->id, id) != 0)
    s = s->next;

  if (s) // s not null song alredy present
    return true;

  return false;
}

Song *duplicate_song(Song *song) {
  if (!song)
    return NULL;

  Song *dup = calloc(1, sizeof(Song));
  if (!dup)
    return NULL;

  dup->id = song->id ? strdup(song->id) : NULL;
  dup->title = song->title ? strdup(song->title) : NULL;
  dup->album_id = song->album_id ? strdup(song->album_id) : NULL;
  dup->artist_id = song->artist_id ? strdup(song->artist_id) : NULL;
  dup->album = song->album ? strdup(song->album) : NULL;
  dup->artist = song->artist ? strdup(song->artist) : NULL;
  dup->content_type = song->content_type ? strdup(song->content_type) : NULL;
  dup->format = song->format ? strdup(song->format) : NULL;
  dup->cover_art = song->cover_art ? strdup(song->cover_art) : NULL;
  dup->duration = song->duration;
  dup->bitrate = song->bitrate;
  dup->year = song->year;
  dup->sampling_rate = song->sampling_rate;
  dup->parent = song->parent ? strdup(song->parent) : NULL;
  dup->next = NULL;

  return dup;
}

SongsDirectory *duplicate_songs_dir(SongsDirectory *songs_dir) {
  if (!songs_dir)
    return NULL;

  SongsDirectory *dup = calloc(1, sizeof(SongsDirectory));
  if (!dup)
    return NULL;

  dup->id = songs_dir->id ? strdup(songs_dir->id) : NULL;
  dup->name = songs_dir->name ? strdup(songs_dir->name) : NULL;
  dup->parent = songs_dir->parent ? strdup(songs_dir->parent) : NULL;
  dup->play_count = songs_dir->play_count;
  dup->cover_art = songs_dir->cover_art ? strdup(songs_dir->cover_art) : NULL;
  dup->song_count = songs_dir->song_count;
  dup->songs = NULL;

  return dup;
}

Album *duplicate_album(Album *album) {
  if (!album)
    return NULL;

  Album *dup = calloc(1, sizeof(Album));
  if (!dup)
    return NULL;

  dup->id = album->id ? strdup(album->id) : NULL;
  dup->artist_id = album->artist_id ? strdup(album->artist_id) : NULL;
  dup->artist = album->artist ? strdup(album->artist) : NULL;
  dup->title = album->title ? strdup(album->title) : NULL;
  dup->cover_art = album->cover_art ? strdup(album->cover_art) : NULL;
  dup->year = album->year;
  dup->genre = album->genre ? strdup(album->genre) : NULL;
  dup->song_count = album->song_count;
  dup->duration = album->duration;
  dup->parent = album->parent ? strdup(album->parent) : NULL;
  dup->songs_dir = NULL;
  dup->next = NULL;

  return dup;
}

AlbumsDirectory *duplicate_albums_dir(AlbumsDirectory *albums_dir) {
  if (!albums_dir)
    return NULL;

  AlbumsDirectory *dup = calloc(1, sizeof(AlbumsDirectory));
  if (!dup)
    return NULL;

  dup->id = albums_dir->id ? strdup(albums_dir->id) : NULL;
  dup->name = albums_dir->name ? strdup(albums_dir->name) : NULL;
  dup->album_count = albums_dir->album_count;
  dup->albums = NULL;

  return dup;
}

Artist *duplicate_artist(Artist *artist) {
  if (!artist)
    return NULL;

  Artist *dup = calloc(1, sizeof(Artist));
  if (!dup)
    return NULL;

  dup->id = artist->id ? strdup(artist->id) : NULL;
  dup->cover_art = artist->cover_art ? strdup(artist->cover_art) : NULL;
  dup->image_url = artist->image_url ? strdup(artist->image_url) : NULL;
  dup->name = artist->name ? strdup(artist->name) : NULL;
  dup->albums_dir = NULL;
  dup->next = NULL;

  return dup;
}

uint16_t count_folders(MusicFolder *folder) {
  if (!folder)
    return 0;
  MusicFolder *tmp = folder;
  int count = 0;
  while (tmp) {
    count++;
    tmp = tmp->next;
  }
  return count;
}

uint32_t count_artists(Artist *artist) {
  if (!artist)
    return 0;
  Artist *tmp = artist;
  uint32_t counter = 0;
  while (tmp != NULL) {
    counter++;
    tmp = tmp->next;
  }
  return counter;
}

Artist *get_artist_from_id(Artist *head, const char *id) {
  if (!head || !id)
    return NULL;
  Artist *tmp = head;
  while (tmp != NULL && strcmp(tmp->id, id) != 0)
    tmp = tmp->next;
  return tmp;
}

Album *get_album_from_id(Album *head, const char *id) {
  if (!head || !id)
    return NULL;
  Album *tmp = head;
  while (tmp != NULL && strcmp(tmp->id, id) != 0)
    tmp = tmp->next;
  return tmp;
}

void free_song(Song *song) {
  if (song == NULL)
    return;
  free(song->id);
  free(song->title);
  free(song->album_id);
  free(song->album);
  free(song->cover_art);
  free(song->artist);
  free(song->content_type);
  free(song->artist_id);
  free(song->parent);
  free(song->format);
  free(song);
}

void free_song_list(Song *song) {
  if (song != NULL)
    free_song_list(song->next);

  free_song(song);
}

void free_album(Album *album) {
  if (album == NULL)
    return;
  free(album->id);
  free(album->artist_id);
  free(album->artist);
  free(album->title);
  free(album->cover_art);
  free(album->genre);
  free(album->parent);
  free(album);
}

void free_album_list(Album *album) {
  if (album != NULL)
    free_album_list(album->next);

  free_album(album);
}

void free_songs_dir(SongsDirectory *dir) {
  if (dir == NULL)
    return;
  free(dir->id);
  free(dir->name);
  free(dir->parent);
  free(dir->cover_art);
  free(dir);
}
void free_albums_dir(AlbumsDirectory *dir) {
  if (dir == NULL)
    return;
  free(dir->id);
  free(dir->name);
  free(dir);
}

void free_artist(Artist *artist) {
  if (artist == NULL)
    return;
  free(artist->id);
  free(artist->name);
  free(artist->image_url);
  free(artist->cover_art);
  free(artist);
}

void free_artist_list(Artist *artist) {
  if (artist != NULL)
    free_artist_list(artist->next);

  free_artist(artist);
}

void free_music_folder(MusicFolder *folder) {
  if (folder == NULL)
    return;
  free(folder->id);
  free(folder->name);
  free(folder);
}

void free_music_folder_list(MusicFolder *folder) {
  if (folder != NULL)
    free_music_folder_list(folder->next);

  free_music_folder(folder);
}

void free_library(MusicLibrary *library) {
  for (MusicFolder *folder = library->folder_list; folder != NULL;
       folder = folder->next) {
    for (Artist *a = folder->artists; a != NULL; a = a->next) {
      if (a->albums_dir != NULL) {
        for (Album *album = a->albums_dir->albums; album != NULL;
             album = album->next) {
          if (album->songs_dir != NULL) {
            free_song_list(album->songs_dir->songs);
            free_songs_dir(album->songs_dir);
          }
        }
        free_album_list(a->albums_dir->albums);
        free_albums_dir(a->albums_dir);
      }
    }
    free_artist_list(folder->artists);
  }
  free_music_folder_list(library->folder_list);
  free(library);
}
