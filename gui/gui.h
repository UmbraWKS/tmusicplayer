#include "../data/data_struct.h"
#include <menu.h>
#include <ncurses.h>

#ifndef GUI
#define GUI

#define TOP_BAR_ITEMS 3
#define TOP_BAR_HEIGHT 3
#define PLAYER_BAR_HEIGHT 5

#define ARTIST_NAVIGATION_WINDOWS 3
#define PLAYLIST_NAVIGATION_WINDOWS 2
#define QUEUE_NAVIGATION_WINDOWS 1

// different layouts based on the selection in the top bar
// the number of windows to show in the bottom part
typedef enum {
  LAYOUT_ARTIST_NAVIGATION = 0,
  LAYOUT_PLAYLIST_NAVIGATION = 1,
  LAYOUT_QUEUE_NAVIGATION = 2
} layout_type_t;

/* a panel contains the data of a single window in the bottom part of the screen
 */
typedef struct {
  WINDOW
  *window;             // the main window
  MENU *menu;          // the menu contained in the window
  WINDOW *subwin;      // sub-window inside the main window required by the
                       // menus to enble scrolling
  ITEM **menu_items;   // the items contained in each menu
  ITEM *selected_item; // the selected item in each of the bottom menus
} content_panel_t;

typedef struct {
  WINDOW *top_bar;    // top bar with the layout selection
  WINDOW *player_bar; // bar containing player info
  // it contains all the windows at the bottom (max. 3)

  content_panel_t *panels;
  int panels_count;

  int current_content_window; // the window selected
  layout_type_t current_layout;
  int screen_height, screen_width;
  int windows_width, windows_height; // the width and height of the dynamic
                                     // windows at the bottom

} layout_manager_t;

#endif

// initializes ncurses and creates the default view
void ncurses_init();
// user input control
void handle_input(int ch);
/* important to unpost the menu and free the items before calling the populate
 since the number of items inside the menu could change i prefer to delete the
 menu and create it from scrath every time intead of risking memory leaks*/
void populate_artists_window();
void populate_albums_window();
void populate_songs_window();
void populate_queue_window();
// what the program does when ENTER is pressed
void enter_input_handling();
WINDOW *create_top_bar();
WINDOW *create_player_bar();
// creates the windows in the bottom part that contains menus
void create_content_windows();
// standard menu settings
void set_menu_attributes(content_panel_t *panel);
// removes and nulls the items, menus and windows of the panels passed
void remove_panel_windows(int panels);
// unposts menu and delests items safely
void remove_menu(int index);
// ends and restarts ncurses, re-creates the windows but keeps the items data
// and restores user position
void refresh_screen();
// removes all content in the list of items of specified menu
void remove_items(int index);
// converts seconds to a string mm:ss or m:ss
char *format_time(int seconds);
// shows the song total duration on player bar
void song_time(int time);
// show the passed error message, the user can only press q to close the
// program
// moving the cursor in the menu to the desired position
void set_cursor_on_song_menu(content_panel_t *panel, Song *list, char *target);
void error_window(const char *message);
