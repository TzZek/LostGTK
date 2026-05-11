PKGS = gtk4 webkitgtk-6.0 json-glib-1.0

CFLAGS  ?= -O2 -g
override CFLAGS += -Wall -Wextra -Wno-unused-parameter -Wformat=2 \
                   -D_FORTIFY_SOURCE=3 -fstack-protector-strong \
                   -fstack-clash-protection -fPIE \
                   $(shell pkg-config --cflags $(PKGS))
override LDFLAGS += -pie -Wl,-z,relro,-z,now,-z,noexecstack
LDLIBS := $(shell pkg-config --libs $(PKGS))

lostgtk: src/main.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

sanitize: override CFLAGS  += -fsanitize=address,undefined -fno-omit-frame-pointer -O0
sanitize: override LDFLAGS += -fsanitize=address,undefined
sanitize: lostgtk

run: lostgtk
	./lostgtk

clean:
	rm -f lostgtk

.PHONY: run clean sanitize
