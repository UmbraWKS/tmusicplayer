#include "../../data/data_struct.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef API
#define API
#define API_VER "1.16.1"
#define APP_NAME "tmusicplayer"

/* definition in player.c
  the global CURL for api calls */
extern CURL *curl;

typedef struct Server {
  char *host;
  char *name;
  char *password;
} Server;

extern Server *server;

typedef struct APIResponse {
  // raw xml file response
  char *data;
  size_t size;
} APIResponse;

#endif

char *salt_generator(int length);
char *hash_md5(const char *input);
size_t write_callback(void *ptr, size_t size, size_t nmemb,
                      APIResponse *response);
CURLcode call_api(const char *url, APIResponse *response, CURL *curl);

// additional params must contain the whole url content, ex: "&musicFolderId=1"
// if no additional params are needed an empty string should be passed, (NOT
// NULL!!)
char *url_formatter(Server *server, char *function_call,
                    char *additional_params);

// returns the params to pass to url_formatter for the song to play
//  it reqquires the song id
char *song_params(char *id);
/*
 * calls the api to get a the songs in a passed Album
 * return NULL if there is an error in the api call
 */
APIResponse *get_songs_from_server(Album *album);
