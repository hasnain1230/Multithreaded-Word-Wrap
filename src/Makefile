TARGET = ww
CC = gcc
CFLAGS = -g -std=c99 -Wall -Wvla -Wextra -Wundef -Wshadow -Wpointer-arith -O2 -pthread -fexceptions -fsanitize=thread # -Werror
DEPENDENCIES = unbounded_queue.o $(TARGET).o

all: $(TARGET)

run: $(TARGET)
	./$<

$(TARGET): $(DEPENDENCIES)
	$(CC) $(CFLAGS) $^ -o $@

$(TARGET).o: $(TARGET).c unbounded_queue.h
	$(CC) -c $(CFLAGS) $<

queue.o: unbounded_queue.c unbounded_queue.h
	$(CC) -c $(CLAGS) $<

clean:
	rm -rfv $(TARGET) *.o *.a *.dylib *.dSYM
