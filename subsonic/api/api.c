#include "api.h"
#include <curl/curl.h>

#define MD5_DIGEST_LENGTH 16

char *salt_generator(int length) {
  char chars[] =
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  int chars_len = sizeof(chars) - 1; // -1 to exclude null terminator
  char *salt = malloc(length + 1);
  if (salt == NULL)
    return NULL;

  for (int i = 0; i < length; i++)
    salt[i] = chars[rand() % chars_len];

  salt[length] = '\0'; // terminate to prevent errors in string operations
  return salt;
}

// input is password+salt
char *hash_md5(const char *input) {
  unsigned char digest[MD5_DIGEST_LENGTH];
  MD5((unsigned char *)input, strlen(input), digest);
  // TODO: MD5 is deprecated, replace the hasing

  char *hashed_string = malloc(33);
  if (hashed_string == NULL)
    return NULL;

  // Converting binary hash to hex string
  for (int i = 0; i < MD5_DIGEST_LENGTH; i++)
    sprintf(&hashed_string[i * 2], "%02x", digest[i]);

  return hashed_string;
}

// callback function to handle the response data from the API call
size_t write_callback(void *ptr, size_t size, size_t nmemb,
                      APIResponse *response) {
  size_t new_len = response->size + size * nmemb;
  response->data = realloc(response->data, new_len + 1);
  if (response->data == NULL)
    return 0;
  /*
      the response gets put in a string, then it has to be handled accordingly
      by the caller based on the contents
  */
  memcpy(response->data + response->size, ptr, size * nmemb);
  response->data[new_len] = '\0'; // null, terminate the string
  response->size = new_len;
  return size * nmemb;
}

CURLcode call_api(const char *url, APIResponse *response, CURL *curl) {
  if (!curl)
    return CURL_ERROR_SIZE;

  // initializing the response(data will be put here in the callback function)
  response->data = malloc(1);
  response->size = 0;

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);

  return curl_easy_perform(curl);
}

char *url_formatter(Server *server, char *function_call,
                    char *additional_params) {
  size_t salt_len = 6;
  char *salt = salt_generator(salt_len);
  if (salt == NULL)
    return NULL;

  size_t pass_len = strlen(server->password);
  size_t total_len = pass_len + salt_len + 1;
  char *pass_salt = malloc(total_len);
  if (pass_salt == NULL)
    return NULL;
  // unite password and salt
  strcpy(pass_salt, server->password);
  strcat(pass_salt, salt);

  // hash pass and salt
  char *hashed = hash_md5(pass_salt);
  if (hashed == NULL) {
    free(salt);
    free(pass_salt);
    return NULL;
  }

  // accounting for all the params + the needed text to make the url at the end,
  // +1 string terminator
  size_t url_len =
      strlen(server->host) + strlen(function_call) + strlen(server->name) +
      strlen(hashed) + strlen(salt) + strlen(API_VER) + strlen(APP_NAME) +
      strlen("/rest/?u=&t=&s=&v=&c=") + strlen(additional_params) + 1;

  char *url = malloc(url_len);
  snprintf(url, url_len, "%s/rest/%s?u=%s&t=%s&s=%s&v=%s&c=%s%s", server->host,
           function_call, server->name, hashed, salt, API_VER, APP_NAME,
           additional_params);

  free(salt);
  free(pass_salt);
  free(hashed);
  return url;
}

char *song_params(char *id) {
  size_t params_size = strlen("&id=") + strlen(id) + 1;
  char *song_url = malloc(params_size);
  snprintf(song_url, params_size, "&id=%s", id);

  return song_url;
}
