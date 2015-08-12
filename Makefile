CC = gcc
CFLAGS = -Wall -Wextra -Werror -g

logs = log.o

objects = $(logs) unwind.o

all: lsstack unwind

lsstack: $(logs) lsstack.c
	gcc $(CFLAGS) -o lsstack64 lsstack.c $(logs) -lbfd -liberty

unwind: $(objects)
	gcc $(CFLAGS) -o unwind $(objects) -lunwind-x86_64 -lunwind-ptrace

.PHONY: clean
clean:
	-rm -f lsstack64 unwind $(objects)

distclean: clean
	rm -f *~

# just for checkinstall
install: lsstack
	install -d ${DESTDIR}/usr/bin/
	install -g staff -o root ${<} ${DESTDIR}/usr/bin/
