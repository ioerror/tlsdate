build:
	gcc -g -O2 -Wall -fno-strict-aliasing -lcap -lz -lm -lssl -lcrypto -lrt -ldl -o tlsdate tlsdate.c

install: build
	install -o root -m 755 tlsdate $(DESTDIR)/usr/sbin/tlsdate

uninstall:
	rm $(DESTDIR)/usr/sbin/tlsdate

clean:
	-rm tlsdate
