
CC = mips-linux-gnu-gcc

SRCS := unpack.c
OBJS := $(patsubst %.c,%.o,$(wildcard $(SRCS)))
TAR := unpack_tool

CFLAGS = -Wall
LDFLAGS = -lpthread

.PHONY: all clean

%.o: %.c
	$(CC) -c $< -o $@

$(TAR):$(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
	
clean:
	rm -rf *.o;
	rm -rf $(TAR)
	
