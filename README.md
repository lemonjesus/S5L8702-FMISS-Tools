# S5L8702 FMISS Peripheral Bytecode Tools and Documentation
This repo is where I'll keep documentation and findings on this specific part of the S5L8702 architecture. Since my iPod Nano 3rd Generation memory expansion project relies on the firmware being able to communicate with the new NAND chip, and said firmware uses this blackbox peripheral to do the actual communication async from the rest of the code, this research is critical.

There are a lot of unanswered questions about how this part of the S5L8702 works, but gaining a better understanding of it will not only help me toward my goal of expanding the iPod Nano 3rd Generation's NAND capacity, but it will also get us closer to a full Rockbox port for the n3g since NAND support is one of the bigger missing pieces of the puzzle.

**This is a work in progress, and as such, might be incomplete or just outright wrong.** These are all educated guesses. Unfortuantely I'm not that educated in the dark arts of mystery instruction set reverse engineering.

## Building
See the `Makefile` to see how the tools are built.

## Tools
### `s5l8702_objdump`
The disassembler behaves more like `objdump` in that it prints columns of information telling you what's going on in sequential steps, showing you branch labels, and marking instructions that have yet to be confirmed. It prints the representation of the instruction as written in the documentation. As an argument, it takes a binary file of raw FMISS instructions.

### `s5l8702_builder`
This isn't an assembler, but rather organizes and formats the instructions correctly based on the writing format in the documentation. It also constructs the preamble and ending bit (error handling?) that don't seem special, but does appear in every part of the firmware. It's far less refined than `objdump` simply because I've been using it in the [Compiler Explorer](https://godbolt.org/) to generate test code quickly.

## OK But Where Is The Code To Disassemble?
I can't put it here because it's Apple code, but if you have a decrypted NOR EFI bootloader, you can get them at the following locations (and verify them with these hashes):
 - Erase Page: at: `0x85B0`, length: `0x400`, SHA1: `df1e15b83bc718d72d9fe86f167a1412cd71d8cf`
 - Read Page: at: `0x89B0`, length: `0xC58`, SHA1: `5d977654e55c04ddbb6b2e0ef5a8b9d04162ef7e`
 - Read ID: at: `0x9608`, length: `0x210`, SHA1: `8d29b032afdcc550c8ad3b5c9ef800d6fc7af9b0`
 - Read Page No ECC: at: `0x9818`, length: `0x448`, SHA1: `8e4b2d52d2eb2fec86efe93c6126852bba58d69d`
 - Write Page: at: `0x9C60`, length: `0x958`, SHA1: `f8b77c1aa4a25ed7b2fae5cf535f79e5c21c06c1`

## License
The code is GPL 3.0, and the research, documentation, and everything else in this repository is licensed under [CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/). 
