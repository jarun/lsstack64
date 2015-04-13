lsstack : lsstack.c
	gcc -W -Wall -g -o lsstack64 lsstack.c -lbfd -liberty

clean:
	rm -f lsstack64

distclean: clean
	rm -f *~

# just for checkinstall
install: lsstack
	install -d ${DESTDIR}/usr/bin/
	install -g staff -o root ${<} ${DESTDIR}/usr/bin/
