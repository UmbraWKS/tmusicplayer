#include "gui.h"
#include "../data/data_struct.h"
#include "../files/files.h"
#include "../mpv_player/mpv_callback.h"
#include "../program_data.h"
#include <curses.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

const char *top_bar_labels[TOP_BAR_ITEMS] = {"Artists", "Playlists", "Queue"};
const char *three_windows_lables[3] = {"Artists", "Albums", "Songs"};
layout_manager_t manager = {0};
// object passed to mpv_player containing the queue
Queue *queue;
// thread the mpv instance runs on
pthread_t mpv_thread;

/* variables that represent the player status and values inside the player bar*/
char *song_playing = NULL;  // name of the song playing
char *play_status = NULL;   // mpv status
char *play_time = NULL;     // time elapsed playing
char *song_duration = NULL; // total playing song duration
// the volume is taken directly from the Settings

void ncurses_init() {
  queue = calloc(1, sizeof(Queue));
  initscr();
  raw();
  noecho();
  halfdelay(1);
  keypad(stdscr, TRUE);
  curs_set(0);

  getmaxyx(stdscr, manager.screen_height, manager.screen_width);
  manager.current_top_selection = 0;  // default on artists
  manager.current_content_window = 0; // default on Artists view
  manager.current_layout = LAYOUT_THREE_WINDOWS;
  manager.state = STATE_TOP_BAR;
  manager.top_bar_height = 3;
  manager.player_bar_height = 5;
  refresh();

  manager.top_bar = create_top_bar();
  manager.player_bar = create_player_bar();
  create_content_windows();
}

WINDOW *create_top_bar() {
  WINDOW *win = newwin(manager.top_bar_height, manager.screen_width, 0, 0);
  box(win, 0, 0);

  int y = 2;
  for (int i = 0; i < TOP_BAR_ITEMS; i++) {
    if (i == manager.current_top_selection)
      wattron(win, A_REVERSE); // highlighting selection

    mvwprintw(win, 1, y, "%s", top_bar_labels[i]);

    if (i == manager.current_top_selection)
      wattroff(win, A_REVERSE);

    y += strlen(top_bar_labels[i]) + 2;
  }
  wrefresh(win);
  return win;
}

WINDOW *create_player_bar() {
  WINDOW *win = newwin(manager.player_bar_height, manager.screen_width,
                       manager.top_bar_height, 0);
  box(win, 0, 0);

  if (play_status)
    mvwprintw(win, 1, 2, "Status >> %s", play_status);
  else
    mvwprintw(win, 1, 2, "Status >> ");

  if (song_playing)
    mvwprintw(win, 2, 2, "Playing >> %s", song_playing);
  else
    mvwprintw(win, 2, 2, "Playing >> ");

  if (play_time)
    mvwprintw(win, 1, manager.screen_width - 15, "[%s/", play_time);
  else
    mvwprintw(win, 1, manager.screen_width - 15, "[00:00/");

  if (song_duration)
    mvwprintw(win, 1, manager.screen_width - 8, "%s]", song_duration);
  else
    mvwprintw(win, 1, manager.screen_width - 8, "00:00]");

  if (settings && settings->volume)
    mvwprintw(win, 2, manager.screen_width - 15, "[%d%%]", settings->volume);

  wrefresh(win);
  return win;
}
/*
        Creating static windows that will contain the menus
        TODO: create other layouts, currently only TREE_WINDOWS is implemented
*/
void create_content_windows() {
  // starting after the previous bars
  int starty = manager.top_bar_height + manager.player_bar_height;
  manager.windows_height = manager.screen_height - starty;

  switch (manager.current_layout) {
  case LAYOUT_THREE_WINDOWS:
    manager.windows_width = manager.screen_width / 3;

    for (int i = 0; i < 3; i++) {
      int x_pos = i * manager.windows_width;

      // making the windows full size as the menus inside will regulate
      // scrolling
      manager.content_windows[i] =
          newwin(manager.windows_height, manager.windows_width, starty, x_pos);
      wclear(manager.content_windows[i]); // border control
      box(manager.content_windows[i], 0, 0);

      // windows titles and adding the content if menus aren't empty
      if (i == 0) {
        mvwprintw(manager.content_windows[i], 0, 2, "%s",
                  three_windows_lables[0]);
        if (manager.menu_items[0]) {
          set_menu_attributes(0);
          // setting the selected item
          set_current_item(manager.content_menus[0], manager.selected_item[0]);
        }
      } else if (i == 1) {
        mvwprintw(manager.content_windows[i], 0, 2, "%s",
                  three_windows_lables[1]);
        if (manager.menu_items[1]) {
          set_menu_attributes(1);
          set_current_item(manager.content_menus[1], manager.selected_item[1]);
        }
      } else {
        mvwprintw(manager.content_windows[i], 0, 2, "%s",
                  three_windows_lables[2]);
        if (manager.menu_items[2]) {
          set_menu_attributes(2);
          set_current_item(manager.content_menus[2], manager.selected_item[2]);
        }
      }
    }
    break;
  }

  for (int i = 0; i < 3; i++)
    wrefresh(manager.content_windows[i]);
}

void handle_input(int ch) {
  switch (ch) {
  case 'q':
    program_exit = true;
    // this exit handles only curses related cleaning
    for (int i = 0; i < 3; i++) {
      // not removing if alredy NULL, (memory leaks)
      if (manager.menu_items[i] != NULL) {
        remove_menu(i);
        remove_items(i);
      }
      delwin(manager.content_windows[i]);
    }
    delwin(manager.top_bar);
    delwin(manager.player_bar);
    // closing the mpv thread
    if (get_mpv_status() != MPV_STATUS_IDLE) {
      shutdown_player(); // killing previous instance playing
      pthread_join(mpv_thread, NULL);
    }
    free(queue);
    return;
  case '-':
    if (get_mpv_status() != MPV_STATUS_IDLE)
      volume_down();
    break;
  case '=':
    if (get_mpv_status() != MPV_STATUS_IDLE)
      volume_up();
    break;
  case '\t':
    // only if it's in top bar selection
    if (manager.state == STATE_TOP_BAR) {
      // %3 to start back when in the last item
      manager.current_top_selection = (manager.current_top_selection + 1) % 3;
      delwin(manager.top_bar);
      manager.top_bar = create_top_bar();
    }
    break;
  case '\n': // ENTER
    enter_input_handling();
    break;
  case 27: // ESC
    if (manager.state == STATE_CONTENT_SELECTION &&
        manager.current_content_window == 2) {
      // going back to album selection
      // emptying the queue
      queue->songs = NULL;
      queue->album = NULL;
      // freeing the menu
      remove_menu(manager.current_content_window);
      remove_items(manager.current_content_window);
      manager.current_content_window = 1;
    } else if (manager.state == STATE_CONTENT_SELECTION &&
               manager.current_content_window == 1) {
      queue->artist = NULL;
      remove_menu(manager.current_content_window);
      remove_items(manager.current_content_window);
      manager.current_content_window = 0;
    }
    break;
  case KEY_DOWN:
    if (manager.state == STATE_CONTENT_SELECTION)
      menu_driver(manager.content_menus[manager.current_content_window],
                  REQ_DOWN_ITEM);
    break;
  case KEY_UP:
    if (manager.state == STATE_CONTENT_SELECTION)
      menu_driver(manager.content_menus[manager.current_content_window],
                  REQ_UP_ITEM);
    break;
  case 'p':
    pause_toggle();
    break;
  case KEY_RESIZE: // terminal resized
    refresh_screen();
    break;
  default:
    break;
  }
  wrefresh(manager.content_windows[manager.current_content_window]);
}

void refresh_screen() {
  if (manager.current_layout == LAYOUT_THREE_WINDOWS) {
    for (int i = 0; i < 3; i++) {
      // deleting only the menus, not calling remove_items
      // they will be reused on windows_creation
      remove_menu(i);

      // then the top-level windows
      if (manager.content_windows[i]) {
        delwin(manager.content_windows[i]);
        manager.content_windows[i] = NULL;
      }
    }
    // deleting the top bars
    delwin(manager.top_bar);
    manager.top_bar = NULL;
    delwin(manager.player_bar);
    manager.player_bar = NULL;
  }
  endwin();

  initscr();
  raw();
  noecho();
  halfdelay(1);
  keypad(stdscr, TRUE);
  curs_set(0);

  getmaxyx(stdscr, manager.screen_height, manager.screen_width);
  refresh();

  manager.top_bar = create_top_bar();
  manager.player_bar = create_player_bar();
  create_content_windows();
}

void remove_menu(int index) {
  if (manager.content_menus[index]) {
    unpost_menu(manager.content_menus[index]);
    set_menu_sub(manager.content_menus[index], NULL);
    free_menu(manager.content_menus[index]);
    manager.content_menus[index] = NULL;
  }
  if (manager.content_subwin[index]) {
    delwin(manager.content_subwin[index]);
    manager.content_subwin[index] = NULL;
  }

  wrefresh(manager.content_windows[index]);
}

void remove_items(int index) {
  int i = 0;
  while (manager.menu_items[index][i]) {
    free_item(manager.menu_items[index][i]);
    i++;
  }
  free(manager.menu_items[index]);
  manager.menu_items[index] = NULL;
}

void populate_artists_window() {
  manager.menu_items[0] =
      (ITEM **)calloc(library->folder_list->artists_count + 1, sizeof(ITEM *));
  Artist *tmp = library->folder_list->artists;
  for (int i = 0; i < library->folder_list->artists_count; i++) {
    manager.menu_items[0][i] = new_item(tmp->name, "");
    tmp = tmp->next;
  }

  set_menu_attributes(0);
}

void populate_albums_window() {
  manager.menu_items[1] = (ITEM **)calloc(
      queue->artist->albums_dir->album_count + 1, sizeof(ITEM *));
  Album *tmp = queue->artist->albums_dir->albums;
  for (int i = 0; i < queue->artist->albums_dir->album_count; i++) {
    manager.menu_items[1][i] = new_item(tmp->title, "");
    tmp = tmp->next;
  }

  set_menu_attributes(1);
}

void populate_songs_window() {
  manager.menu_items[2] =
      (ITEM **)calloc(queue->album->songs_dir->song_count + 1, sizeof(ITEM *));
  Song *tmp = queue->album->songs_dir->songs;
  for (int i = 0; i < queue->album->songs_dir->song_count; i++) {
    manager.menu_items[2][i] = new_item(tmp->title, "");
    tmp = tmp->next;
  }

  set_menu_attributes(2);
}

void set_menu_attributes(int index) {
  manager.content_menus[index] = new_menu((ITEM **)manager.menu_items[index]);
  set_menu_win(manager.content_menus[index], manager.content_windows[index]);
  manager.content_subwin[index] =
      derwin(manager.content_windows[index], manager.windows_height - 2,
             manager.windows_width - 2, 1, 1);
  set_menu_sub(manager.content_menus[index], manager.content_subwin[index]);
  set_menu_format(manager.content_menus[index], manager.windows_height - 2, 1);
  set_menu_mark(manager.content_menus[index], "* ");

  post_menu(manager.content_menus[index]);
  wrefresh(manager.content_windows[index]);
}

void enter_input_handling() {
  if (manager.state == STATE_TOP_BAR) {

    if (manager.current_top_selection == 0)
      manager.current_layout = LAYOUT_THREE_WINDOWS;
    else if (manager.current_top_selection == 1)
      manager.current_layout = LAYOUT_TWO_WINDOWS;
    else
      manager.current_layout = LAYOUT_ONE_WINDOW;

    manager.state = STATE_CONTENT_SELECTION;
    manager.current_content_window = 0;
    populate_artists_window();
    // if it's the first artists tab
  } else if (manager.state == STATE_CONTENT_SELECTION &&
             manager.current_layout == LAYOUT_THREE_WINDOWS &&
             manager.current_content_window == 0) {

    // first retrieving the correct folder
    manager.selected_item[0] = current_item(manager.content_menus[0]);
    int index = item_index(manager.selected_item[0]);
    Artist *a = library->folder_list->artists;
    for (int i = 0; i < index; i++)
      a = a->next;
    queue->artist = a; // starting to save to queue
    // user goes in albums tab

    manager.current_content_window = 1;
    populate_albums_window();
  } else if (manager.state == STATE_CONTENT_SELECTION &&
             manager.current_layout == LAYOUT_THREE_WINDOWS &&
             manager.current_content_window == 1) {

    manager.selected_item[1] = current_item(manager.content_menus[1]);
    int index = item_index(manager.selected_item[1]);
    Album *a = queue->artist->albums_dir->albums;
    for (int i = 0; i < index; i++)
      a = a->next;

    queue->album = a;

    // user in songs tab
    manager.current_content_window = 2;
    populate_songs_window();
  } else if (manager.state == STATE_CONTENT_SELECTION &&
             manager.current_layout == LAYOUT_THREE_WINDOWS &&
             manager.current_content_window == 2) {

    manager.selected_item[2] = current_item(manager.content_menus[2]);
    int index = item_index(manager.selected_item[2]);
    Song *a = queue->album->songs_dir->songs;
    for (int i = 0; i < index; i++)
      a = a->next;

    queue->songs = a;
    queue->queue_size = queue->album->song_count - index;
    if (get_mpv_status() != MPV_STATUS_IDLE) {
      shutdown_player(); // killing previous instance playing
      pthread_join(mpv_thread, NULL);
    }

    pthread_create(&mpv_thread, NULL, play_queue, queue);
  }
}

void currenyly_playing(int index) {
  Song *tmp = queue->songs;
  for (int i = 0; i < index; i++)
    tmp = tmp->next;

  // clearing previous text
  if (song_playing) {
    size_t lenght = strlen(song_playing);
    // moving the cursor where the song must be deleted
    wmove(manager.player_bar, 2, 2 + strlen("Playing >> "));
    for (int i = 0; i < lenght; i++)
      waddch(manager.player_bar, ' ');
  }

  song_playing = tmp->title;
  song_time(tmp->duration);
  mvwprintw(manager.player_bar, 2, 2, "Playing >> %s", song_playing);
  wrefresh(manager.player_bar);
}

void playback_status(mpv_status_t status) {
  if (program_exit) // the window would be NULL at program exit
    return;

  // clearing text of previous status
  if (play_status) {
    size_t lenght = strlen(play_status);

    wmove(manager.player_bar, 1, 2 + strlen("Status >> "));
    for (int i = 0; i < lenght; i++)
      waddch(manager.player_bar, ' ');
  }

  switch (status) {
  case MPV_STATUS_IDLE:
    play_status = "IDLE";
    break;
  case MPV_STATUS_PLAYING:
    play_status = "PLAYING";
    break;
  case MPV_STATUS_PAUSED:
    play_status = "PAUSED";
    break;
  default:
    play_status = "IDLE";
    break;
  }

  mvwprintw(manager.player_bar, 1, 2, "Status >> %s", play_status);
  wrefresh(manager.player_bar);
}

void playback_time(int time) {
  if (program_exit)
    return;

  free(play_time);
  play_time = NULL;
  play_time = format_time(time);
  mvwprintw(manager.player_bar, 1, manager.screen_width - 15, "[%s/",
            play_time);
  wrefresh(manager.player_bar);
}

void song_time(int time) {
  if (program_exit)
    return;

  free(song_duration);
  song_duration = NULL;
  song_duration = format_time(time);
  mvwprintw(manager.player_bar, 1, manager.screen_width - 8, "%s]",
            song_duration);
  wrefresh(manager.player_bar);
}

char *format_time(int seconds) {
  // finding the minutes
  int minutes = seconds / 60;
  // for each minute reducing the seconds
  for (int i = 0; i < minutes; i++)
    seconds -= 60;

  // 00:00
  char *time = malloc(sizeof(char) * 6);
  snprintf(time, 6, "%.2d:%.2d", minutes, seconds);
  return time;
}

void volume_update(int volume) {
  // removing previous volume (without removal there could be bugs related to
  // number of chars, i had multiple ] at the end passing from 100 to 95 for
  // example)
  wmove(manager.player_bar, 2, manager.screen_width - 15);
  for (int i = 0; i < 6; i++)
    waddch(manager.player_bar, ' ');

  mvwprintw(manager.player_bar, 2, manager.screen_width - 15, "[%d%%]", volume);
  wrefresh(manager.player_bar);
}

void error_window(const char *message) {
  initscr();
  raw();
  noecho();
  halfdelay(1);
  keypad(stdscr, TRUE);
  curs_set(0);

  getmaxyx(stdscr, manager.screen_height, manager.screen_width);
  WINDOW *win =
      newwin(manager.screen_height / 2, manager.screen_width / 2, 0, 0);
  box(win, 0, 0);
  refresh();
  mvwprintw(win, 1, 1, "ERROR: %s\n Press 'q' to close", message);
  wrefresh(win);
  while (1) {
    char ch = getch();
    if (ch == 'q')
      break;
  }
}
