# jfave Janne Louise F Ave
p4: p4.o
	gcc -Wall -std=c99 p4.o -o p4 -lpthread
p4.o: p4.c
	gcc -Wall -std=c99 -c p4.c
clean:
	rm -f p4.o p4
