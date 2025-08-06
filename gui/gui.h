#include <menu.h>
#include <ncurses.h>

#ifndef GUI
#define GUI

#define TOP_BAR_ITEMS 3

typedef enum { STATE_TOP_BAR, STATE_CONTENT_SELECTION } program_state_t;

// different layouts based on the selection in the top bar
// the number of windows to show in the bottom part
typedef enum {
  LAYOUT_THREE_WINDOWS,
  LAYOUT_TWO_WINDOWS,
  LAYOUT_ONE_WINDOW
} layout_type_t;

typedef struct {
  WINDOW *top_bar;    // top bar with the layout selection
  WINDOW *player_bar; // bar containing player info
  int top_bar_height,
      player_bar_height; // the heigth of the top bars, they have max width
  // it contains all the windows at the bottom (max. 3)
  WINDOW
  *content_windows[3];       // since the windows are based on the bar selection
  MENU *content_menus[3];    // the menus contained in the windows
  WINDOW *content_subwin[3]; // pointers to the sub-windows required by the
                             // menus to enble scrolling
  ITEM **menu_items[3];      // the items contained in each menu
  ITEM *selected_item[3];    // the selected item in each of the bottom menus
  int current_top_selection; // the selected option in the top bar
  int current_content_window; // the window selected
  layout_type_t current_layout;
  program_state_t state; // where the user focus is
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
// what the program does when ENTER is pressed
void enter_input_handling();
WINDOW *create_top_bar();
WINDOW *create_player_bar();
// creates the windows in the bottom part that contains menus
void create_content_windows();
// standard menu settings
void set_menu_attributes(int index);
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
void error_window(const char *message);
