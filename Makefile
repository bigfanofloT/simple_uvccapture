CC=arm-himix200-linux-gcc
CPP=arm-himix200-linux-g++

APP_BINARY=simple_uvccapture

WARNINGS = -Wall

CFLAGS = -std=gnu99 -O2 -DLINUX $(WARNINGS)
CPPFLAGS = $(CFLAGS)

OBJECTS= simple_uvccapture.o v4l2uvc.o


all:    uvccapture

clean:
	@echo "Cleaning up directory."
	rm -f *.a *.o $(APP_BINARY) core *~ log errlog

# Applications:
uvccapture: $(OBJECTS)
	$(CC)   $(OBJECTS) $(XPM_LIB) $(MATH_LIB) -o $(APP_BINARY)
