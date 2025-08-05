CROSS_COMPILE ?=

CC = $(CROSS_COMPILE)gcc

SRC = writer.c
OBJ = writer.o
BINARY = writer

CFLAGS = -Wall -O2

all: $(BINARY)

$(BINARY): $(OBJ)
	$(CC) $(OBJ) -o $@

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f writer writer.o

.PHONY: all writer writer.o clean
