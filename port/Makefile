# Native test build of the RVIP port (headless, keys from stdin):
#   make -C port            -> port/ularn-test
#   make -C port asan       -> AddressSanitizer build
# The web build is web/build.sh.
CC ?= cc
CFLAGS ?= -O1 -g
override CFLAGS += -std=gnu89 -DULARN_PORT -DLIBDIR='"."' -I. -w -fcommon
GAME = action bill create data diag display fortune player help io main monster moreobj \
	movem object regen savelev scores show signal sphere store tok
OBJ = $(GAME:%=obj/%.o) obj/wcurses.o obj/panes.o obj/be_tty.o

ularn-test: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) -lz $(LDFLAGS)

asan:
	$(MAKE) clean
	$(MAKE) CFLAGS="-O1 -g -fsanitize=address -fno-omit-frame-pointer" LDFLAGS=-fsanitize=address

obj/%.o: ../src/%.c ../src/*.h curses.h config.h
	@mkdir -p obj
	$(CC) $(CFLAGS) -c -o $@ $<

obj/%.o: %.c ../src/*.h curses.h
	@mkdir -p obj
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf obj ularn-test
