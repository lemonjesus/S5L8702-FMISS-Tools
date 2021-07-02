all: s5l8702_builder s5l8702_objdump

s5l8702_builder:
	g++ -std=c++11 s5l8702_builder.cpp -o s5l8702_builder

s5l8702_objdump:
	gcc -std=c99 s5l8702_objdump.c -o s5l8702_objdump

clean:
	@rm -f s5l8702_builder s5l8702_objdump
