CC = gcc
CFLAGS = -g `pkg-config --cflags libxml-2.0 mpv`
LDFLAGS = -lssl -lcrypto -lcurl -ljson-c -lncursesw -lmenuw `pkg-config --libs libxml-2.0 mpv`

SRC = \
    player.c \
    files/files.c \
    subsonic/api/api.c \
    data/data_struct.c \
    gui/gui.c \
    mpv_player/mpv_audio.c

OBJ = $(SRC:.c=.o)

TARGET = tmusicplayer

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(OBJ) $(TARGET)
