#include "data_struct.h"
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
  Song *new_song = duplicate_song(song);
  if (!new_song)
    return NULL;

  if (head == NULL)
    return new_song;

  Song *temp = head;
  while (temp->next != NULL)
    temp = temp->next;

  temp->next = new_song;
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
      free(tmp);
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

void free_song_list(Song *head) {
  while (head) {
    Song *tmp = head;
    head = head->next;
    free(tmp);
  }
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
  Song *new_song = malloc(sizeof(Song));
  if (!new_song)
    return NULL;
  *new_song = *song;
  new_song->next = NULL;
  return new_song;
}

Artist *duplicate_artist(Artist *artist) {
  Artist *new_artist = malloc(sizeof(Artist));
  if (!new_artist)
    return NULL;
  *new_artist = *artist;
  new_artist->next = NULL;
  return new_artist;
}
Album *duplicate_album(Album *album) {
  Album *new_album = malloc(sizeof(Album));
  if (!new_album)
    return NULL;
  *new_album = *album;
  new_album->next = NULL;
  return new_album;
}
