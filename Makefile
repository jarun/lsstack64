

lsstack : lsstack.c
	gcc -W -Wall -g -o lsstack lsstack.c -lbfd -liberty

clean:
	rm -f lsstack

distclean: clean
	rm -f *~

# just for checkinstall
install: lsstack
	install -d ${DESTDIR}/usr/bin/
	install -g staff -o root ${<} ${DESTDIR}/usr/bin/
