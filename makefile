CC	:= cc
CFLAGS	:= -Og -g -std=c99 -Wpedantic -Wall
OBJS	 = chunk.o gc.o main.o object.o parser.o print.o vm.o vendor/mpc.o \
		   vendor/yar.o

growl: $(OBJS)
	$(CC) -o growl $(OBJS)

.PHONY: clean
clean:
	rm -f growl $(OBJS)
