CC = emcc

WARNINGS = \
	-Wall \
	-Wextra \
	-Wno-unused-but-set-variable \
	-Wno-misleading-indentation \
	-Wno-implicit-fallthrough \
	-Wno-unused-function

SRC := $(wildcard src/*.c)
OBJ := $(patsubst %.c,%.o,$(SRC))
DEP := $(subst .o,.d,$(OBJ))

# substitute -Oz for -g to enable debugging
public/index.html: $(OBJ)
	make -C lua-5.1 ansi
	emcc -g -o $@ --preload-file game@/ --use-preload-plugins $^ \
		-sALLOW_MEMORY_GROWTH -sLEGACY_GL_EMULATION --use-port=sdl2 --use-port=sdl2_mixer --use-port=sdl2_image:formats=png -s "STACK_SIZE=262144" \
		-Llua-5.1/src -llua -lidbfs.js
	cp public/index.html public/orig.html
	cp public/game.html public/index.html
	TIMESTAMP=`date +%s`; \
		sed -i "s/index\.js/index.js?v=$$TIMESTAMP/g" public/index.html; \
		mv public/index.wasm public/index.$$TIMESTAMP.wasm; \
		sed -i "s/index\.wasm/index.$$TIMESTAMP.wasm/g" public/index.js

src/%.o: src/%.c
	$(CC) -g -I. -Ilua-5.1/src --use-port=sdl2 --use-port=sdl2_mixer --use-port=sdl2_image:formats=png $(WARNINGS) -c $< -o $@ -MD

.PHONY clean:
	make clean -C lua-5.1
	rm -f $(OBJ) $(DEP) public/index.*

-include $(DEP)
