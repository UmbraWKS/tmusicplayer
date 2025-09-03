#include "files/files.h"
#include "glib.h"
#include "gui/gui.h"
#include "mpv_player/mpv_audio.h"
#include "subsonic/api/api.h"
#include <curl/curl.h>
#include <curl/easy.h>
#include <curses.h>
#include <gio/gio.h>
#include <locale.h>
#include <ncurses.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#define APP_VERSION 0.4

APIResponse response;

MusicLibrary *library = NULL;
Server *server = NULL;
Settings *settings = NULL;
bool program_exit = false;
Queue *queue;
user_selection_t user_selection;

CURL *curl = NULL;

bool get_data_from_server();
void program_loop();
void free_server();
void init_curl();

static pthread_t dbus_thread;
static GMainLoop *main_dbus_loop;
void *dbus_thread_func(void *arg);

int main() {
  setlocale(LC_ALL, "");
  setlocale(LC_NUMERIC, "C");
  // initializing program data
  init_curl();
  srand(time(NULL));
  library = malloc(sizeof(MusicLibrary));
  int result; // functions return
  result = app_path_validation();
  if (result == -1) {
    error_window("Encountered error initializing storage paths and files");
    return -1;
  } else if (result == -2) {
    error_window("The Config file doesnt exist, can't connect to the server");
    return -2; // closing with error
  }
  server = malloc(sizeof(Server));
  result = read_server_data(server);
  if (result == -1) {
    error_window("Encountered error while reading the config file, check if "
                 "the format is correct");
    return -1;
  }

  settings = calloc(1, sizeof(Settings));
  read_settings(settings);
  if (!get_data_from_server()) {
    curl_global_cleanup();
    endwin();
    return 1;
  }

  mpris_ctx = init_mpris_player();

  /* GBUS main loop */
  pthread_create(&dbus_thread, NULL, dbus_thread_func, NULL);

  // initializing ncurses
  ncurses_init();

  program_loop();

  curl_global_cleanup();
  endwin();
  return 0;
}

void program_loop() {
  int ch;
  timeout(16);
  while (!program_exit) {
    ch = getch();
    if (ch != ERR) // key was pressed
      handle_input(ch);
  }

  save_settings(settings);
  free(settings);
  free_server();
  free(server);
  g_main_loop_quit(main_dbus_loop);
  g_main_loop_unref(main_dbus_loop);
  cleanup_player();
  pthread_join(dbus_thread, NULL);
}

void *dbus_thread_func(void *arg) {
  // All your existing D-Bus setup code here
  main_dbus_loop = g_main_loop_new(NULL, FALSE);

  // D-Bus registration
  mpris_ctx->owner_id =
      g_bus_own_name(G_BUS_TYPE_SESSION, "org.mpris.MediaPlayer2.tmusicplayer",
                     G_BUS_NAME_OWNER_FLAGS_NONE, on_bus_acquired,
                     on_name_acquired, on_name_lost, main_dbus_loop, NULL);

  g_main_loop_run(main_dbus_loop);

  return NULL;
}

void init_curl() {
  curl_global_init(CURL_GLOBAL_DEFAULT);
  curl = curl_easy_init();
  // setting standard options
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
  curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 0L);
  curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
}

// makes api calls to the server and fills structs
// return true if execution was successful, false if there were errors
bool get_data_from_server() {
  char *url = url_formatter(server, "getMusicFolders", "");

  CURLcode call_code = call_api(url, &response, curl);
  if (call_code != CURLE_OK) {
    error_window("Encountered error while retrieving data from server");
    return false;
  }
  free(url);
  MusicFolder *folder = parse_music_folders(response.data);
  free(response.data);
  library->folder_list = folder;

  // i am only considering the first music folder
  // TODO: allow for multiple musicFolders

  // artists
  char *call_param = malloc(strlen("&musicFolderId=") + strlen(folder->id) + 1);
  sprintf(call_param, "&musicFolderId=%s", folder->id);
  url = url_formatter(server, "getIndexes", call_param);
  call_code = call_api(url, &response, curl);
  if (call_code != CURLE_OK) {
    error_window("Encountered error while retrieving data from server");
    return false;
  }
  free(url); // need to handle url like that (can' t do inline call) to prevent
             // memory leaks
  folder->artists = parse_artists(response.data);
  free(call_param);
  free(response.data);

  folder->artists_count = 0;
  for (Artist *a = folder->artists; a; a = a->next) {
    a->albums_dir = NULL;
    // for every artists i get it's AlbumDirectory filled with Albums
    call_param = malloc(strlen("&id=") + strlen(a->id) + 1);
    sprintf(call_param, "&id=%s", a->id);
    url = url_formatter(server, "getMusicDirectory", call_param);
    call_code = call_api(url, &response, curl);
    if (call_code != CURLE_OK) {
      error_window("Encountered error while retrieving data from server");
      return false;
    }
    free(url);
    a->albums_dir = parse_albums(response.data);
    free(call_param);
    free(response.data);
    folder->artists_count++;

    // and for every Album i get it's SongsDirectory filled with Songs
    Album *al = a->albums_dir->albums;
    al->songs_dir = NULL;
    for (int i = 0; i < a->albums_dir->album_count && al != NULL; i++) {
      call_param = malloc(strlen("&id=") + strlen(al->id) + 1);
      sprintf(call_param, "&id=%s", al->id);
      url = url_formatter(server, "getMusicDirectory", call_param);
      call_code = call_api(url, &response, curl);
      if (call_code != CURLE_OK) {
        error_window("Encountered error while retrieving data from server");
        return false;
      }
      free(url);

      al->songs_dir = parse_songs(response.data);

      free(call_param);
      free(response.data);

      al = al->next;
    }
  }
  return true;
}

// server is created using strdup, the data must be free'd
void free_server() {
  free(server->host);
  free(server->name);
  free(server->password);
}
