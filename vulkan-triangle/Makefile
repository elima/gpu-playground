TARGET=vulkan-triangle

GLSL_VALIDATOR=../glslangValidator

VULKAN_SO_NAME=vulkan_intel
VULKAN_SO_PATH=~/devel/build/lib

all: Makefile $(TARGET) vert.spv frag.spv

vert.spv: shader.vert
	$(GLSL_VALIDATOR) -V shader.vert

frag.spv: shader.frag
	$(GLSL_VALIDATOR) -V shader.frag

$(TARGET): main.c vert.spv frag.spv
	gcc -ggdb -O0 -Wall -std=c99 \
		-DCURRENT_DIR=\"`pwd`\" \
		`pkg-config --libs --cflags xcb` \
		-L $(VULKAN_SO_PATH) \
		-l$(VULKAN_SO_NAME) \
		-o $(TARGET) \
		main.c

clean:
	rm -f $(TARGET) vert.spv frag.spv
