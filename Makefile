CC = arm-none-linux-gnueabi-gcc
OBJECTS = main.o gprs.o
CFLAGS = -Wall #-g -D _DEBUG_GPRS_

main : $(OBJECTS)
main.o : gprs.h
gprs.o : gprs.h

.PHONY : clean
clean:
	-rm -f main $(OBJECTS)
