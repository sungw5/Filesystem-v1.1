#
# CMPSC311 - LionCloud Device - Assignment #3
# Makefile - makefile to build the base system (with caching)
#

# Locations

# Make environment
INCLUDES=-I.
CC=gcc
CFLAGS=-I. -c -g -Wall -fno-stack-protector $(INCLUDES)
LINKARGS=-g
LIBS=-L. -llcloudlib -lcmpsc311 -lgcrypt -lcurl
AR=ar



# Suffix rules
.SUFFIXES: .c .o

.c.o:
	$(CC) $(CFLAGS)  -o $@ $<
	
# Files
OBJECT_FILES=	lcloud_sim.o \
				lcloud_filesys.o \
				lcloud_cache.o
				
# Productions
all : lcloud_sim

# Check environment dependencies
prebuild:
	./cmpsc311_prebuild

lcloud_sim : prebuild $(OBJECT_FILES)
	$(CC) $(LINKARGS) $(OBJECT_FILES) -o $@ $(LIBS)

clean : 
	rm -f lcloud_sim $(OBJECT_FILES) 
	
