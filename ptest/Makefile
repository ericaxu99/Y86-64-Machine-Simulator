SIM=../pipe/psim

ISADIR = ../misc
YAS=$(ISADIR)/yas

.SUFFIXES: .ys .yo

.ys.yo:
	$(YAS) $*.ys

test:
	./optest.pl -s $(SIM)
	./jtest.pl -s $(SIM)
	./htest.pl -s $(SIM)
	./mtest.pl -s $(SIM)

test-cache:
	./mtest.pl -c -s $(SIM)


clean:
	rm -f *.o *~ *.yo *.ys

