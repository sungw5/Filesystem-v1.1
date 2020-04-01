#
# CMPSC311 - LionCloud Device - Assignment #2
# Makefile - makefile to build the base system
#

# Locations

# Make environment
INCLUDES=-I.
CC=gcc
CFLAGS=-I. -c -g -Wall $(INCLUDES)
LINKARGS=-g
LIBS=-L. -llcloudlib -lcmpsc311 -lgcrypt -lcurl
AR=ar



# Suffix rules
.SUFFIXES: .c .o

.c.o:
	$(CC) $(CFLAGS)  -o $@ $<
	
# Files
OBJECT_FILES=	lcloud_sim.o \
				lcloud_filesys.o
				
# Productions
all : lcloud_sim

# Check environment dependencies
prebuild:
	./cmpsc311_prebuild

lcloud_sim : prebuild $(OBJECT_FILES)
	$(CC) $(LINKARGS) $(OBJECT_FILES) -o $@ $(LIBS)

clean : 
	rm -f lcloud_sim $(OBJECT_FILES) 
	