CC=gcc -std=c99 -Wall -DVIEWCGI

LINKER=link
LINKCON = /nologo

WWW_DIR ?= ../../www

all:objdir view.cgi wait_change.cgi tb.cgi $(WWW_DIR)/realtime.html $(WWW_DIR)/favicon.ico $(WWW_DIR)/showpic.js $(WWW_DIR)/styledir.css

OBJ = obj

OBJECTS_BROWSE = $(OBJ)/view.o \
	$(OBJ)/showdir.o    \
	$(OBJ)/actagram.o    \
	$(OBJ)/showimage.o $(OBJ)/showpic.o \
	$(OBJ)/collectdir.o  \
	$(OBJ)/utility.o
    
OBJECTS_SHARED = $(OBJ)/jpgfile.o \
	$(OBJ)/exif.o \
	$(OBJ)/jpgqguess.o

$(OBJ)/%.o:../src/%.c ../src/jhead.h
	${CC} -O3 -Wall -c $< -o $@

$(OBJ)/%.o:%.c view.h
	${CC} -O3 -Wall -c $< -o $@

objdir:
	@mkdir -p obj

view.cgi: ${OBJECTS_BROWSE} ${OBJECTS_SHARED}
	${CC} -o view.cgi ${OBJECTS_BROWSE} ${OBJECTS_SHARED}
	cp view.cgi $(WWW_DIR)
	chmod +s $(WWW_DIR)/view.cgi

tb.cgi:	$(OBJ)/thumb_bat.o 
	${CC} -o tb.cgi $(OBJ)/thumb_bat.o -ljpeg
	cp tb.cgi $(WWW_DIR)

wait_change.cgi:	$(OBJ)/wait_change.o 
	${CC} -o wait_change.cgi $(OBJ)/wait_change.o 
	cp wait_change.cgi $(WWW_DIR)

$(WWW_DIR)/realtime.html: realtime.html
	cp realtime.html $(WWW_DIR)

$(WWW_DIR)/favicon.ico: favicon.ico
	cp favicon.ico  $(WWW_DIR)

$(WWW_DIR)/showpic.js: showpic.js
	cp showpic.js $(WWW_DIR)

$(WWW_DIR)/styledir.css: styledir.css
	cp styledir.css $(WWW_DIR)
