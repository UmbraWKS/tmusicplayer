#include "files/files.h"
#include "gui/gui.h"
#include "mpv_player/mpv_audio.h"
#include "subsonic/api/api.h"
#include <curl/curl.h>
#include <curl/easy.h>
#include <curses.h>
#include <locale.h>
#include <ncurses.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>

#define APP_VERSION 0.7

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

int main() {
  setlocale(LC_ALL, "");
  setlocale(LC_NUMERIC, "C");
  // initializing program data
  init_curl();
  srand(time(NULL));
  library = calloc(1, sizeof(MusicLibrary));

  init_curses();

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

  mpris_init();
  // starting the main tui
  init_main_tui();

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

    mpris_process();
  }

  mpris_free();
  save_settings(settings);
  free(settings);
  free_server();
  free(server);
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

// getting the music folders and making the user choose one
// return true if execution was successful, false if there were errors
bool get_data_from_server() {
  APIResponse *response = calloc(1, sizeof(APIResponse));
  char *url = url_formatter(server, "getMusicFolders", "");
  // TODO: add check for ResponseAPI after calls

  CURLcode call_code = call_api(url, response, curl);
  if (call_code != CURLE_OK) {
    free(response->data);
    free(response);
    error_window("Encountered error while retrieving data from server");
    return false;
  }
  free(url);
  MusicFolder *folder = parse_music_folders(response->data);
  free(response->data);
  free(response);
  library->folder_list = folder;

  library->folders_count = count_folders(library->folder_list);
  library->selected_folder = 0;
  if (library->folders_count > 1)
    library->selected_folder = music_folder_window();
  if (library->selected_folder == -1)
    return false;

  return true;
}

// server is created using strdup, the data must be free'd
void free_server() {
  free(server->host);
  free(server->name);
  free(server->password);
}
