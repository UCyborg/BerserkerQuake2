CC=gcc

CFLAGS=-w -O3 -fno-strict-aliasing -DNDEBUG

LDFLAGS_ENGINE=-lm -lGL -lvorbis -lvorbisfile -lz -lminizip -lSDL2 -lpng -ljpeg
LDFLAGS_GAME=-shared -fPIC -lm

ENGINE_SOURCES=main.c unpak.c
GAME_SOURCES=game.c

ENGINE_OBJECTS=$(ENGINE_SOURCES:.c=.o)
GAME_OBJECTS=$(GAME_SOURCES:.c=.o)

ENGINE_EXE=berserkerq2
GAME_LIB=game.so

all: $(ENGINE_EXE) $(GAME_LIB)

$(ENGINE_OBJECTS): %.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(GAME_OBJECTS): %.o : %.c
	$(CC) $(CFLAGS) -fPIC -fvisibility=hidden -c $< -o $@

$(ENGINE_EXE): $(ENGINE_OBJECTS)
	$(CC) -o $@ $(ENGINE_OBJECTS) $(LDFLAGS_ENGINE)

$(GAME_LIB): $(GAME_OBJECTS)
	$(CC) -o $@ $(GAME_OBJECTS) $(LDFLAGS_GAME)

clean:
	-rm $(ENGINE_EXE) $(GAME_LIB) $(ENGINE_OBJECTS) $(GAME_OBJECTS)
