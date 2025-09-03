CC = gcc
CFLAGS = -g `pkg-config --cflags libxml-2.0 mpv glib-2.0 gio-2.0`
LDFLAGS = -lssl -lcrypto -lcurl -ljson-c -lncursesw -lmenuw `pkg-config --libs libxml-2.0 mpv glib-2.0 gio-2.0`

SRC = \
    player.c \
    files/files.c \
    subsonic/api/api.c \
    data/data_struct.c \
    gui/gui.c \
    mpv_player/mpv_audio.c \
    mpv_player/mpris.c

OBJ = $(SRC:.c=.o)

TARGET = tmusicplayer

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(OBJ) $(TARGET)
