build:
	gcc -g -O2 -Wall -fno-strict-aliasing -lcap -lz -lm -lssl -lcrypto -lrt -ldl -o tlsdate tlsdate.c

install:
	install -o root -m 755 $(DESTDIR)/usr/sbin/tlsdate

uninstall:
	rm $(DESTDIR)/usr/sbin/tlsdate

clean:
	-rm tlsdate
