build:
	gcc -g -O2 -Wall -fno-strict-aliasing -lcap -lz -lm -lssl -lcrypto -lrt -ldl -o tlsdate tlsdate.c

install: build
	install -o root -m 755 tlsdate $(DESTDIR)/usr/sbin/tlsdate
	cp tlsdate.1 $(DESTDIR)/usr/share/man/man1/

uninstall:
	rm $(DESTDIR)/usr/sbin/tlsdate
	rm $(DESTDIR)/usr/share/man/man1/tlsdate.1

clean:
	-rm tlsdate
