# Makefile, ECE252  
# Yiqing Huang, 2018/11/02

CC = gcc 
CFLAGS = -Wall -std=c99 -g # "curl-config --cflags" output is empty  
LD = gcc
LDFLAGS = -std=c99 -g 
LDLIBS = -lcurl -lz -lpthread# "curl-config --libs" output 

SRCS   = pastor.c
OBJS   = pastor.o
TARGETS= pastor

all: ${TARGETS}

pastor.out: $(OBJS) 
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS) 

%.o: %.c 
	$(CC) $(CFLAGS) -c $< 

%.d: %.c
	gcc -MM -MF $@ $<

-include $(SRCS:.c=.d)

.PHONY: clean
clean:
	rm -f *~ *.d *.o $(TARGETS) *.png
# CC=gcc
# CFLAGS=-Wall -g
# LD=gcc
# LDFLAGS=-g
# LDLIBS = -lz   # link with libz


# c_SRCS=catpng.c
# c_OBJS=catpng.o


# all: catpng 

# catpng.out: $(c_OBJS) 
# 	$(LD) $(LDLIBS) $(LDFLAGS) -o $@ $^

# .c.o:
# 	$(CC) $(CFLAGS) -c $<

# %.d: %.c
# 	gcc -MM -MF $@ $<  

# -include $(c_SRCS:.c=.d) 

# .PHONY: clean
# clean:
# 	rm -f *.d *.o *.out *.png
# 	rm catpng 