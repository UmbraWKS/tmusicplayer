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
#define FOLDER_SELECTION_WINDOWS 1

// different layouts based on the selection in the top bar
// the number of windows to show in the bottom part
typedef enum {
  LAYOUT_ARTIST_NAVIGATION = 0,
  LAYOUT_PLAYLIST_NAVIGATION,
  LAYOUT_QUEUE_NAVIGATION
} layout_type_t;

// identificator for panels in Artist navigation
typedef enum {
  ARTISTS_PANEL = 0,
  ALBUMS_PANEL,
  SONGS_PANEL
} artist_nav_panels_t;

// identificator for panels in Playlist navigation
typedef enum { PLAYLISTS_PANEL = 0, P_SONGS_PANEL } playlist_nav_panel_t;

// identificator for panels in Queue navigation
typedef enum { QUEUE_PANEL = 0 } queue_nav_panels_t;

/*
 * a panel contains the data of a menu
 */
typedef struct {
  WINDOW *window;      // the main window
  MENU *menu;          // the menu contained in the window
  WINDOW *subwin;      // sub-window inside the main window required by the
                       // menus to enble scrolling
  ITEM **menu_items;   // the items contained in each menu
  ITEM *selected_item; // the selected item in each of the bottom menus
  int w_width, w_height;
} menu_panel_t;

/*
 * the error window is an input blocking window that shows an error and a static
 * message, the only interaction available is 'q' to close it
 */
typedef struct {
  WINDOW *window;
  const char *message;
  int w_height, w_width;
} error_window_t;

typedef struct {
  WINDOW *top_bar;    // top bar with the layout selection
  WINDOW *player_bar; // bar containing player info
  // it contains all the windows at the bottom (max. 3)

  menu_panel_t *panels;
  int panels_count;

  int current_content_window; // the window selected
  layout_type_t current_layout;
  int screen_height, screen_width;

  error_window_t *error_window;
  /*
   * menu window used to select the musicFolder at
   * startup if the user has more than one
   */
  menu_panel_t *library_selection;

} layout_manager_t;

#endif

// initializes ncurses and layout manager
void init_curses();
// creates the default view
void init_main_tui();
// user input control
void handle_input(int ch);
/* important to unpost the menu and free the items before calling the populate
 since the number of items inside the menu could change i prefer to delete the
 menu and create it from scrath every time intead of risking memory leaks*/
bool populate_artists_window();
bool populate_albums_window();
bool populate_songs_window();
bool populate_queue_window();
// what the program does when ENTER is pressed
void enter_input_handling();
WINDOW *create_top_bar();
WINDOW *create_player_bar();
// creates the windows in the bottom part that contains menus
void create_content_windows();
// standard menu settings
void set_menu_attributes(menu_panel_t *panel);
// removes and nulls the items, menus and windows of the panels passed
void remove_panel_windows(int panels);
// unposts menu and delests items safely
void remove_menu(menu_panel_t *panel);
// ends and restarts ncurses, re-creates the windows but keeps the items data
// and restores user position
void refresh_screen();
// removes all content in the list of items of specified menu
void remove_items(menu_panel_t *panel);
// converts seconds to a string mm:ss or m:ss
void format_time(int seconds, char time[6]);
// shows the song total duration on player bar
void song_time(int time);
// show the passed error message, the user can only press q to close the
// program
// moving the cursor in the menu to the desired position
void set_cursor_on_song_menu(menu_panel_t *panel, Song *list, char *target);
void error_window(const char *message);
/*
 * window used to select the desired musicFolder
 * it has it's unique loop and input determination
 */
int music_folder_window();
// erase text from a window at specified position and lenght of removal
void erase_text(WINDOW *win, int y_pos, int x_pos, size_t lenght);
// prints on the passed window the current loop status
void print_loop_status(WINDOW *win, int y_pos, int x_pos);
