
all: CSORT
	tar -c -f A2_101143497_Ismail_Zakaria.zip *.c *.h Makefile *.txt

CSORT: CSORT.c CSORT.h
	gcc -g -o CSORT CSORT.c

clean:
	rm CSORT