#include "data_struct.h"
#include <stdio.h>

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
