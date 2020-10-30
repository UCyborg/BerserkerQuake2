CC=gcc
WINDRES=windres
ifndef OS
OS=$(shell uname)
endif

CFLAGS=-w -O3 -fno-strict-aliasing -DNDEBUG

ifeq ($(OS), Windows_NT)
GL_LIB=-lopengl32
else
GL_LIB=-lGL
endif

SDL2_LIBS=$(shell sdl2-config --libs)

LDFLAGS_ENGINE:=-lm $(GL_LIB) -lvorbis -lvorbisfile -lz -lminizip $(SDL2_LIBS) -lpng -ljpeg
LDFLAGS_GAME=-shared -fPIC -lm

ifeq ($(OS), Windows_NT)
LDFLAGS_ENGINE+=-lws2_32
endif

ENGINE_SOURCES=main.c unpak.c
GAME_SOURCES=game.c

ENGINE_OBJECTS=$(ENGINE_SOURCES:.c=.o)
GAME_OBJECTS=$(GAME_SOURCES:.c=.o)

ENGINE_EXE=berserkerq2
GAME_LIB=game.so
RES=
ifeq ($(OS), Windows_NT)
GAME_LIB=game.dll
RES=Berserker/Berserker.res
ICON=Berserker/Berserker.rc
endif

all: $(ENGINE_EXE) $(GAME_LIB)

$(ENGINE_OBJECTS): %.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(GAME_OBJECTS): %.o : %.c
	$(CC) $(CFLAGS) -fPIC -fvisibility=hidden -c $< -o $@

$(ENGINE_EXE): $(ENGINE_OBJECTS) $(RES)
	$(CC) -o $@ $^ $(LDFLAGS_ENGINE)

$(GAME_LIB): $(GAME_OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS_GAME)

$(RES): $(ICON)
	$(WINDRES) $^ -O coff -o $@

clean:
	rm -f $(ENGINE_EXE) $(GAME_LIB) $(ENGINE_OBJECTS) $(GAME_OBJECTS) $(RES)
