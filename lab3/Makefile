# Makefile, ECE252  
# Yiqing Huang, 2018/11/02

CC = gcc 
CFLAGS = -Wall -std=c99 -g # "curl-config --cflags" output is empty  
LD = gcc
LDFLAGS = -std=c99 -g 
LDLIBS = -lz -lcurl -lpthread# "curl-config --libs" output 

SRCS   = pastor2.c
OBJS   = pastor2.o
TARGETS= pastor2

all: ${TARGETS}

pastor2.out: $(OBJS) 
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS) 

%.o: %.c 
	$(CC) $(CFLAGS) -c $< 

%.d: %.c
	gcc -MM -MF $@ $<

-include $(SRCS:.c=.d)

.PHONY: clean
clean:
	rm -f *~ *.d *.o $(TARGETS) *.png
