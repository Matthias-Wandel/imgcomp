CC=gcc
CFLAGS=

LINKER=link
LINKCON = /nologo

all:view.cgi tb.cgi

OBJ = obj

OBJECTS_MAKEHTML = $(OBJ)/main.o \
	$(OBJ)/makehtml.o    \
	$(OBJ)/collectdir.o  \
	$(OBJ)/utility.o


$(OBJ)/%.o:%.c view.h
	${CC} -O3 -Wall -c $< -o $@

view.cgi: ${OBJECTS_MAKEHTML}
	${CC} -o view.cgi ${OBJECTS_MAKEHTML}
	cp view.cgi ../../www

tb.cgi:	$(OBJ)/thumb_bat.o
	${CC} -o tb.cgi $(OBJ)/thumb_bat.o
	cp tb.cgi ../../www
