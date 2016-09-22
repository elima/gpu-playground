all: Makefile
	make -C render-nodes-minimal all
	make -C vulkan-minimal all

clean:
	make -C render-nodes-minimal clean
	make -C vulkan-minimal clean
