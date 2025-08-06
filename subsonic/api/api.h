#include <curl/curl.h>
#include <openssl/md5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef API
#define API
#define API_VER "1.16.1"
#define APP_NAME "tmusicplayer"

typedef struct Server {
  char *host;
  char *name;
  char *password;
} Server;

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
