PROJECT=sos
PLATFORM = $(shell uname -s)
CC = gcc

ifeq ($(PLATFORM),MINGW32_NT-5.1)
	LIBS = -lopengl32 -lSDL_mixer
	TARGET = generic
	SDL_CONFIG = win/sdl-config
	EXTRA = src/$(PROJECT).res
	BIN = $(PROJECT).exe
else ifeq ($(PLATFORM),Darwin)
	TARGET = macosx
	SDL_CONFIG = sdl-config
	LIBS = `$(SDL_CONFIG) --static-libs` -lSDL_mixer
	BIN = $(PROJECT)-macosx
	EXTRA_CFLAGS = -DNO_BUNDLE
	BIN = $(PROJECT)
else
	VERSION = `git branch | grep '*' | cut -d ' ' -f 2`
	LIBS = -lGL -lSDL_mixer
	EXTRA_LIBS = -ldl -lm
	TARGET = linux
	SDL_CONFIG = sdl-config
ifeq ($(shell uname -m),x86_64)
	BIN = $(PROJECT)-linux64
else
	BIN = $(PROJECT)-linux32
endif
endif

CFLAGS = -Wall -Wextra -std=c99 -g -I. $(EXTRA_CFLAGS)
INCLUDE = `$(SDL_CONFIG) --cflags` -Ilua-5.1/src
LIBS += -Llua-5.1/src `$(SDL_CONFIG) --libs` -llua -lSDL_image $(EXTRA_LIBS)

SRC := $(wildcard src/*.c)
OBJ := $(patsubst %.c,%.o,$(SRC))
DEP := $(subst .o,.d,$(OBJ))

$(PROJECT)/$(BIN): $(OBJ)
	make -C lua-5.1 $(TARGET)
	echo $(PLATFORM)
ifeq ($(PLATFORM),MINGW32_NT-5.1)
	windres src/$(PROJECT).rc -O coff -o src/$(PROJECT).res
endif
	gcc -o $@ $^ $(LIBS) $(EXTRA)

src/%.o: src/%.c
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@ -MD

release: # this rule is intened to be used only on linux
	rm -f $(PROJECT)*.zip $(PROJECT)/saavgaam $(PROJECT)/setup.lua \
		$(PROJECT)/script/debug.lua*
	sed "s/version = \".*/version = \""$(VERSION)"\",/" \
		$(PROJECT)/config.lua -i
	zip -r $(PROJECT)-$(VERSION).zip $(PROJECT)

.PHONY clean:
	make clean -C lua-5.1
	rm -f $(OBJ) $(DEP) $(PROJECT)/$(BIN)

-include $(DEP)
