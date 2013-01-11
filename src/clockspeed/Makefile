# Don't edit Makefile! Use conf-* for configuration.

SHELL=/bin/sh

default: it

auto-ccld.sh: \
conf-cc conf-ld warn-auto.sh
	( cat warn-auto.sh; \
	echo CC=\'`head -1 conf-cc`\'; \
	echo LD=\'`head -1 conf-ld`\' \
	) > auto-ccld.sh

auto-str: \
load auto-str.o substdio.a error.a str.a
	./load auto-str substdio.a error.a str.a 

auto-str.o: \
compile auto-str.c substdio.h readwrite.h exit.h
	./compile auto-str.c

auto_home.c: \
auto-str conf-home
	./auto-str auto_home `head -1 conf-home` > auto_home.c

auto_home.o: \
compile auto_home.c
	./compile auto_home.c

byte_chr.o: \
compile byte_chr.c byte.h
	./compile byte_chr.c

byte_copy.o: \
compile byte_copy.c byte.h
	./compile byte_copy.c

byte_cr.o: \
compile byte_cr.c byte.h
	./compile byte_cr.c

byte_diff.o: \
compile byte_diff.c byte.h
	./compile byte_diff.c

byte_zero.o: \
compile byte_zero.c byte.h
	./compile byte_zero.c

check: \
it man instcheck
	./instcheck

clockadd: \
load clockadd.o strerr.a substdio.a error.a str.a
	./load clockadd strerr.a substdio.a error.a str.a 

clockadd.0: \
clockadd.1
	nroff -man clockadd.1 > clockadd.0

clockadd.o: \
compile clockadd.c substdio.h readwrite.h strerr.h exit.h select.h
	./compile clockadd.c

clockspeed: \
load clockspeed.o auto_home.o fifo.o open.a error.a str.a fs.a
	./load clockspeed auto_home.o fifo.o open.a error.a str.a \
	fs.a 

clockspeed.0: \
clockspeed.1
	nroff -man clockspeed.1 > clockspeed.0

clockspeed.o: \
compile clockspeed.c readwrite.h exit.h select.h scan.h fmt.h str.h \
fifo.h open.h error.h auto_home.h timing.h hasrdtsc.h hasgethr.h
	./compile clockspeed.c

clockview: \
load clockview.o strerr.a substdio.a error.a str.a fs.a
	./load clockview strerr.a substdio.a error.a str.a fs.a 

clockview.0: \
clockview.1
	nroff -man clockview.1 > clockview.0

clockview.o: \
compile clockview.c substdio.h readwrite.h strerr.h exit.h fmt.h
	./compile clockview.c

compile: \
make-compile warn-auto.sh systype
	( cat warn-auto.sh; ./make-compile "`cat systype`" ) > \
	compile
	chmod 755 compile

error.a: \
makelib error.o error_str.o
	./makelib error.a error.o error_str.o

error.o: \
compile error.c error.h
	./compile error.c

error_str.o: \
compile error_str.c error.h
	./compile error_str.c

fifo.o: \
compile fifo.c hasmkffo.h fifo.h
	./compile fifo.c

find-systype: \
find-systype.sh auto-ccld.sh
	cat auto-ccld.sh find-systype.sh > find-systype
	chmod 755 find-systype

fmt_str.o: \
compile fmt_str.c fmt.h
	./compile fmt_str.c

fmt_uint.o: \
compile fmt_uint.c fmt.h
	./compile fmt_uint.c

fmt_uint0.o: \
compile fmt_uint0.c fmt.h
	./compile fmt_uint0.c

fmt_ulong.o: \
compile fmt_ulong.c fmt.h
	./compile fmt_ulong.c

fs.a: \
makelib fmt_str.o fmt_uint.o fmt_uint0.o fmt_ulong.o scan_ulong.o
	./makelib fs.a fmt_str.o fmt_uint.o fmt_uint0.o \
	fmt_ulong.o scan_ulong.o

hasgethr.h: \
trygethr.c compile load
	( ( ./compile trygethr.c && ./load trygethr ) >/dev/null \
	2>&1 \
	&& echo \#define HASGETHRTIME 1 || exit 0 ) > hasgethr.h
	rm -f trygethr.o

hasmkffo.h: \
trymkffo.c compile load
	( ( ./compile trymkffo.c && ./load trymkffo ) >/dev/null \
	2>&1 \
	&& echo \#define HASMKFIFO 1 || exit 0 ) > hasmkffo.h
	rm -f trymkffo.o trymkffo

hasrdtsc.h: \
tryrdtsc.c compile load
	( ( ./compile tryrdtsc.c && ./load tryrdtsc && ./tryrdtsc \
	) >/dev/null 2>&1 \
	&& echo \#define HASRDTSC 1 || exit 0 ) > hasrdtsc.h
	rm -f tryrdtsc.o tryrdtsc

hier.o: \
compile hier.c auto_home.h
	./compile hier.c

install: \
load install.o hier.o auto_home.o strerr.a substdio.a open.a error.a \
str.a
	./load install hier.o auto_home.o strerr.a substdio.a \
	open.a error.a str.a 

install.o: \
compile install.c substdio.h strerr.h error.h open.h readwrite.h \
exit.h
	./compile install.c

instcheck: \
load instcheck.o hier.o auto_home.o strerr.a substdio.a error.a str.a
	./load instcheck hier.o auto_home.o strerr.a substdio.a \
	error.a str.a 

instcheck.o: \
compile instcheck.c strerr.h error.h readwrite.h exit.h
	./compile instcheck.c

ip.o: \
compile ip.c fmt.h scan.h ip.h
	./compile ip.c

it: \
man sntpclock taiclock taiclockd clockspeed clockadd clockview \
install instcheck

leapsecs_add.o: \
compile leapsecs_add.c leapsecs.h tai.h uint64.h
	./compile leapsecs_add.c

leapsecs_init.o: \
compile leapsecs_init.c leapsecs.h
	./compile leapsecs_init.c

leapsecs_read.o: \
compile leapsecs_read.c tai.h uint64.h leapsecs.h
	./compile leapsecs_read.c

libtai.a: \
makelib tai_pack.o tai_unpack.o taia_add.o taia_half.o taia_less.o \
taia_now.o taia_pack.o taia_sub.o taia_unpack.o leapsecs_add.o \
leapsecs_init.o leapsecs_read.o
	./makelib libtai.a tai_pack.o tai_unpack.o taia_add.o \
	taia_half.o taia_less.o taia_now.o taia_pack.o taia_sub.o \
	taia_unpack.o leapsecs_add.o leapsecs_init.o leapsecs_read.o

load: \
make-load warn-auto.sh systype
	( cat warn-auto.sh; ./make-load "`cat systype`" ) > load
	chmod 755 load

make-compile: \
make-compile.sh auto-ccld.sh
	cat auto-ccld.sh make-compile.sh > make-compile
	chmod 755 make-compile

make-load: \
make-load.sh auto-ccld.sh
	cat auto-ccld.sh make-load.sh > make-load
	chmod 755 make-load

make-makelib: \
make-makelib.sh auto-ccld.sh
	cat auto-ccld.sh make-makelib.sh > make-makelib
	chmod 755 make-makelib

makelib: \
make-makelib warn-auto.sh systype
	( cat warn-auto.sh; ./make-makelib "`cat systype`" ) > \
	makelib
	chmod 755 makelib

man: \
sntpclock.0 taiclock.0 taiclockd.0 clockspeed.0 clockadd.0 \
clockview.0

open.a: \
makelib open_read.o open_trunc.o open_write.o
	./makelib open.a open_read.o open_trunc.o open_write.o

open_read.o: \
compile open_read.c open.h
	./compile open_read.c

open_trunc.o: \
compile open_trunc.c open.h
	./compile open_trunc.c

open_write.o: \
compile open_write.c open.h
	./compile open_write.c

scan_ulong.o: \
compile scan_ulong.c scan.h
	./compile scan_ulong.c

select.h: \
compile trysysel.c select.h1 select.h2
	( ./compile trysysel.c >/dev/null 2>&1 \
	&& cat select.h2 || cat select.h1 ) > select.h
	rm -f trysysel.o trysysel

setup: \
it man install leapsecs.dat
	./install

shar: \
FILES BLURB README TODO THANKS CHANGES FILES VERSION SYSDEPS INSTALL \
TARGETS Makefile conf-home auto_home.h conf-cc conf-ld hier.c \
clockspeed.1 clockspeed.c clockadd.1 clockadd.c clockview.1 \
clockview.c sntpclock.1 sntpclock.c taiclock.1 taiclock.c taiclockd.1 \
taiclockd.c find-systype.sh make-compile.sh make-load.sh \
make-makelib.sh trycpp.c warn-auto.sh byte.h byte_diff.c byte_chr.c \
byte_copy.c byte_cr.c byte_zero.c str.h str_diff.c str_len.c strerr.h \
strerr_sys.c strerr_die.c substdio.h substdio.c substdi.c substdo.c \
substdio_copy.c subfd.h subfderr.c readwrite.h exit.h error.h error.c \
error_str.c ip.h ip.c fmt.h fmt_str.c fmt_uint.c fmt_uint0.c \
fmt_ulong.c scan.h scan_ulong.c select.h1 select.h2 trysysel.c fifo.h \
fifo.c trymkffo.c open.h open_read.c open_trunc.c open_write.c \
auto-str.c install.c instcheck.c trylsock.c leapsecs.dat leapsecs.3 \
leapsecs.h leapsecs_add.c leapsecs_init.c leapsecs_read.c tai.3 \
tai_pack.3 tai.h tai_pack.c tai_unpack.c taia.3 taia_now.3 \
taia_pack.3 taia.h taia_add.c taia_half.c taia_less.c taia_now.c \
taia_pack.c taia_sub.c taia_unpack.c uint64.h1 uint64.h2 tryulong64.c \
timing.h tryrdtsc.c trygethr.c
	shar -m `cat FILES` > shar
	chmod 400 shar

sntpclock: \
load sntpclock.o ip.o libtai.a strerr.a substdio.a error.a str.a fs.a \
socket.lib
	./load sntpclock ip.o libtai.a strerr.a substdio.a error.a \
	str.a fs.a  `cat socket.lib`

sntpclock.0: \
sntpclock.1
	nroff -man sntpclock.1 > sntpclock.0

sntpclock.o: \
compile sntpclock.c strerr.h ip.h str.h byte.h substdio.h readwrite.h \
select.h scan.h leapsecs.h tai.h uint64.h taia.h
	./compile sntpclock.c

socket.lib: \
trylsock.c compile load
	( ( ./compile trylsock.c && \
	./load trylsock -lsocket -lnsl ) >/dev/null 2>&1 \
	&& echo -lsocket -lnsl || exit 0 ) > socket.lib
	rm -f trylsock.o trylsock

str.a: \
makelib str_len.o str_diff.o byte_diff.o byte_chr.o byte_copy.o \
byte_cr.o byte_zero.o
	./makelib str.a str_len.o str_diff.o byte_diff.o \
	byte_chr.o byte_copy.o byte_cr.o byte_zero.o

str_diff.o: \
compile str_diff.c str.h
	./compile str_diff.c

str_len.o: \
compile str_len.c str.h
	./compile str_len.c

strerr.a: \
makelib strerr_sys.o strerr_die.o
	./makelib strerr.a strerr_sys.o strerr_die.o

strerr_die.o: \
compile strerr_die.c substdio.h subfd.h exit.h strerr.h
	./compile strerr_die.c

strerr_sys.o: \
compile strerr_sys.c error.h strerr.h
	./compile strerr_sys.c

subfderr.o: \
compile subfderr.c readwrite.h substdio.h subfd.h
	./compile subfderr.c

substdi.o: \
compile substdi.c substdio.h byte.h error.h
	./compile substdi.c

substdio.a: \
makelib substdio.o substdi.o substdo.o subfderr.o substdio_copy.o
	./makelib substdio.a substdio.o substdi.o substdo.o \
	subfderr.o substdio_copy.o

substdio.o: \
compile substdio.c substdio.h
	./compile substdio.c

substdio_copy.o: \
compile substdio_copy.c substdio.h
	./compile substdio_copy.c

substdo.o: \
compile substdo.c substdio.h str.h byte.h error.h
	./compile substdo.c

systype: \
find-systype trycpp.c
	./find-systype > systype

tai_pack.o: \
compile tai_pack.c tai.h uint64.h
	./compile tai_pack.c

tai_unpack.o: \
compile tai_unpack.c tai.h uint64.h
	./compile tai_unpack.c

taia_add.o: \
compile taia_add.c taia.h tai.h uint64.h
	./compile taia_add.c

taia_half.o: \
compile taia_half.c taia.h tai.h uint64.h
	./compile taia_half.c

taia_less.o: \
compile taia_less.c taia.h tai.h uint64.h
	./compile taia_less.c

taia_now.o: \
compile taia_now.c taia.h tai.h uint64.h
	./compile taia_now.c

taia_pack.o: \
compile taia_pack.c taia.h tai.h uint64.h
	./compile taia_pack.c

taia_sub.o: \
compile taia_sub.c taia.h tai.h uint64.h
	./compile taia_sub.c

taia_unpack.o: \
compile taia_unpack.c taia.h tai.h uint64.h
	./compile taia_unpack.c

taiclock: \
load taiclock.o ip.o libtai.a strerr.a substdio.a error.a str.a fs.a \
socket.lib
	./load taiclock ip.o libtai.a strerr.a substdio.a error.a \
	str.a fs.a  `cat socket.lib`

taiclock.0: \
taiclock.1
	nroff -man taiclock.1 > taiclock.0

taiclock.o: \
compile taiclock.c strerr.h ip.h str.h byte.h substdio.h readwrite.h \
select.h taia.h tai.h uint64.h
	./compile taiclock.c

taiclockd: \
load taiclockd.o libtai.a strerr.a substdio.a error.a str.a \
socket.lib
	./load taiclockd libtai.a strerr.a substdio.a error.a \
	str.a  `cat socket.lib`

taiclockd.0: \
taiclockd.1
	nroff -man taiclockd.1 > taiclockd.0

taiclockd.o: \
compile taiclockd.c taia.h tai.h uint64.h byte.h strerr.h
	./compile taiclockd.c

uint64.h: \
tryulong64.c compile load uint64.h1 uint64.h2
	( ( ./compile tryulong64.c && ./load tryulong64 && \
	./tryulong64 ) >/dev/null 2>&1 \
	&& cat uint64.h1 || cat uint64.h2 ) > uint64.h
	rm -f tryulong64.o tryulong64
