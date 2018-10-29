.PHONY: all clean

CFLAGS = -std=c99 -g -ggdb -O0 -Wall

LDFLAGS = -lm

PKG_CONFIG_LIBS = \
	glfw3 \
	glesv2 \
	libpng \
	libjpeg \
	$(NULL)

CFLAGS += $(shell pkg-config --cflags $(PKG_CONFIG_LIBS))
LDFLAGS += $(shell pkg-config --libs $(PKG_CONFIG_LIBS))

OBJS = png.o jpeg.o image.o

all: gl-image-loader

png.o: png.c png.h
jpeg.o: jpeg.c jpeg.h
image.o: image.c image.h

gl-image-loader: main.c $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm -f ./*.o
	rm -f gl-image-loader
