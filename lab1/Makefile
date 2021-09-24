CC=gcc
CFLAGS=-Wall -g
LD=gcc
LDFLAGS=-g

SRCS=catpng.c
OBJS=catpng.o

all: catpng

pnginfo.out: $(OBJS) 
	$(LD) $(LDFLAGS) -o $@ $^

.c.o:
	$(CC) $(CFLAGS) -c $<

%.d: %.c
	gcc -MM -MF $@ $<  

-include $(SRCS:.c=.d) 

SRCS=pnginfo.c
OBJS=pnginfo.o

all: pnginfo

pnginfo.out: $(OBJS) 
	$(LD) $(LDFLAGS) -o $@ $^

.c.o:
	$(CC) $(CFLAGS) -c $<

%.d: %.c
	gcc -MM -MF $@ $<  

-include $(SRCS:.c=.d) 

SRCS=findpng.c
OBJS=findpng.o

all: findpng

findpng.out: $(OBJS) 
	$(LD) $(LDFLAGS) -o $@ $^

.c.o:
	$(CC) $(CFLAGS) -c $<

%.d: %.c
	gcc -MM -MF $@ $<  

-include $(SRCS:.c=.d) 

.PHONY: clean
clean:
	rm -f *.d *.o *.out 


# CC=gcc
# CFLAGS=-Wall -g
# LD=gcc
# LDFLAGS=-g

# SRCS=pnginfo.c
# OBJS=pnginfo.o

# all: pnginfo

# pnginfo.out: $(OBJS) 
# 	$(LD) $(LDFLAGS) -o $@ $^

# .c.o:
# 	$(CC) $(CFLAGS) -c $<

# %.d: %.c
# 	gcc -MM -MF $@ $<  

# -include $(SRCS:.c=.d) 

# .PHONY: clean
# clean:
# 	rm -f *.d *.o *.out 

# CC=gcc
# CFLAGS=-Wall -g
# LD=gcc
# LDFLAGS=-g

# SRCS=pnginfo.c
# OBJS=pnginfo.o

# all: pnginfo

# pnginfo.out: $(OBJS) 
# 	$(LD) $(LDFLAGS) -o $@ $^

# .c.o:
# 	$(CC) $(CFLAGS) -c $<

# %.d: %.c
# 	gcc -MM -MF $@ $<  

# -include $(SRCS:.c=.d) 

# .PHONY: clean
# clean:
# 	rm -f *.d *.o *.out 
