all: Makefile
	make -C render-nodes-minimal all
	make -C vulkan-minimal all
	make -C vulkan-triangle all

clean:
	make -C render-nodes-minimal clean
	make -C vulkan-minimal clean
	make -C vulkan-triangle clean
