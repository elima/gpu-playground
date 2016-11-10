TARGET=vulkan-triangle

GLSL_VALIDATOR=../glslangValidator

all: $(TARGET) vert.spv frag.spv

vert.spv: shader.vert
	$(GLSL_VALIDATOR) -V shader.vert

frag.spv: shader.frag
	$(GLSL_VALIDATOR) -V shader.frag

$(TARGET): Makefile main.c vert.spv frag.spv \
	common/wsi.h common/wsi-xcb.c \
	common/vk-api.h common/vk-api.c
	gcc -ggdb -O0 -Wall -std=c99 \
		-DCURRENT_DIR=\"`pwd`\" \
		`pkg-config --libs --cflags xcb` \
		-lvulkan \
		-DVK_USE_PLATFORM_XCB_KHR \
		-o $(TARGET) \
		common/wsi-xcb.c \
		common/vk-api.c \
		main.c

clean:
	rm -f $(TARGET) vert.spv frag.spv
