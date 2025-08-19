#include "gui.h"
#include "../files/files.h"
#include "../mpv_player/mpv_callback.h"
#include "../program_data.h"
#include <curses.h>
#include <menu.h>
#include <mpv/client.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

const char *top_bar_labels[TOP_BAR_ITEMS] = {"Artists", "Playlists", "Queue"};
const char *artist_nav_lables[ARTIST_NAVIGATION_WINDOWS] = {"Artists", "Albums",
                                                            "Songs"};
const char *playlist_nav_lables[PLAYLIST_NAVIGATION_WINDOWS] = {"Playlists",
                                                                "Songs"};
// array because i am thinking about adding stuff
const char *queue_nav_lables[QUEUE_NAVIGATION_WINDOWS] = {"Queue"};
layout_manager_t *manager = NULL;
// thread the mpv instance runs on
pthread_t mpv_thread;

/* variables that represent the player status and values inside the player bar*/
char *song_playing = NULL;  // name of the song playing
char *play_status = NULL;   // mpv status
char *play_time = NULL;     // time elapsed playing
char *song_duration = NULL; // total playing song duration
// the volume is taken directly from the Settings

void ncurses_init() {
  manager = calloc(1, sizeof(layout_manager_t));
  queue = calloc(1, sizeof(Queue));
  initscr();
  raw();
  noecho();
  halfdelay(1);
  keypad(stdscr, TRUE);
  curs_set(0);

  getmaxyx(stdscr, manager->screen_height, manager->screen_width);
  // default on the first tab
  manager->current_layout = LAYOUT_ARTIST_NAVIGATION;
  manager->current_content_window = 0;
  manager->panels_count = ARTIST_NAVIGATION_WINDOWS;
  manager->panels = calloc(manager->panels_count, sizeof(content_panel_t));
  refresh();

  manager->top_bar = create_top_bar();
  manager->player_bar = create_player_bar();
  create_content_windows();
  populate_artists_window();
}

WINDOW *create_top_bar() {
  WINDOW *win = newwin(TOP_BAR_HEIGHT, manager->screen_width, 0, 0);
  box(win, 0, 0);

  int y = 2;
  for (int i = 0; i < TOP_BAR_ITEMS; i++) {
    if (i == manager->current_layout)
      wattron(win, A_REVERSE); // highlighting selection

    mvwprintw(win, 1, y, "%s", top_bar_labels[i]);

    if (i == manager->current_layout)
      wattroff(win, A_REVERSE);

    y += strlen(top_bar_labels[i]) + 2;
  }
  wrefresh(win);
  return win;
}
// TODO: add playlist loop indicator
WINDOW *create_player_bar() {
  WINDOW *win =
      newwin(PLAYER_BAR_HEIGHT, manager->screen_width, TOP_BAR_HEIGHT, 0);
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
    mvwprintw(win, 1, manager->screen_width - 15, "[%s/", play_time);
  else
    mvwprintw(win, 1, manager->screen_width - 15, "[00:00/");

  if (song_duration)
    mvwprintw(win, 1, manager->screen_width - 8, "%s]", song_duration);
  else
    mvwprintw(win, 1, manager->screen_width - 8, "00:00]");

  if (settings && settings->volume)
    mvwprintw(win, 2, manager->screen_width - 15, "[%d%%]", settings->volume);

  wrefresh(win);
  return win;
}
/*
        Creating static windows that will contain the menus
*/
void create_content_windows() {
  // starting after the previous bars
  int starty = TOP_BAR_HEIGHT + PLAYER_BAR_HEIGHT;
  manager->windows_height = manager->screen_height - starty;

  // setting panels count based on user selected tab
  switch (manager->current_layout) {
  case LAYOUT_ARTIST_NAVIGATION:
    manager->panels_count = ARTIST_NAVIGATION_WINDOWS;
    break;
  case LAYOUT_PLAYLIST_NAVIGATION:
    manager->panels_count = PLAYLIST_NAVIGATION_WINDOWS;
    break;
  case LAYOUT_QUEUE_NAVIGATION:
    manager->panels_count = QUEUE_NAVIGATION_WINDOWS;
    break;
  }

  manager->windows_width = manager->screen_width / manager->panels_count;

  for (int i = 0; i < manager->panels_count; i++) {
    // relative starting position of the window in the ui
    int x_pos = i * manager->windows_width;

    // creation of the windows
    manager->panels[i].window =
        newwin(manager->windows_height, manager->windows_width, starty, x_pos);
    wclear(manager->panels[i].window);
    box(manager->panels[i].window, 0, 0);

    // placing window labels
    const char *label = NULL;
    if (manager->current_layout == LAYOUT_ARTIST_NAVIGATION)
      label = artist_nav_lables[i];
    else if (manager->current_layout == LAYOUT_PLAYLIST_NAVIGATION)
      label = playlist_nav_lables[i];
    else if (manager->current_layout == LAYOUT_QUEUE_NAVIGATION)
      label = queue_nav_lables[i];

    if (label)
      mvwprintw(manager->panels[i].window, 0, 2, "%s", label);

    // restoring menus if content is present
    if (manager->panels[i].menu_items) {
      set_menu_attributes(&manager->panels[i]);

      if (manager->panels[i].selected_item)
        set_current_item(manager->panels[i].menu,
                         manager->panels[i].selected_item);
    }

    wrefresh(manager->panels[i].window);
  }
}

void handle_input(int ch) {
  switch (ch) {
  case 'q':
    program_exit = true;
    // this exit handles only curses related cleaning
    remove_panel_windows(manager->panels_count);
    free(manager->panels);

    delwin(manager->top_bar);
    delwin(manager->player_bar);
    // closing the mpv thread
    if (get_mpv_status() != MPV_STATUS_IDLE) {
      shutdown_player(); // killing previous instance playing
      pthread_join(mpv_thread, NULL);
    }

    free(queue->songs);
    free(manager);
    endwin();
    return;
  case '-':
    if (get_mpv_status() != MPV_STATUS_IDLE)
      volume_down();
    break;
  case '=':
    if (get_mpv_status() != MPV_STATUS_IDLE)
      volume_up();
    break;
  case '1': // these cases handles the top bar selection
            // artist layout
    if (manager->current_layout != LAYOUT_ARTIST_NAVIGATION) {
      remove_panel_windows(manager->panels_count);
      free(manager->panels); // removing old panels
      manager->panels =
          calloc(ARTIST_NAVIGATION_WINDOWS, sizeof(content_panel_t));
      manager->current_layout = LAYOUT_ARTIST_NAVIGATION;
      manager->panels_count = ARTIST_NAVIGATION_WINDOWS;
      manager->current_content_window = 0;
      create_content_windows();
      populate_artists_window();

      delwin(manager->top_bar);
      manager->top_bar = create_top_bar();
    }
    break;
  case '2': // playlist layout
    if (manager->current_layout != LAYOUT_PLAYLIST_NAVIGATION) {
      remove_panel_windows(manager->panels_count);
      free(manager->panels);
      manager->panels =
          calloc(PLAYLIST_NAVIGATION_WINDOWS, sizeof(content_panel_t));
      manager->current_layout = LAYOUT_PLAYLIST_NAVIGATION;
      manager->panels_count = PLAYLIST_NAVIGATION_WINDOWS;
      manager->current_content_window = 0;
      create_content_windows();

      delwin(manager->top_bar);
      manager->top_bar = create_top_bar();
    }
    break;
  case '3': // queue layout
    if (manager->current_layout != LAYOUT_QUEUE_NAVIGATION) {
      remove_panel_windows(manager->panels_count);
      free(manager->panels);
      manager->panels =
          calloc(QUEUE_NAVIGATION_WINDOWS, sizeof(content_panel_t));
      manager->current_layout = LAYOUT_QUEUE_NAVIGATION;
      manager->panels_count = QUEUE_NAVIGATION_WINDOWS;
      manager->current_content_window = 0;
      create_content_windows();
      populate_queue_window();

      // if a song is playing adjusting the cursor position
      set_cursor_on_song_menu(&manager->panels[0], queue->songs, song_playing);

      delwin(manager->top_bar);
      manager->top_bar = create_top_bar();
    }

    break;
  case '\n': // ENTER - select
    enter_input_handling();
    break;
  case 27: // ESC - back
    if (manager->current_content_window == 2) {

      // freeing the menu
      remove_menu(manager->current_content_window);
      remove_items(manager->current_content_window);
      manager->current_content_window = 1;
    } else if (manager->current_content_window == 1) {

      remove_menu(manager->current_content_window);
      remove_items(manager->current_content_window);
      manager->current_content_window = 0;
    }
    break;
  case KEY_DOWN: // scroll down
    menu_driver(manager->panels[manager->current_content_window].menu,
                REQ_DOWN_ITEM);
    manager->panels[manager->current_content_window].selected_item =
        current_item(manager->panels[manager->current_content_window].menu);
    break;
  case KEY_UP: // scroll up
    menu_driver(manager->panels[manager->current_content_window].menu,
                REQ_UP_ITEM);
    manager->panels[manager->current_content_window].selected_item =
        current_item(manager->panels[manager->current_content_window].menu);
    break;
  case 'p': // pause toggle
    pause_toggle();
    break;
  case KEY_RESIZE: // terminal resized
    refresh_screen();
    break;
  case 'd': // delete from queue
    if (manager->current_layout == LAYOUT_QUEUE_NAVIGATION &&
        manager->current_content_window == 0) {

      int index = item_index(manager->panels[0].selected_item);
      Song *s = queue->songs;
      for (int i = 0; i < index; i++) {
        s = s->next;
      }
      // can't delete currently playing
      if (strcmp(s->title, song_playing) == 0)
        break;

      queue->songs = remove_song_from_list(queue->songs, s->id);
      queue->queue_size = count_songs(queue->songs);

      remove_menu(0);
      remove_items(0);
      populate_queue_window();
    }
    break;
  case 'k': // skip song
    if (get_mpv_status() != MPV_STATUS_IDLE)
      skip_song();
    break;
  case 'a': // add to queue
    if (manager->current_layout == LAYOUT_ARTIST_NAVIGATION) {
      // TODO: add check, if the song is alredy in the queue and don't add it
      // again
      int artist_index = item_index(manager->panels[0].selected_item);
      int album_index = item_index(manager->panels[1].selected_item);
      Artist *a = library->folder_list->artists;
      for (int i = 0; i < artist_index && a != NULL; i++) {
        a = a->next;
      }
      Album *al = a->albums_dir->albums;
      for (int i = 0; i < album_index && al != NULL; i++) {
        al = al->next;
      }

      if (manager->current_content_window == 1) { // albums
        int i = 0;                                // songs counter
        for (Song *s = al->songs_dir->songs; s != NULL; s = s->next) {
          queue->songs = add_song_to_list(queue->songs, s);
          i++;
        }
        queue->queue_size += i;
      } else if (manager->current_content_window == 2) { // songs
        int song_index = item_index(manager->panels[2].selected_item);
        Song *s = al->songs_dir->songs;
        for (int i = 0; i < song_index && s != NULL; i++)
          s = s->next;
        if (s) {
          queue->songs = add_song_to_list(queue->songs, s);
          queue->queue_size++;
        }
      }
    }
    break;
  case 'l': // toggle playlist loop
            // only changing the variable, it's saved to file on program close
    settings->loop = !settings->loop;
    break;
  default:
    break;
  }
  wrefresh(manager->panels[manager->current_content_window].window);
}

void refresh_screen() {
  for (int i = 0; i < manager->panels_count; i++) {
    remove_menu(i);

    // then the top-level windows
    if (manager->panels[i].window) {
      delwin(manager->panels[i].window);
      manager->panels[i].window = NULL;
    }
  }
  // deleting the top bars
  delwin(manager->top_bar);
  manager->top_bar = NULL;
  delwin(manager->player_bar);
  manager->player_bar = NULL;

  endwin();

  initscr();
  raw();
  noecho();
  halfdelay(1);
  keypad(stdscr, TRUE);
  curs_set(0);

  getmaxyx(stdscr, manager->screen_height, manager->screen_width);
  refresh();

  manager->top_bar = create_top_bar();
  manager->player_bar = create_player_bar();
  create_content_windows();
}

void remove_panel_windows(int panels) {
  for (int i = 0; i < panels; i++) {
    remove_menu(i);
    remove_items(i);
    delwin(manager->panels[i].window);
    manager->panels[i].window = NULL;
  }
}

void remove_menu(int index) {
  if (manager->panels[index].menu) {
    unpost_menu(manager->panels[index].menu);
    set_menu_sub(manager->panels[index].menu, NULL);
    free_menu(manager->panels[index].menu);
    manager->panels[index].menu = NULL;
  }
  if (manager->panels[index].subwin) {
    delwin(manager->panels[index].subwin);
    manager->panels[index].subwin = NULL;
  }

  wrefresh(manager->panels[index].window);
}

void remove_items(int index) {
  if (manager->panels[index].menu_items) {
    int i = 0;
    while (manager->panels[index].menu_items[i]) {
      free_item(manager->panels[index].menu_items[i]);
      i++;
    }
    free(manager->panels[index].menu_items);
    manager->panels[index].menu_items = NULL;
  }
}

void populate_artists_window() {
  manager->panels[0].menu_items =
      (ITEM **)calloc(library->folder_list->artists_count + 1, sizeof(ITEM *));
  Artist *tmp = library->folder_list->artists;
  for (int i = 0; i < library->folder_list->artists_count; i++) {
    manager->panels[0].menu_items[i] = new_item(tmp->name, "");
    tmp = tmp->next;
  }

  set_menu_attributes(&manager->panels[0]);
}

void populate_albums_window() {
  int index = item_index(manager->panels[1].selected_item);
  int index_artist = item_index(manager->panels[0].selected_item);
  Artist *a = library->folder_list->artists;
  for (int i = 0; i < index_artist; i++)
    a = a->next;
  Album *al = a->albums_dir->albums;
  for (int i = 0; i < index; i++)
    al = al->next;

  manager->panels[1].menu_items =
      (ITEM **)calloc(a->albums_dir->album_count + 1, sizeof(ITEM *));
  for (int i = 0; i < a->albums_dir->album_count && al != NULL; i++) {
    manager->panels[1].menu_items[i] = new_item(al->title, "");
    al = al->next;
  }

  set_menu_attributes(&manager->panels[1]);
}

void populate_songs_window() {
  int index = item_index(manager->panels[1].selected_item);
  int index_artist = item_index(manager->panels[0].selected_item);
  Artist *a = library->folder_list->artists;
  for (int i = 0; i < index_artist; i++)
    a = a->next;
  Album *al = a->albums_dir->albums;
  for (int i = 0; i < index; i++)
    al = al->next;

  manager->panels[2].menu_items =
      (ITEM **)calloc(al->songs_dir->song_count + 1, sizeof(ITEM *));
  Song *tmp = al->songs_dir->songs;
  for (int i = 0; i < al->songs_dir->song_count; i++) {
    manager->panels[2].menu_items[i] = new_item(tmp->title, "");
    tmp = tmp->next;
  }

  set_menu_attributes(&manager->panels[2]);
}

void populate_queue_window() {
  manager->panels[0].menu_items =
      (ITEM **)calloc(queue->queue_size + 1, sizeof(ITEM *));
  Song *tmp = queue->songs;
  for (int i = 0; i < queue->queue_size; i++) {
    manager->panels[0].menu_items[i] = new_item(tmp->title, "");
    tmp = tmp->next;
  }

  set_menu_attributes(&manager->panels[0]);
}

void set_menu_attributes(content_panel_t *panel) {
  panel->menu = new_menu(panel->menu_items);
  set_menu_win(panel->menu, panel->window);
  panel->subwin = derwin(panel->window, manager->windows_height - 2,
                         manager->windows_width - 2, 1, 1);
  set_menu_sub(panel->menu, panel->subwin);
  set_menu_format(panel->menu, manager->windows_height - 2, 1);
  set_menu_mark(panel->menu, "* ");

  post_menu(panel->menu);
  wrefresh(panel->window);
}

void enter_input_handling() {
  if (manager->current_layout == LAYOUT_ARTIST_NAVIGATION &&
      manager->current_content_window == 0) {

    manager->current_content_window = 1;
    populate_albums_window();
  } else if (manager->current_layout == LAYOUT_ARTIST_NAVIGATION &&
             manager->current_content_window == 1) {

    // user in songs tab
    manager->current_content_window = 2;
    populate_songs_window();
  } else if (manager->current_layout == LAYOUT_ARTIST_NAVIGATION &&
             manager->current_content_window == 2) {

    int index_song = item_index(manager->panels[2].selected_item);
    int index = item_index(manager->panels[1].selected_item);
    int index_artist = item_index(manager->panels[0].selected_item);
    Artist *a = library->folder_list->artists;
    for (int i = 0; i < index_artist; i++)
      a = a->next;
    Album *al = a->albums_dir->albums;
    for (int i = 0; i < index; i++)
      al = al->next;
    Song *s = al->songs_dir->songs;
    for (int i = 0; i < index_song; i++)
      s = s->next;

    queue->songs = NULL;
    int i = 0;
    while (s != NULL) {
      queue->songs = add_song_to_list(queue->songs, s);
      s = s->next;
      i++;
    }
    queue->queue_size = i;
    if (get_mpv_status() != MPV_STATUS_IDLE) {
      shutdown_player(); // killing previous instance playing
      pthread_join(mpv_thread, NULL);
    }

    pthread_create(&mpv_thread, NULL, play_queue, NULL);
  }
  // TODO: add song play when ENTER pressed in queue layout
}

void currently_playing(const char *id) {
  if (program_exit)
    return;
  Song *tmp = queue->songs;
  while (tmp && strcmp(id, tmp->id) != 0)
    tmp = tmp->next;

  // clearing previous text
  if (song_playing) {
    size_t lenght = strlen(song_playing);
    // moving the cursor where the song must be deleted
    wmove(manager->player_bar, 2, 2 + strlen("Playing >> "));
    for (int i = 0; i < lenght; i++)
      waddch(manager->player_bar, ' ');
  }

  song_playing = tmp->title;
  song_time(tmp->duration);
  mvwprintw(manager->player_bar, 2, 2, "Playing >> %s", song_playing);
  wrefresh(manager->player_bar);

  // if the user is in queue layout moving the cursor to match the song playing
  if (manager->current_layout == LAYOUT_QUEUE_NAVIGATION)
    set_cursor_on_song_menu(&manager->panels[0], queue->songs, song_playing);
}

void playback_status(mpv_status_t status) {
  if (program_exit) // the window would be NULL at program exit
    return;

  // clearing text of previous status
  if (play_status) {
    size_t lenght = strlen(play_status);

    wmove(manager->player_bar, 1, 2 + strlen("Status >> "));
    for (int i = 0; i < lenght; i++)
      waddch(manager->player_bar, ' ');
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

  mvwprintw(manager->player_bar, 1, 2, "Status >> %s", play_status);
  wrefresh(manager->player_bar);
}

void playback_time(int time) {
  if (program_exit)
    return;

  free(play_time);
  play_time = NULL;
  play_time = format_time(time);
  mvwprintw(manager->player_bar, 1, manager->screen_width - 15, "[%s/",
            play_time);
  wrefresh(manager->player_bar);
}

void song_time(int time) {
  if (program_exit)
    return;

  free(song_duration);
  song_duration = NULL;
  song_duration = format_time(time);
  mvwprintw(manager->player_bar, 1, manager->screen_width - 8, "%s]",
            song_duration);
  wrefresh(manager->player_bar);
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
  // number of chars
  wmove(manager->player_bar, 2, manager->screen_width - 15);
  for (int i = 0; i < 6; i++)
    waddch(manager->player_bar, ' ');

  mvwprintw(manager->player_bar, 2, manager->screen_width - 15, "[%d%%]",
            volume);
  wrefresh(manager->player_bar);
}

void error_window(const char *message) {
  initscr();
  raw();
  noecho();
  halfdelay(1);
  keypad(stdscr, TRUE);
  curs_set(0);

  getmaxyx(stdscr, manager->screen_height, manager->screen_width);
  WINDOW *win =
      newwin(manager->screen_height / 2, manager->screen_width / 2, 0, 0);
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

void set_cursor_on_song_menu(content_panel_t *panel, Song *list, char *target) {
  if (list && target) {
    Song *s = list;
    int index = 0;
    while (s != NULL && strcmp(s->title, target) != 0) {
      s = s->next;
      index++;
    }
    if (s != NULL && strcmp(s->title, target) == 0) {
      set_current_item(panel->menu, panel->menu_items[index]);
      panel->selected_item = panel->menu_items[index];
    }
  }
  wrefresh(panel->window);
}
