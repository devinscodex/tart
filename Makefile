CC     = clang
CFLAGS = -std=c99 -Wall -Werror -fsanitize=address -g -Iinclude
LIBS   = -lncursesw -ltinfo

SRC    = src/main.c src/canvas.c src/render.c src/input.c
OBJ = $(SRC:.c=.o)
TARGET = tart

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET)

run: $(TARGEt)
	./$(TARGET)

.PHONY: all clean run
