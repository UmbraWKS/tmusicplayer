#include "../data/data_struct.h"
#include "../subsonic/api/api.h"
#include <json-c/json.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifndef FILES
#define FILES

typedef enum { NONE, QUEUE, TRACK } loop_status_t;

#define SERVER_DATA_PATH ".config/tmusicplayer"
#define SERVER_DATA_FILE "config.json"
#define SETTINGS_FILE_NAME "settings.json"

// variables storing the actual path of the files at runtime
extern const char *config_file;
extern const char *settings_file;

typedef struct Settings {
  /* default values in files.c */
  uint8_t volume;
  loop_status_t loop;
  bool scrobble;
  // the percentage of playtime at which scrobble api call is made
  uint8_t scrobble_time;
} Settings;

extern Settings *settings;
#endif

// reading the json file containing server connection data
int read_server_data(Server *server);
int read_settings(Settings *settings);
/*
checks if all folders and files exist, creates them if they don't
exept for the server data obs, this function must be called before
read_server_data and read_settings
*/
int app_path_validation();

// transforms the $HOME system variable into the actual path
int get_actual_path(char *path, size_t path_size);

bool folder_exists(const char *path);
bool create_folder(const char *path);
bool file_exists(const char *path);
void create_settings_file(const char *path);
void save_settings(Settings *settings);

MusicFolder *parse_music_folders(const char *xml_string);
Artist *parse_artists(const char *xml_string);
AlbumsDirectory *parse_albums(const char *xml_string);
SongsDirectory *parse_songs(const char *xml_string);
// since ITEMS can't handle non ASCII characther it is mandatory to sanitize the
// strings to make them printable
void sanitize_strings(char *s);
