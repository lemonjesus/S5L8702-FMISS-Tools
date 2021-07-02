# S5L8702 FMISS Peripheral Bytecode Tools and Documentation
This repo is where I'll keep documentation and findings on this specific part of the S5L8702 architecture. Since my iPod Nano 3rd Generation memory expansion project relies on the firmware being able to communicate with the new NAND chip, and said firmware uses this blackbox peripheral to do the actual communication async from the rest of the code, this research is critical.

There are a lot of unanswered questions about how this part of the S5L8702 works, but gaining a better understanding of it will not only help me toward my goal of expanding the iPod Nano 3rd Generation's NAND capacity, but it will also get us closer to a full Rockbox port for the n3g since NAND support is one of the bigger missing pieces of the puzzle.

## Building
See the `Makefile` to see how the tools are built.

## Tools
### `s5l8702_objdump`
The disassembler behaves more like `objdump` in that it prints columns of information telling you what's going on in sequential steps, showing you branch labels, and marking instructions that have yet to be confirmed. It prints the representation of the instruction as written in the documentation. As an argument, it takes a binary file of raw FMISS instructions.

### `s5l8702_builder`
This isn't an assembler, but rather organizes and formats the instructions correctly based on the writing format in the documentation. It also constructs the preamble and ending bit (error handling?) that don't seem special, but does appear in every part of the firmware. It's far less refined than `objdump` simply because I've been using it in the [Compiler Explorer](https://godbolt.org/) to generate test code quickly.

## License
The code, research, documentation, and everything else in this repository is © 2021 by Tucker Osman licensed under [CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/). 