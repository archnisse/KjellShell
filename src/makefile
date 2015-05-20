all:
	gcc -pedantic -Wall -ansi -O4 kjell.c -o kjell
	chmod 755 kjell

signal:
	gcc -pedantic -Wall -ansi -O4 -o kjell -D SIGDET=1 kjell.c
	chmod 755 kjell

polling:
	gcc -pedantic -Wall -ansi -O4 kjell.c -D SIGDET=0 -o kjell
	chmod 755 kjell

clean:
	rm kjell
