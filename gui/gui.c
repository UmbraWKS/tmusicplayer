#include "gui.h"
#include "../files/files.h"
#include "../mpv_player/mpv_callback.h"
#include <curses.h>
#include <menu.h>
#include <mpv/client.h>
#include <sched.h>
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
char *play_status = NULL;   // mpv status
char *play_time = NULL;     // time elapsed playing
char *song_duration = NULL; // total playing song duration
// the volume is taken directly from the Settings

void init_curses() {
  initscr();
  raw();
  noecho();
  halfdelay(1);
  keypad(stdscr, TRUE);
  curs_set(0);
  refresh();

  if (!manager) {
    manager = calloc(1, sizeof(layout_manager_t));
    getmaxyx(stdscr, manager->screen_height, manager->screen_width);
  }
}

void init_main_tui() {
  queue = calloc(1, sizeof(Queue));
  // initializing the mpv player
  pthread_create(&mpv_thread, NULL, init_player, NULL);

  if (!manager)
    init_curses();

  // default on the first tab
  manager->current_layout = LAYOUT_ARTIST_NAVIGATION;
  manager->current_content_window = 0;
  manager->panels_count = ARTIST_NAVIGATION_WINDOWS;
  manager->panels = calloc(manager->panels_count, sizeof(menu_panel_t));
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

WINDOW *create_player_bar() {
  WINDOW *win =
      newwin(PLAYER_BAR_HEIGHT, manager->screen_width, TOP_BAR_HEIGHT, 0);
  box(win, 0, 0);

  if (play_status)
    mvwprintw(win, 1, 2, "Status >> %s", play_status);
  else
    mvwprintw(win, 1, 2, "Status >> ");

  if (user_selection.playing_song)
    mvwprintw(win, 2, 2, "Playing >> %s", user_selection.playing_song->title);
  else
    mvwprintw(win, 2, 2, "Playing >> ");

  switch (settings->loop) {
  case QUEUE:
    mvwprintw(win, 3, 2, "Loop >> QUEUE");
    break;
  case TRACK:
    mvwprintw(win, 3, 2, "Loop >> TRACK");
    break;
  case NONE:
    mvwprintw(win, 3, 2, "Loop >> NONE");
  }

  if (play_time)
    mvwprintw(win, 1, manager->screen_width - 15, "[%s/", play_time);
  else
    mvwprintw(win, 1, manager->screen_width - 15, "[00:00/");

  if (song_duration)
    mvwprintw(win, 1, manager->screen_width - 8, "%s]", song_duration);
  else
    mvwprintw(win, 1, manager->screen_width - 8, "00:00]");

  if (settings && settings->volume)
    mvwprintw(win, 2, manager->screen_width - 15, "[Vol: %d%%]",
              settings->volume);

  wrefresh(win);
  return win;
}
/*
 * Creating static windows that will contain the menus
 */
void create_content_windows() {
  // starting after the previous bars
  int starty = TOP_BAR_HEIGHT + PLAYER_BAR_HEIGHT;
  int windows_height = manager->screen_height - starty;

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

  int windows_width = manager->screen_width / manager->panels_count;

  for (int i = 0; i < manager->panels_count; i++) {
    // relative starting position of the window in the ui
    manager->panels[i].w_width = windows_width;
    manager->panels[i].w_height = windows_height;
    int x_pos = i * windows_width;

    // creation of the windows
    manager->panels[i].window =
        newwin(windows_height, windows_width, starty, x_pos);
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

    free_song_list(queue->songs);
    free(user_selection.artist);
    free(user_selection.album);
    free(user_selection.song);
    free(user_selection.playing_song);
    free(manager);
    endwin();
    return;
  case '-': // decrease volume
    if (get_mpv_status() != MPV_STATUS_IDLE)
      volume_down();
    break;
  case '=': // increase volume
    if (get_mpv_status() != MPV_STATUS_IDLE)
      volume_up();
    break;
  case '1': // these cases handles the top bar selection
            // artist layout
    if (manager->current_layout != LAYOUT_ARTIST_NAVIGATION) {
      remove_panel_windows(manager->panels_count);
      free(manager->panels); // removing old panels
      manager->panels = calloc(ARTIST_NAVIGATION_WINDOWS, sizeof(menu_panel_t));
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
          calloc(PLAYLIST_NAVIGATION_WINDOWS, sizeof(menu_panel_t));
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
      manager->panels = calloc(QUEUE_NAVIGATION_WINDOWS, sizeof(menu_panel_t));
      manager->current_layout = LAYOUT_QUEUE_NAVIGATION;
      manager->panels_count = QUEUE_NAVIGATION_WINDOWS;
      manager->current_content_window = 0;
      create_content_windows();
      populate_queue_window();

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
      remove_menu(&manager->panels[manager->current_content_window]);
      remove_items(&manager->panels[manager->current_content_window]);
      manager->current_content_window = 1;
    } else if (manager->current_content_window == 1) {

      remove_menu(&manager->panels[manager->current_content_window]);
      remove_items(&manager->panels[manager->current_content_window]);
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
    play_pause();
    break;
  case KEY_RESIZE: // terminal resized
    refresh_screen();
    break;
  case 'd': // delete from queue
    if (manager->current_layout == LAYOUT_QUEUE_NAVIGATION &&
        manager->current_content_window == 0) {

      int index = item_index(manager->panels[0].selected_item);
      Song *s = get_song_from_pos(queue->songs, index);
      if (s == NULL)
        return;
      // can't delete currently playing
      if (strcmp(s->id, user_selection.playing_song->id) == 0)
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
  case 'j': // previous song
    if (get_mpv_status() != MPV_STATUS_IDLE)
      previous_song();
    break;
  case 'a': // add to queue
    if (manager->current_layout == LAYOUT_ARTIST_NAVIGATION) {
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
        for (Song *s = al->songs_dir->songs;
             s != NULL && !is_song_present(queue->songs, s->id); s = s->next) {
          queue->songs = add_song_to_list(queue->songs, s);
          i++;
        }
        queue->queue_size += i;
      } else if (manager->current_content_window == 2) { // songs
        int song_index = item_index(manager->panels[2].selected_item);
        Song *s = get_song_from_pos(al->songs_dir->songs, song_index);
        if (s && !is_song_present(queue->songs, s->id)) {
          queue->songs = add_song_to_list(queue->songs, s);
          queue->queue_size++;
        } else if (s == NULL)
          return;
      }
    }
    break;
  case 'l':
    settings->loop = (loop_status_t)(((int)settings->loop + 1) % 3);
    wmove(manager->player_bar, 3, 2);
    for (int i = 0; i < 15; i++)
      waddch(manager->player_bar, ' ');

    switch (settings->loop) {
    case QUEUE:
      mvwprintw(manager->player_bar, 3, 2, "Loop >> QUEUE");
      break;
    case TRACK:
      mvwprintw(manager->player_bar, 3, 2, "Loop >> TRACK");
      break;
    case NONE:
      mvwprintw(manager->player_bar, 3, 2, "Loop >> NONE");
    }

    update_mpris_loop_status(convert_loop_status(settings->loop));

    wrefresh(manager->player_bar);
    break;
  default:
    break;
  }
  wrefresh(manager->panels[manager->current_content_window].window);
}

// TODO: handle refresh screen for musicFolder selection and error_window
void refresh_screen() {
  for (int i = 0; i < manager->panels_count; i++) {
    remove_menu(&manager->panels[i]);

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

  init_curses();
  getmaxyx(stdscr, manager->screen_height, manager->screen_width);
  refresh();

  manager->top_bar = create_top_bar();
  manager->player_bar = create_player_bar();
  create_content_windows();
}

void remove_panel_windows(int panels) {
  for (int i = 0; i < panels; i++) {
    remove_menu(&manager->panels[i]);
    remove_items(&manager->panels[i]);
    delwin(manager->panels[i].window);
    manager->panels[i].window = NULL;
  }
}

void remove_menu(menu_panel_t *panel) {
  if (panel->menu) {
    unpost_menu(panel->menu);
    set_menu_sub(panel->menu, NULL);
    free_menu(panel->menu);
    panel->menu = NULL;
  }
  if (panel->subwin) {
    delwin(panel->subwin);
    panel->subwin = NULL;
  }

  wrefresh(panel->window);
}

void remove_items(menu_panel_t *panel) {
  if (panel->menu_items) {
    int i = 0;
    while (panel->menu_items[i]) {
      free_item(panel->menu_items[i]);
      i++;
    }
    free(panel->menu_items);
    panel->menu_items = NULL;
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
  // TODO: restore state if an artist is saved in user_selection
  set_menu_attributes(&manager->panels[0]);
}

void populate_albums_window() {
  Album *al = user_selection.artist->albums_dir->albums;
  manager->panels[1].menu_items = (ITEM **)calloc(
      user_selection.artist->albums_dir->album_count + 1, sizeof(ITEM *));
  for (int i = 0;
       i < user_selection.artist->albums_dir->album_count && al != NULL; i++) {
    manager->panels[1].menu_items[i] = new_item(al->title, "");
    al = al->next;
  }

  set_menu_attributes(&manager->panels[1]);
}

void populate_songs_window() {
  int index = item_index(manager->panels[1].selected_item);

  manager->panels[2].menu_items = (ITEM **)calloc(
      user_selection.album->songs_dir->song_count + 1, sizeof(ITEM *));
  Song *tmp = user_selection.album->songs_dir->songs;
  for (int i = 0; i < user_selection.album->songs_dir->song_count; i++) {
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

  // if a song is playing adjusting the cursor position
  if (queue->songs)
    set_cursor_on_song_menu(&manager->panels[0], queue->songs,
                            user_selection.playing_song->title);
}

void set_menu_attributes(menu_panel_t *panel) {
  panel->menu = new_menu(panel->menu_items);
  set_menu_win(panel->menu, panel->window);
  panel->subwin =
      derwin(panel->window, panel->w_height - 2, panel->w_width - 2, 1, 1);
  set_menu_sub(panel->menu, panel->subwin);
  set_menu_format(panel->menu, panel->w_height - 2, 1);
  set_menu_mark(panel->menu, "");

  post_menu(panel->menu);
  wrefresh(panel->window);
}

void enter_input_handling() {
  if (manager->current_layout == LAYOUT_ARTIST_NAVIGATION &&
      manager->current_content_window == 0) {

    int index = item_index(manager->panels[0].selected_item);
    Artist *a = get_artist_from_pos(library->folder_list->artists, index);
    if (a == NULL)
      return;
    if (user_selection.artist != NULL)
      free(user_selection.artist);
    user_selection.artist = duplicate_artist(a);

    manager->current_content_window = 1;
    populate_albums_window();
  } else if (manager->current_layout == LAYOUT_ARTIST_NAVIGATION &&
             manager->current_content_window == 1) {

    int index = item_index(manager->panels[1].selected_item);
    Album *a =
        get_album_from_pos(user_selection.artist->albums_dir->albums, index);
    if (a == NULL)
      return;
    if (user_selection.album != NULL)
      free(user_selection.album);

    user_selection.album = duplicate_album(a);

    manager->current_content_window = 2;
    populate_songs_window();
  } else if (manager->current_layout == LAYOUT_ARTIST_NAVIGATION &&
             manager->current_content_window == 2) {

    int index_song = item_index(manager->panels[2].selected_item);
    // song needed to loop all the songs in the album
    Song *s =
        get_song_from_pos(user_selection.album->songs_dir->songs, index_song);
    if (s == NULL)
      return;
    if (user_selection.song != NULL)
      free(user_selection.song);
    user_selection.song = duplicate_song(s);

    free_song_list(queue->songs);
    queue->songs = NULL;
    Song *song = user_selection.album->songs_dir->songs;
    while (song != NULL) {
      queue->songs = add_song_to_list(queue->songs, song);
      song = song->next;
    }
    queue->queue_size = user_selection.album->song_count;

    start_new_playback(s->id);
  } else if (manager->current_layout == LAYOUT_QUEUE_NAVIGATION &&
             manager->current_content_window == 0) {
    int index = item_index(manager->panels[0].selected_item);
    Song *s = get_song_from_pos(queue->songs, index);
    if (s == NULL)
      return;

    play_song(s->id);
  }
}

void currently_playing(Song *song) {
  if (program_exit)
    return;

  // clearing previous text
  if (user_selection.playing_song) {
    size_t lenght = strlen(user_selection.playing_song->title);
    // moving the cursor where the song must be deleted
    wmove(manager->player_bar, 2, 2 + strlen("Playing >> "));
    for (int i = 0; i < lenght; i++)
      waddch(manager->player_bar, ' ');
  }

  if (user_selection.playing_song) {
    free(user_selection.playing_song);
    user_selection.playing_song = NULL;
  }
  user_selection.playing_song = duplicate_song(song);
  song_time(user_selection.playing_song->duration);
  mvwprintw(manager->player_bar, 2, 2, "Playing >> %s",
            user_selection.playing_song->title);
  wrefresh(manager->player_bar);

  // if the user is in queue layout moving the cursor to match the song playing
  if (manager->current_layout == LAYOUT_QUEUE_NAVIGATION)
    set_cursor_on_song_menu(&manager->panels[0], queue->songs,
                            user_selection.playing_song->title);
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
  for (int i = 0; i < 11; i++)
    waddch(manager->player_bar, ' ');

  mvwprintw(manager->player_bar, 2, manager->screen_width - 15, "[Vol: %d%%]",
            volume);
  wrefresh(manager->player_bar);
}

void error_window(const char *message) {
  if (stdscr == NULL)
    init_curses();

  if (manager->error_window != NULL)
    free(manager->error_window);

  manager->error_window = calloc(1, sizeof(error_window_t));

  manager->error_window->w_height = 4;
  if (manager->screen_width > strlen(message) + 2)
    manager->error_window->w_width = strlen(message) + 2;
  else
    manager->error_window->w_width = manager->screen_width - 2;
  manager->error_window->window =
      newwin(manager->error_window->w_height, manager->error_window->w_width, 0,
             manager->screen_width - manager->error_window->w_width - 1);
  box(manager->error_window->window, 0, 0);
  refresh();
  mvwprintw(manager->error_window->window, 1, 1, "%s", message);
  mvwprintw(manager->error_window->window, 2, 1, "Press 'CTRL + q' to close");
  wrefresh(manager->error_window->window);

  int ch;
  timeout(16);
  while (1) {
    ch = getch();
    if (ch == 17) {
      free(manager->error_window);
      manager->error_window = NULL;
      break;
    }
  }
  // if the top bar doesn't exists the error appeared before initialing ncurses
  if (manager->top_bar != NULL)
    refresh_screen();
}

void set_cursor_on_song_menu(menu_panel_t *panel, Song *list, char *target) {
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

int music_folder_window() {
  if (!stdscr)
    init_curses();

  manager->library_selection = calloc(1, sizeof(menu_panel_t));

  manager->library_selection->w_height = library->folders_count + 2;
  manager->library_selection->w_width = manager->screen_width / 3;

  manager->library_selection->window = newwin(
      manager->library_selection->w_height, manager->library_selection->w_width,
      (manager->screen_height - manager->library_selection->w_height) / 2,
      (manager->screen_width - manager->library_selection->w_width) / 2);

  box(manager->library_selection->window, 0, 0);
  mvwprintw(manager->library_selection->window, 0, 2, "Select Library");
  wrefresh(manager->library_selection->window);

  manager->library_selection->menu_items =
      (ITEM **)calloc(library->folders_count + 1, sizeof(ITEM *));
  MusicFolder *tmp = library->folder_list;
  for (int i = 0; i < library->folders_count && tmp; ++i) {
    manager->library_selection->menu_items[i] = new_item(tmp->name, "");
    tmp = tmp->next;
  }
  set_menu_attributes(manager->library_selection);

  manager->library_selection->selected_item =
      current_item(manager->library_selection->menu);

  bool run = true;
  int input;
  int selection = 0;

  while (run) {
    input = getch();
    switch (input) {
    case 'q':
      selection = -1;
      run = false;
      break;
    case '\n':
      selection = item_index(manager->library_selection->selected_item);
      run = false;
      break;
    case KEY_DOWN:
      menu_driver(manager->library_selection->menu, REQ_DOWN_ITEM);
      manager->library_selection->selected_item =
          current_item(manager->library_selection->menu);
      break;
    case KEY_UP:
      menu_driver(manager->library_selection->menu, REQ_UP_ITEM);
      manager->library_selection->selected_item =
          current_item(manager->library_selection->menu);
      break;
    }
    wrefresh(manager->library_selection->window);
  }

  remove_menu(manager->library_selection);
  remove_items(manager->library_selection);
  delwin(manager->library_selection->window);
  free(manager->library_selection);

  return selection;
}
