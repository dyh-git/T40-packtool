
CC ?= gcc

SRCS := cJSON.c pack.c
OBJS := $(patsubst %.c,%.o,$(wildcard $(SRCS)))
TAR := pack_tool

CFLAGS = -Wall -g
LDFLAGS = -lm 

.PHONY: all clean

%.o: %.c
	$(CC) -c $< -o $@

$(TAR):$(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
	
clean:
	rm -rf *.o;
	rm -rf $(TAR)
	
