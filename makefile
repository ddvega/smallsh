smallsh: smallsh.o
	gcc -g -Wall -std=gnu99 -o smallsh smallsh.o

smallsh.o: smallsh.c smallsh.h
	gcc -g -lm -Wall -std=gnu99 -c smallsh.c

clean:
	rm smallsh.o
	rm smallsh

