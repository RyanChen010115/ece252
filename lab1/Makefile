CC=gcc
CFLAGS=-Wall -g
LD=gcc
LDFLAGS=-g

a_SRCS=pnginfo.c
a_OBJS=pnginfo.o

b_SRCS=findpng.c
b_OBJS=findpng.o

c_SRCS=catpng.c
c_OBJS=catpng.o

all: pnginfo findpng catpng

pnginfo.out: $(a_OBJS) 
	$(LD) $(LDFLAGS) -o $@ $^

findpng.out: $(b_OBJS) 
	$(LD) $(LDFLAGS) -o $@ $^

catpng.out: $(c_OBJS) 
	$(LD) $(LDFLAGS) -o $@ $^

.c.o:
	$(CC) $(CFLAGS) -c $<

%.d: %.c
	gcc -MM -MF $@ $<  

-include $(a_SRCS:.c=.d) 
-include $(b_SRCS:.c=.d) 
-include $(c_SRCS:.c=.d) 


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
