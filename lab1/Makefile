CC=gcc
CFLAGS=-Wall -g
LD=gcc
LDFLAGS=-g
LDLIBS = -lz   # link with libz

a_SRCS=pnginfo.c
a_OBJS=pnginfo.o

b_SRCS=findpng.c
b_OBJS=findpng.o

c_SRCS=catpng.c
c_OBJS=catpng.o

d_SRCS=catpng2.c
d_OBJS=catpng2.o

all: pnginfo findpng catpng catpng2

pnginfo.out: $(a_OBJS) 
	$(LD) $(LDLIBS) $(LDFLAGS) -o $@ $^

findpng.out: $(b_OBJS) 
	$(LD) $(LDLIBS) $(LDFLAGS) -o $@ $^

catpng.out: $(c_OBJS) 
	$(LD) $(LDLIBS) $(LDFLAGS) -o $@ $^

catpng2.out: $(d_OBJS) 
	$(LD) $(LDLIBS) $(LDFLAGS) -o $@ $^	


.c.o:
	$(CC) $(CFLAGS) -c $<

%.d: %.c
	gcc -MM -MF $@ $<  

-include $(a_SRCS:.c=.d) 
-include $(b_SRCS:.c=.d) 
-include $(c_SRCS:.c=.d) 


.PHONY: clean
clean:
	rm -f *.d *.o *.out *.png
	rm catpng catpng2 findpng pnginfo


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
