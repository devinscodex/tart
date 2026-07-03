CC     = clang
CFLAGS = -std=c99 -Wall -Werror -fsanitize=address -g -Iinclude
LIBS   = -lncursesw -ltinfo

SRC    = main.c canvas.c render.c input.c
OBJ    = $(SRC:.c=.o)
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
