BIN = ev_record ev_playback

all: $(BIN)
ev_playback: ev_playback.c
	cc  -D_GNU_SOURCE -std=c99 -o ev_playback ev_playback.c
ev_record: ev_record.c
	cc  -D_GNU_SOURCE -std=c99 -o ev_record ev_record.c

install:
	mkdir -p "$(DESTDIR)/usr/bin"
	mkdir -p "$(DESTDIR)/usr/lib"
	install ev_playback "$(DESTDIR)/usr/bin"
	install ev_record "$(DESTDIR)/usr/bin"
	install process_event "$(DESTDIR)/usr/bin"
clean:
	rm -rf *.o $(BIN)
