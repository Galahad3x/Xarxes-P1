all:
	gcc sr.c -ansi -pedantic -Wall -pthread -o sr
	./sr
