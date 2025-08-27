CROSS_COMPILE ?=

CC = $(CROSS_COMPILE)gcc

SRC = aesdsocket.c
OBJ = aesdsocket.o
BINARY = aesdsocket

CFLAGS = -Wall -O2

all: clean $(BINARY)

$(BINARY): $(OBJ)
	$(CC) $(OBJ) -o $@

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f aesdsocket aesdsocket.o

.PHONY: all aesdsocket aesdsocket.o clean
