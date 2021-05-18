CC = gcc
CFLAGS = -Wall 

all:
	$(CC) world.c -o world $(CFLAGS)
	$(CC) monster.c -o monster $(CFLAGS)

clean:
	rm -rf monster world