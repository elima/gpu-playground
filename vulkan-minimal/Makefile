TARGET=vulkan-minimal

GLSL_VALIDATOR=../glslangValidator

all: $(TARGET) vert.spv frag.spv

vert.spv: shader.vert
	$(GLSL_VALIDATOR) -V shader.vert

frag.spv: shader.frag
	$(GLSL_VALIDATOR) -V shader.frag

$(TARGET): Makefile main.c vert.spv frag.spv
	gcc -ggdb -O0 -Wall -std=c99 \
		-DCURRENT_DIR=\"`pwd`\" \
		main.c \
		`pkg-config --libs --cflags xcb` \
		-lvulkan \
		-o $(TARGET)

clean:
	rm -f $(TARGET) vert.spv frag.spv
