CC = gcc
CFLAGS = -W -Wall -g

objects = log.o unwind.o

all: lsstack unwind

lsstack: lsstack.c
	gcc -W -Wall -g -o lsstack64 lsstack.c -lbfd -liberty

unwind: $(objects)
	gcc -W -Wall -g -o unwind $(objects) -lunwind-x86_64 -lunwind-ptrace

.PHONY: clean
clean:
	-rm -f lsstack64 unwind $(objects)

distclean: clean
	rm -f *~

# just for checkinstall
install: lsstack
	install -d ${DESTDIR}/usr/bin/
	install -g staff -o root ${<} ${DESTDIR}/usr/bin/
