[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

# TMusicPlayer (Terminal Music Player)

  A terminal-based Subsonic client written in C.

## Dependencies
- OpenSSL
- libcurl
- json-c
- ncurses
- libxml2
- mpv
- pkg-config

## Compiling 
  
  Compile with `make`.

## Paths/Configuration

  The app expects a `config.json` file in the path: `$HOME/.config/tmusicplayer`, the folder will be created automatically the first time the app is opened but the file must be put there manually.

### Example

```json
  {
	  "host":"http://subsonic.server",
	  "name":"username",
	  "password":"plain-text-password"
  }
```
  
  Another file will be created automatically and it will contain the app settings.

### Example

```json 
  {
    "volume": 50,
    "playlist-loop": false
  }
```

## WARNING

  This app is in early development! Expect bugs, missing features, and potential crashes.

## Features
  
- Browsing in the order Artist->Album->Songs and queue visualization
- Volume control
- Songs skip
- Queue control: add/remove songs, add albums

## Controls

| Key           | Action                         |
|---------------|--------------------------------|
| `p`             | Play/Pause toggle              |
| `-` / `=`         | Lower/Raise volume by 5        |
| `ESC`           | Go back in navigation          |
| `ENTER`         | Select                         |
| `↑` / `↓`         | Move in the menu               |
| `1`/`2`/`3`         | Switch TAB in top bar          |
| `q`             | Quit                           |
| `l`             | Toggle queue loop              |
| `d`             | Remove song from queue         |
| `k`             | Skip current song              |
| `a`             | Add Album or Song to queue     |


## Screenshots
  ![TMusicPlayer screenshot](images/browse.png)
  ![TMusicPlayer screenshot](images/queue.png)
