#-------------------------------------------------------------
# imgcomp makefile for Linux (compile on raspberry pi)
# Matthias Wandel 2023
#-------------------------------------------------------------
OBJ=obj
SRC=src

# with debug:
#CFLAGS:= $(CFLAGS) -O3 -Wall -g

CFLAGS:= $(CFLAGS) -std=c99 -O3 -Wall
all: objdir imgcomp

objdir:
	@mkdir -p obj

objs = $(OBJ)/main.o $(OBJ)/config.o $(OBJ)/compare.o $(OBJ)/compare_util.o $(OBJ)/jpeg2mem.o \
	$(OBJ)/jpgfile.o $(OBJ)/exif.o $(OBJ)/start_camera_prog.o $(OBJ)/util.o $(OBJ)/send_udp.o $(OBJ)/exposure.o

$(OBJ)/jpgfile.o $(OBJ)/exif.o $(OBJ)/start_camera_prog.o: $(SRC)/jhead.h
$(OBJ)/main.o $(OBJ)/config.o: $(SRC)/config.h

$(OBJ)/%.o:$(SRC)/%.c $(SRC)/imgcomp.h
	${CC} $(CFLAGS) -c $< -o $@

imgcomp: $(objs)
	${CC} -o imgcomp $(objs) -ljpeg -lm

clean:
	rm -f $(objs) imgcomp

