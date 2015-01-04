#--------------------------------
# imgcomp makefile for Linux (raspberry pi)
#--------------------------------
OBJ=.
SRC=.
CFLAGS:= $(CFLAGS) -O3 -Wall

all: imgcomp

objs = $(OBJ)/cdjpeg.o $(OBJ)/djpeg.o $(OBJ)/wrppm.o 

$(OBJ)/%.o:$(SRC)/%.c
	${CC} $(CFLAGS) -c $< -o $@

imgcomp: $(objs) libjpeg/libjpeg.a jconfig.h
	${CC} -o imgcomp $(objs) libjpeg/libjpeg.a

clean:
	rm -f $(objs) imgcomp
