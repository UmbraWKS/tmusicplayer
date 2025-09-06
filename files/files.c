#include "files.h"
#include <ctype.h>
#include <json-c/json_object.h>
#include <json-c/json_types.h>

const char *config_file;
const char *settings_file;

int read_server_data(Server *server) {
  json_object *obj = json_object_from_file(config_file);
  if (obj == NULL)
    return -1;

  json_object *host, *name, *password;

  json_object_object_get_ex(obj, "host", &host);
  json_object_object_get_ex(obj, "name", &name);
  json_object_object_get_ex(obj, "password", &password);

  // duping because when the function ends the contents of the
  // local var will be voided, the pointer would become garbage or null
  server->host = strdup(json_object_get_string(host));
  server->name = strdup(json_object_get_string(name));
  server->password = strdup(json_object_get_string(password));

  json_object_put(obj);

  return 0;
}

int read_settings(Settings *settings) {
  json_object *obj = json_object_from_file(settings_file);
  if (obj == NULL)
    return -1;

  json_object *volume, *playlist_loop;

  json_object_object_get_ex(obj, "volume", &volume);
  json_object_object_get_ex(obj, "playlist-loop", &playlist_loop);

  settings->volume = json_object_get_int(volume);
  int tmp = json_object_get_int(playlist_loop);
  settings->loop = (loop_status_t)tmp;

  json_object_put(obj);

  return 0;
}

int app_path_validation() {
  char data_path[1024];
  char settings_path[1024];
  char config_path[1024];
  int res;

  if (get_actual_path(data_path, sizeof(data_path)) == -1)
    return -1;

  if (!folder_exists(data_path))
    if (!create_folder(data_path))
      return -1; // error when creating the folder

  // snprintf returns the space left on the array, or the space needed to
  // perform the operation if the array is too small
  res = snprintf(settings_path, sizeof(settings_path), "%s/%s", data_path,
                 SETTINGS_FILE_NAME);
  if (res >= sizeof(settings_path))
    return -1;

  if (!file_exists(settings_path))
    create_settings_file(settings_path);

  settings_file = strdup(settings_path);

  res = snprintf(config_path, sizeof(config_path), "%s/%s", data_path,
                 SERVER_DATA_FILE);
  if (res >= sizeof(config_path))
    return -1;

  if (!file_exists(config_path))
    return -2; // if the config file doesnt exist an error message must be
               // displayied
  config_file = strdup(config_path);

  return 0;
}

int get_actual_path(char *path, size_t path_size) {
  const char *home = getenv("HOME");

  int result = snprintf(path, path_size, "%s/%s", home, SERVER_DATA_PATH);
  if (result >= path_size)
    return -1;

  return 0;
}

bool folder_exists(const char *path) {
  struct stat st;
  return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

bool create_folder(const char *path) { return mkdir(path, 0755) == 0; }

bool file_exists(const char *path) {
  struct stat st;
  return (stat(path, &st) == 0 && S_ISREG(st.st_mode));
}

void create_settings_file(const char *path) {
  json_object *obj = json_object_new_object();
  json_object_object_add(obj, "volume", json_object_new_int(50));
  json_object_object_add(obj, "playlist-loop", json_object_new_boolean(false));
  json_object_to_file(path, obj);
  json_object_put(obj);
}

void save_settings(Settings *settings) {
  json_object *obj = json_object_new_object();
  if (settings->volume >= 0 && settings->volume <= 100)
    json_object_object_add(obj, "volume",
                           json_object_new_int(settings->volume));
  else
    json_object_object_add(obj, "volume",
                           json_object_new_int(50)); // default to 50

  json_object_object_add(obj, "playlist-loop",
                         json_object_new_int((int)settings->loop));

  json_object_to_file(settings_file, obj);
  json_object_put(obj);
}

void sanitize_strings(char *s) {
  if (s == NULL)
    return;

  for (size_t i = 0; i < strlen(s); i++) {
    if (!isprint((unsigned char)s[i])) {
      s[i] = '?'; // replacing char
    }
  }
}

MusicFolder *parse_music_folders(const char *xml_string) {
  /*
      <subsonic-response>
          <musicFolders>
              <musicFolder>
  */
  xmlDocPtr doc;
  xmlNodePtr root, node, folder_node;
  MusicFolder *folder = NULL;

  doc = xmlReadMemory(xml_string, strlen(xml_string), "file.xml", NULL, 0);
  if (doc == NULL)
    return NULL;

  root = xmlDocGetRootElement(doc);
  if (root == NULL) {
    xmlFreeDoc(doc);
    return NULL;
  }

  for (node = root->children; node; node = node->next) {
    if (xmlStrcmp(node->name, (const xmlChar *)"musicFolders") == 0) {
      for (folder_node = node->children; folder_node;
           folder_node = folder_node->next) {
        if (xmlStrcmp(folder_node->name, (const xmlChar *)"musicFolder") == 0) {
          MusicFolder *folder_dir = calloc(1, sizeof(MusicFolder));
          folder_dir->next = NULL;
          xmlChar *tmp;

          tmp = xmlGetProp(folder_node, (const xmlChar *)"id");
          folder_dir->id = tmp ? strdup((char *)tmp) : NULL;
          xmlFree(tmp);

          tmp = xmlGetProp(folder_node, (const xmlChar *)"name");
          folder_dir->name = tmp ? strdup((char *)tmp) : NULL;
          sanitize_strings(folder_dir->name);
          xmlFree(tmp);

          folder = add_folder_to_list(folder, folder_dir);
        }
      }
    }
  }

  xmlFreeDoc(doc);
  xmlCleanupParser();
  return folder;
}

Artist *parse_artists(const char *xml_string) {
  /*
      <subsonic-response>
          <indexes>
              <index>
                  <artist>

  */
  xmlDocPtr doc;
  xmlNodePtr root, indexes, index_node, artist_node;
  Artist *artist = NULL;

  doc = xmlReadMemory(xml_string, strlen(xml_string), "file.xml", NULL, 0);
  if (doc == NULL)
    return NULL;

  root = xmlDocGetRootElement(doc);
  if (root == NULL) {
    xmlFreeDoc(doc);
    return NULL;
  }

  for (indexes = root->children; indexes; indexes = indexes->next) {
    if (xmlStrcmp(indexes->name, (const xmlChar *)"indexes") == 0) {
      for (index_node = indexes->children; index_node;
           index_node = index_node->next) {
        if (xmlStrcmp(index_node->name, (const xmlChar *)"index") == 0) {
          xmlChar *tmp;
          for (artist_node = index_node->children; artist_node;
               artist_node = artist_node->next) {
            if (xmlStrcmp(artist_node->name, (const xmlChar *)"artist") == 0) {
              Artist *a = calloc(1, sizeof(Artist));
              a->next = NULL;
              a->albums_dir = NULL;

              tmp = xmlGetProp(artist_node, (const xmlChar *)"id");
              a->id = tmp ? strdup((char *)tmp) : NULL;
              xmlFree(tmp);

              tmp = xmlGetProp(artist_node, (const xmlChar *)"name");
              a->name = tmp ? strdup((char *)tmp) : NULL;
              sanitize_strings(a->name);
              xmlFree(tmp);

              tmp = xmlGetProp(artist_node, (const xmlChar *)"coverArt");
              a->cover_art = tmp ? strdup((char *)tmp) : NULL;
              xmlFree(tmp);

              tmp = xmlGetProp(artist_node, (const xmlChar *)"artistImageUrl");
              a->image_url = tmp ? strdup((char *)tmp) : NULL;
              xmlFree(tmp);

              artist = add_artist_to_list(artist, a);
            }
          }
        }
      }
    }
  }

  xmlFreeDoc(doc);
  xmlCleanupParser();
  return artist;
}

AlbumsDirectory *parse_albums(const char *xml_string) {
  /*
      <subsonic-response>
          <directory>
              <child> //the album
                  <genres>
                  <artists>
                  <albumArtists>
  */
  xmlDocPtr doc;
  xmlNodePtr root, dir_node, child_node;
  AlbumsDirectory *dir = calloc(1, sizeof(AlbumsDirectory));
  dir->next = NULL;
  dir->albums = NULL;

  doc = xmlReadMemory(xml_string, strlen(xml_string), "file.xml", NULL, 0);
  if (doc == NULL)
    return NULL;

  root = xmlDocGetRootElement(doc);
  if (root == NULL) {
    xmlFreeDoc(doc);
    return NULL;
  }

  for (dir_node = root->children; dir_node; dir_node = dir_node->next) {
    if (xmlStrcmp(dir_node->name, (const xmlChar *)"directory") == 0) {
      xmlChar *tmp;

      tmp = xmlGetProp(dir_node, (const xmlChar *)"id");
      dir->id = tmp ? strdup((char *)tmp) : NULL;
      xmlFree(tmp);

      tmp = xmlGetProp(dir_node, (const xmlChar *)"name");
      dir->name = tmp ? strdup((char *)tmp) : NULL;
      sanitize_strings(dir->name);
      xmlFree(tmp);

      tmp = xmlGetProp(dir_node, (const xmlChar *)"albumCount");
      dir->album_count = tmp ? atoi((char *)tmp) : 0;
      xmlFree(tmp);

      for (child_node = dir_node->children; child_node;
           child_node = child_node->next) {
        if (xmlStrcmp(child_node->name, (const xmlChar *)"child") == 0) {
          Album *album = calloc(1, sizeof(Album));
          album->next = NULL;
          album->songs_dir = NULL;

          tmp = xmlGetProp(child_node, (const xmlChar *)"id");
          album->id = tmp ? strdup((char *)tmp) : NULL;
          xmlFree(tmp);

          tmp = xmlGetProp(child_node, (const xmlChar *)"artist");
          album->artist = tmp ? strdup((char *)tmp) : NULL;
          sanitize_strings(album->artist);
          xmlFree(tmp);

          tmp = xmlGetProp(child_node, (const xmlChar *)"artistId");
          album->artist_id = tmp ? strdup((char *)tmp) : NULL;
          xmlFree(tmp);

          tmp = xmlGetProp(child_node, (const xmlChar *)"genre");
          album->genre = tmp ? strdup((char *)tmp) : NULL;
          xmlFree(tmp);

          tmp = xmlGetProp(child_node, (const xmlChar *)"title");
          album->title = tmp ? strdup((char *)tmp) : NULL;
          sanitize_strings(album->title);
          xmlFree(tmp);

          tmp = xmlGetProp(child_node, (const xmlChar *)"coverArt");
          album->cover_art = tmp ? strdup((char *)tmp) : NULL;
          xmlFree(tmp);

          tmp = xmlGetProp(child_node, (const xmlChar *)"parent");
          album->parent = tmp ? strdup((char *)tmp) : NULL;
          xmlFree(tmp);

          tmp = xmlGetProp(child_node, (const xmlChar *)"duration");
          album->duration = tmp ? atoi((char *)tmp) : 0;
          xmlFree(tmp);

          tmp = xmlGetProp(child_node, (const xmlChar *)"songCount");
          album->song_count = tmp ? atoi((char *)tmp) : 0;
          xmlFree(tmp);

          dir->albums = add_album_to_list(dir->albums, album);
        }
      }
    }
  }

  xmlFreeDoc(doc);
  xmlCleanupParser();
  return dir;
}

SongsDirectory *parse_songs(const char *xml_string) {
  /*
      <subsonic-response>
          <directory>
              <child>
                  <genres>
                  <replayGain>
                  <artists>
                  <albumArtists>
  */
  xmlDocPtr doc;
  xmlNodePtr root, dir_node, child;
  SongsDirectory *dir = calloc(1, sizeof(SongsDirectory));
  dir->next = NULL;
  dir->songs = NULL;

  doc = xmlReadMemory(xml_string, strlen(xml_string), "file.xml", NULL, 0);
  if (doc == NULL)
    return NULL;

  root = xmlDocGetRootElement(doc);
  if (root == NULL) {
    xmlFreeDoc(doc);
    return NULL;
  }

  for (dir_node = root->children; dir_node; dir_node = dir_node->next) {
    if (xmlStrcmp(dir_node->name, (const xmlChar *)"directory") == 0) {
      xmlChar *tmp;

      tmp = xmlGetProp(dir_node, (const xmlChar *)"id");
      dir->id = tmp ? strdup((char *)tmp) : NULL;
      xmlFree(tmp);

      tmp = xmlGetProp(dir_node, (const xmlChar *)"name");
      dir->name = tmp ? strdup((char *)tmp) : NULL;
      sanitize_strings(dir->name);
      xmlFree(tmp);

      tmp = xmlGetProp(dir_node, (const xmlChar *)"parent");
      dir->parent = tmp ? strdup((char *)tmp) : NULL;
      xmlFree(tmp);

      tmp = xmlGetProp(dir_node, (const xmlChar *)"coverArt");
      dir->cover_art = tmp ? strdup((char *)tmp) : NULL;
      xmlFree(tmp);

      tmp = xmlGetProp(dir_node, (const xmlChar *)"playCount");
      dir->play_count = tmp ? atoi((char *)tmp) : 0;
      xmlFree(tmp);

      tmp = xmlGetProp(dir_node, (const xmlChar *)"songCount");
      dir->song_count = tmp ? atoi((char *)tmp) : 0;
      xmlFree(tmp);

      for (child = dir_node->children; child; child = child->next) {
        if (xmlStrcmp(child->name, (const xmlChar *)"child") == 0) {
          Song *song = calloc(1, sizeof(Song));
          song->next = NULL;

          tmp = xmlGetProp(child, (const xmlChar *)"id");
          song->id = tmp ? strdup((char *)tmp) : NULL;
          xmlFree(tmp);

          tmp = xmlGetProp(child, (const xmlChar *)"parent");
          song->parent = tmp ? strdup((char *)tmp) : NULL;
          xmlFree(tmp);

          tmp = xmlGetProp(child, (const xmlChar *)"title");
          song->title = tmp ? strdup((char *)tmp) : NULL;
          sanitize_strings(song->title);
          xmlFree(tmp);

          tmp = xmlGetProp(child, (const xmlChar *)"album");
          song->album = tmp ? strdup((char *)tmp) : NULL;
          sanitize_strings(song->album);
          xmlFree(tmp);

          tmp = xmlGetProp(child, (const xmlChar *)"artist");
          song->artist = tmp ? strdup((char *)tmp) : NULL;
          sanitize_strings(song->artist);
          xmlFree(tmp);

          tmp = xmlGetProp(child, (const xmlChar *)"suffix");
          song->format = tmp ? strdup((char *)tmp) : NULL;
          xmlFree(tmp);

          tmp = xmlGetProp(child, (const xmlChar *)"albumId");
          song->album_id = tmp ? strdup((char *)tmp) : NULL;
          xmlFree(tmp);

          tmp = xmlGetProp(child, (const xmlChar *)"artistId");
          song->artist_id = tmp ? strdup((char *)tmp) : NULL;
          xmlFree(tmp);

          tmp = xmlGetProp(child, (const xmlChar *)"contentType");
          song->content_type = tmp ? strdup((char *)tmp) : NULL;
          xmlFree(tmp);

          tmp = xmlGetProp(child, (const xmlChar *)"year");
          song->year = tmp ? atoi((char *)tmp) : 0;
          xmlFree(tmp);

          tmp = xmlGetProp(child, (const xmlChar *)"duration");
          song->duration = tmp ? atoi((char *)tmp) : 0;
          xmlFree(tmp);

          tmp = xmlGetProp(child, (const xmlChar *)"coverArt");
          song->cover_art = tmp ? strdup((char *)tmp) : NULL;
          xmlFree(tmp);

          tmp = xmlGetProp(child, (const xmlChar *)"bitRate");
          song->bitrate = tmp ? atoi((char *)tmp) : 0;
          xmlFree(tmp);

          tmp = xmlGetProp(child, (const xmlChar *)"samplingRate");
          song->sampling_rate = tmp ? atoi((char *)tmp) : 0;
          xmlFree(tmp);

          dir->songs = add_song_to_list(dir->songs, song);
          // since add_song_to_list duplicates the song i will free the song
          // here
          free(song);
        }
      }
    }
  }

  xmlFreeDoc(doc);
  xmlCleanupParser();
  return dir;
}
