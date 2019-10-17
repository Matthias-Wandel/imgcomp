CC=gcc
CFLAGS=

LINKER=link
LINKCON = /nologo

all:view.cgi tb.cgi

OBJ = obj

OBJECTS_MAKEHTML = $(OBJ)/view.o \
	$(OBJ)/makehtml.o    \
	$(OBJ)/collectdir.o  \
	$(OBJ)/utility.o

OBJECTS_SHARED = $(OBJ)/jpgfile.o \
	$(OBJ)/exif.o

$(OBJ)/%.o:../src/%.c ../src/jhead.h
	${CC} -O3 -Wall -c $< -o $@

$(OBJ)/%.o:%.c view.h
	${CC} -O3 -Wall -c $< -o $@

view.cgi: ${OBJECTS_MAKEHTML} ${OBJECTS_SHARED}
	${CC} -o view.cgi ${OBJECTS_MAKEHTML} ${OBJECTS_SHARED}
	cp view.cgi ../../www
	chmod +s ../../www/view.cgi

tb.cgi:	$(OBJ)/thumb_bat.o 
	${CC} -o tb.cgi $(OBJ)/thumb_bat.o -ljpeg
	cp tb.cgi ../../www
