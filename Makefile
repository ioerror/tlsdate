# Debug and hardening flags all in one shot
CFLAGS = -g -O1 -Wall -fno-strict-aliasing \
 -D_FORTIFY_SOURCE=2 -fstack-protector-all \
 -fwrapv -fPIE -Wstack-protector \
 --param ssp-buffer-size=1
LDFLAGS = -pie -z relro -z now

build:
	gcc $(CFLAGS) $(LDFLAGS) -lcap -lssl -o tlsdate tlsdate.c

install: build
	install --strip -o root -m 755 tlsdate $(DESTDIR)/usr/sbin/tlsdate
	cp tlsdate.1 $(DESTDIR)/usr/share/man/man1/

uninstall:
	rm $(DESTDIR)/usr/sbin/tlsdate
	rm $(DESTDIR)/usr/share/man/man1/tlsdate.1

clean:
	-rm tlsdate
