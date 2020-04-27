#-------------------------------------------------------------
# imgcomp makefile for Linux (compile on raspberry pi)
# Matthias Wandel 2015
#-------------------------------------------------------------
OBJ=obj
SRC=src

export PREFIX ?= /usr/local
export BINDIR ?= /bin

export USER ?= pi
export CONFIGDIR ?= /home/${USER}/.config/imgcomp/
export CONFIGFILE ?= imgcomp.conf

SYSTEMDSERVICEDIR ?= ${shell pkg-config systemd --variable=systemdsystemconfdir}
ifeq (,${SYSTEMDSERVICEDIR})
    $(error Could not retrieve systemd user service directory from pkg-config)
endif

# with debug:
#CFLAGS:= $(CFLAGS) -O3 -Wall -g

CFLAGS:= $(CFLAGS) -O3 -Wall

all: objdir imgcomp

objdir:
	@mkdir -p obj

objs = $(OBJ)/main.o $(OBJ)/config.o $(OBJ)/compare.o $(OBJ)/jpeg2mem.o \
	$(OBJ)/jpgfile.o $(OBJ)/exif.o $(OBJ)/start_raspistill.o $(OBJ)/util.o $(OBJ)/send_udp.o

$(OBJ)/jpgfile.o $(OBJ)/exif.o $(OBJ)/start_raspistill.o: $(SRC)/jhead.h
$(OBJ)/main.o $(OBJ)/config.o: $(SRC)/config.h

$(OBJ)/%.o:$(SRC)/%.c $(SRC)/imgcomp.h
	${CC} $(CFLAGS) -c $< -o $@

imgcomp: $(objs)
	${CC} -o imgcomp $(objs) -ljpeg -lm

clean:
	rm -f $(objs) imgcomp imgcomp.service

configdir:
	@mkdir -p ${CONFIGDIR}

install: imgcomp
	@install -Dm755 imgcomp ${PREFIX}${BINDIR}/imgcomp

uninstall:
	rm -f ${PREFIX}${BINDIR}/imgcomp

service: imgcomp.service.in
	@envsubst < imgcomp.service.in > imgcomp.service

install-service: service configdir install
	$(info Installing imgcomp.service to ${SYSTEMDSERVICEDIR}/imgcomp.service)
	@install -Dm644 imgcomp.service ${SYSTEMDSERVICEDIR}/imgcomp.service
	$(info Run 'sudo systemctl daemon-reload' to register the imgcomp service)

uninstall-service:
	rm -f ${SYSTEMDSERVICEDIR}/imgcomp.service
	$(info Run 'sudo systemctl daemon-reload' to remove the imgcomp service)
