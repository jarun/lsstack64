

lsstack : lsstack.c
	gcc -g -o lsstack -Wall -lbfd -liberty lsstack.c

clean:
	rm -f lsstack

distclean: clean
	rm -f *~

# just for checkinstall
install: lsstack
	install -d ${DESTDIR}/usr/bin/
	install -g staff -o root ${<} ${DESTDIR}/usr/bin/
