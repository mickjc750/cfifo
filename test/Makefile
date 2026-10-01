CC ?= gcc
CFLAGS ?= -std=gnu11 -Wall -Wextra -Wpedantic -O0 -g

TARGET := test_cfifo

.PHONY: all test clean

all: $(TARGET)

$(TARGET): test_cfifo.c ../cfifo.h greatest.h
	$(CC) $(CFLAGS) -o $@ test_cfifo.c

test: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
