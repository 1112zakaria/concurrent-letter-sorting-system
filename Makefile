
all: CSORT clean
	tar -c -f A2_101143497_Ismail_Zakaria.zip *.c *.h Makefile *.txt

CSORT: CSORT.c CSORT.h
	gcc -g -o CSORT CSORT.c

CSORT_dbg: CSORT.c CSORT.h clean
	gcc -g -o CSORT CSORT.c -DDEBUG_ENABLED

clean:
	rm CSORT