CC=gcc -std=c99 -DVIEWCGI

LINKER=link
LINKCON = /nologo

all:objdir view.cgi wait_change.cgi tb.cgi ../../www/realtime.html ../../www/favicon.ico ../../www/showpic.js

OBJ = obj

OBJECTS_BROWSE = $(OBJ)/view.o \
	$(OBJ)/showdir.o    \
    $(OBJ)/actagram.o    \
	$(OBJ)/showimage.o showpic.o \
	$(OBJ)/collectdir.o  \
	$(OBJ)/utility.o
    
OBJECTS_SHARED = $(OBJ)/jpgfile.o \
	$(OBJ)/exif.o

$(OBJ)/%.o:../src/%.c ../src/jhead.h
	${CC} -O3 -Wall -c $< -o $@

$(OBJ)/%.o:%.c view.h
	${CC} -O3 -Wall -c $< -o $@

objdir:
	@mkdir -p obj

view.cgi: ${OBJECTS_BROWSE} ${OBJECTS_SHARED}
	${CC} -o view.cgi ${OBJECTS_BROWSE} ${OBJECTS_SHARED}
	cp view.cgi ../../www
	chmod +s ../../www/view.cgi

tb.cgi:	$(OBJ)/thumb_bat.o 
	${CC} -o tb.cgi $(OBJ)/thumb_bat.o -ljpeg
	cp tb.cgi ../../www

wait_change.cgi:	$(OBJ)/wait_change.o 
	${CC} -o wait_change.cgi $(OBJ)/wait_change.o 
	cp wait_change.cgi ../../www

../../www/realtime.html: realtime.html
	cp realtime.html ../../www

../../www/favicon.ico: favicon.ico
	cp favicon.ico  ../../www

../../www/showpic.js: showpic.js
	cp showpic.js ../../www
