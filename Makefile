CC = gcc
CFLAGS = -Wall -g $(shell pkg-config --cflags gtk+-3.0 webkit2gtk-4.1 libsoup-3.0 json-glib-1.0)
LIBS = $(shell pkg-config --libs gtk+-3.0 webkit2gtk-4.1 libsoup-3.0 json-glib-1.0)

OBJS = main.o ui.o hiscores.o mapview.o

client: $(OBJS)
	$(CC) -o client $(OBJS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o client
