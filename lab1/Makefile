CC=gcc
CFLAGS=-Wall -g
LD=gcc
LDFLAGS=-g

SRCS=pnginfo.c
OBJS=pnginfo.o

all: pnginfo.out

pnginfo.out: $(OBJS) 
	$(LD) $(LDFLAGS) -o $@ $^

.c.o:
	$(CC) $(CFLAGS) -c $<

%.d: %.c
	gcc -MM -MF $@ $<  

-include $(SRCS:.c=.d) 

.PHONY: clean
clean:
	rm -f *.d *.o *.out 
