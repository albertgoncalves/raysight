FLAGS = \
	-ferror-limit=1 \
	-fsanitize=address \
	-fsanitize=bounds \
	-fsanitize=float-divide-by-zero \
	-fsanitize=implicit-conversion \
	-fsanitize=integer \
	-fsanitize=nullability \
	-fsanitize=undefined \
	-fshort-enums \
	-g \
	-Iraylib/include \
	-lGL \
	-lraylib \
	-Lraylib/lib \
	-march=native \
	-O3 \
	-Werror \
	-Weverything \
	-Wno-declaration-after-statement \
	-Wno-padded \
	-Wno-unsafe-buffer-usage

.PHONY: all run clean

all: bin/main

run: all
	./bin/main

clean:
	rm bin/main
	rmdir bin/

raylib/:
	git clone --depth 1 https://github.com/raysan5/raylib.git raylib

raylib/lib/libraylib.a: raylib/
	./scripts/install.sh

bin/main: raylib/lib/libraylib.a src/main.c
	mkdir -p bin/
	clang-format -i src/main.c
	mold -run clang $(FLAGS) src/main.c -o bin/main
