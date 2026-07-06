CC     = clang
CFLAGS = -std=c99 -Wall -Werror -fsanitize=address -g
LIBS   = -lncursesw -ltinfo -lm

SRC    = main.c input.c
TARGET = tart

all: $(TARGET)

$(TARGET): $(SRC) tart.h
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LIBS)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
