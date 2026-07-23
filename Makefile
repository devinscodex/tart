CC     = clang
CFLAGS = -std=c99 -Wall -Werror -fsanitize=address -g
LIBS   = -lncursesw -ltinfo -lm

SRC    = main.c input.c
TARGET = tart

all: $(TARGET)

$(TARGET): $(SRC) tart.h
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LIBS)

clean:
	rm -f $(TARGET) maptest mapedit spritetest

run: $(TARGET)
	./$(TARGET)

maptest: maptest.c tart.h
	$(CC) $(CFLAGS) -o $@ maptest.c $(LIBS)

mapedit: mapedit.c tart.h
	$(CC) $(CFLAGS) -o $@ mapedit.c $(LIBS)

spritetest: spritetest.c tart.h
	$(CC) $(CFLAGS) -o $@ spritetest.c $(LIBS)

.PHONY: all clean run maptest mapedit spritetest
