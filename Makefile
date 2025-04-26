# makefile for the gmt application
# the '?=' combination sets a variable only
# if it has not been set previously

# General configuration variables:
DESTDIR ?= /
INCDIR ?= $(DESTDIR)/usr/include


LIBS = -lm

GMT_OBJECTS = gmt.c elfcfg.c

GMT_TARGET = gmt

# MODULES = $(SRCS:.c=.o)
# MODULES := $(MODULES:.c=.o)
CC ?= gcc
# CFLAGS ?= -Wall
CFLAGS += $(INCLUDE)
# CFLAGS += -I./gpl
LNK_FLAGS = $(LIBS)


default: all

all: gmt

gmt:
	$(CC) -o $(GMT_TARGET) $(CFLAGS) -O1 $(GMT_OBJECTS) $(LNK_FLAGS) 

gmt_dbg:
	$(CC) -o $(GMT_TARGET) -g $(CFLAGS) $(GMT_OBJECTS) $(LNK_FLAGS) 

gmt_sim:
	$(CC) -o $(GMT_TARGET) -g $(CFLAGS) -D__SIMULATION__ $(GMT_OBJECTS) $(LNK_FLAGS) 

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o $(GMT_TARGET)
