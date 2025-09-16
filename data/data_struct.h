#include <stdbool.h>
#include <stdint.h>

#ifndef DATA_STRUCT
#define DATA_STRUCT

/* DATA STRUCTURE
    MusicFolder(constains the macro groups (music, podcast ecc.))
        -> Artist(constains the artists)
            ->AlbumsDirectory(it's the Artist folder, contains the Albums)
                ->Album
                    ->SongsDirectory(it's the Album folder, contains the Songs)
                        ->Song
*/
/* APIS CALLS ORDER
    getMusicFolder() - data in folder_list, returns the libraries
    getIndexes(folder_id) - returns the Index with the artists inside
    (I won't create a Index struct, because it's return is useless, i will just
   put the Artists in) getMusicDirectory(artist_id) - returns artist's
   AlbumsDirecory and Album getMusicDirectory(album_id) - returns SongsDirectory
   and Song
*/

typedef struct SongsDirectory {
  char *id; // id of Album
  char *name;
  char *parent; // id of AlbumDirectory
  int play_count;
  char *cover_art;
  int song_count;

  struct Song *songs;
  struct SongsDirectory *next; // a directory is based on an album
} SongsDirectory;

typedef struct AlbumsDirectory {
  char *id;   // id of Artist
  char *name; // name of the Artist
  int album_count;

  struct Album *albums;
  struct AlbumsDirectory *next;
} AlbumsDirectory;

typedef struct Artist {
  char *id;
  char *cover_art;
  char *image_url;
  char *name;
  struct Artist *next;
  struct AlbumsDirectory *albums_dir; // one artists has only 1 album_dir,
                                      // as this is only an abstraction
} Artist;

typedef struct Album {
  char *id;
  char *artist_id;
  char *artist;
  char *title;
  char *cover_art;
  int year;
  char *genre;
  int song_count;
  int duration;
  char *parent; // id of Artist
  struct Album *next;
  struct SongsDirectory *songs_dir; // one Album has only 1 songs_dir
} Album;

typedef struct Song {
  char *id;
  char *title;
  char *album_id;  // id of Album
  char *artist_id; // Artist id
  char *album;
  char *artist;
  char *content_type;
  char *format;    // mp3/...
  char *cover_art; // id of the cover_art
  int duration;    // seconds
  int bitrate;
  int year;
  int sampling_rate;
  char *parent; // id of Album
  struct Song *next;
} Song;

typedef struct MusicFolder {
  char *id;
  char *name;
  int artists_count;
  struct MusicFolder *next;
  Artist *artists; // list of the artists in a folder
} MusicFolder;

typedef struct MusicLibrary {
  MusicFolder *folder_list;
  int16_t folders_count;
  // index of the selected folder
  int16_t selected_folder;

} MusicLibrary;

extern MusicLibrary *library;

typedef struct Queue {
  int queue_size; // number of songs
  Song *songs;    // list of songs in the queue
} Queue;

extern Queue *queue;

// the user selected ITEMS
typedef struct {
  /* ITEMS selected in Artist view */
  Artist *artist;
  Album *album;
  Song *song;
  /* Song currently playing */
  Song *playing_song;

  // TODO: add playlist

} user_selection_t;

#endif

#ifndef PROGRAM_DATA
#define PROGRAM_DATA

extern user_selection_t user_selection;
extern bool program_exit;

#endif

/* these functions add the item at the end of the list*/
MusicFolder *add_folder_to_list(MusicFolder *head, MusicFolder *node);
Artist *add_artist_to_list(Artist *head, Artist *artist);
Album *add_album_to_list(Album *head, Album *album);
Song *add_song_to_list(Song *head, Song *song);
// removes the song matching the id from the list and returns the new list
Song *remove_song_from_list(Song *head, const char *id);
// counts the songs in the list
int count_songs(Song *head);
// counts the musicFolders in a list
uint16_t count_folders(MusicFolder *folder);
// given a list of songs frees all the elements
void free_song_list(Song *head);
// given a list of songs and an id it returns the song of the corresponding id
// in the list
Song *get_song_from_id(Song *head, const char *id);
// given a list of songs and a position in the list returns the song
// in the position
Song *get_song_from_pos(Song *head, int pos);
// given a list of artists and a position in the list returns the artist at the
// position
Artist *get_artist_from_pos(Artist *head, int pos);
// given a list of Albums and a postion in the list returns the album in the
// position
Album *get_album_from_pos(Album *head, int pos);
// given a list of songs and an id returns true if the song with the
// corresponding is present in the list, false otherwise
bool is_song_present(Song *head, const char *id);
/* functions that duplicate objects */
Song *duplicate_song(Song *song);
Artist *duplicate_artist(Artist *artist);
Album *duplicate_album(Album *album);
