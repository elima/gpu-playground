TARGET=render-nodes-minimal

all: Makefile $(TARGET)

$(TARGET): main.c
	gcc -ggdb -O0 -Wall -std=c99 \
		`pkg-config --libs --cflags glesv2 egl gbm` \
		-o $(TARGET) \
		main.c

clean:
	rm -f $(TARGET)
