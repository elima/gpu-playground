TARGET=render-nodes-minimal

all: Makefile $(TARGET)

$(TARGET): main.c
	gcc -ggdb -O0 -Wall -std=c99 \
		main.c \
		`pkg-config --libs --cflags glesv2 egl gbm` \
		-o $(TARGET)

clean:
	rm -f $(TARGET)
