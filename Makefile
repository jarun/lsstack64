all: lsstack unwind

lsstack: lsstack.c
	gcc -W -Wall -g -o lsstack64 lsstack.c -lbfd -liberty

unwind: unwind.c
	gcc -W -Wall -g -o unwind unwind.c -lunwind-x86_64

clean:
	rm -f lsstack64
	rm -f unwind

distclean: clean
	rm -f *~

# just for checkinstall
install: lsstack
	install -d ${DESTDIR}/usr/bin/
	install -g staff -o root ${<} ${DESTDIR}/usr/bin/
