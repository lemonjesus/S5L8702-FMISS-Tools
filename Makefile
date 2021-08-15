all: s5l8702_builder s5l8702_objdump s5l8702_simulator

s5l8702_builder: s5l8702_builder.cpp
	g++ -std=c++11 s5l8702_builder.cpp -o s5l8702_builder

s5l8702_objdump: s5l8702_explainer.o s5l8702_objdump.c
	gcc -std=c99 s5l8702_explainer.o s5l8702_objdump.c -o s5l8702_objdump

s5l8702_simulator: s5l8702_explainer.o s5l8702_simulator.c
	gcc -std=c99 s5l8702_explainer.o s5l8702_simulator.c -o s5l8702_simulator -lreadline

s5l8702_explainer.o: s5l8702_explainer.c s5l8702_explainer.h
	gcc -c -std=c99 s5l8702_explainer.c -o s5l8702_explainer.o

clean:
	@rm -f s5l8702_builder s5l8702_objdump *.o
