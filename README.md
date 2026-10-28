# S5L8702 FMISS Peripheral Bytecode Tools and Documentation
Tools and notes for the S5L8702 FMISS coprocessor: the small VM at NAND
controller `0x38A00000+0xC00` that Apple's Nano 3G firmware uses to talk
to flash.

The original reasons to reverse it were a Nano 3G NAND capacity project
and a Rockbox port that had no working storage. Both of those are no
longer blocked on this ISA:

- Factory 4 GB and 8 GB units share the same VFL/FTL; 8 GB is another
  chip-table row (Hynix `A555D5AD`), not a new FMISS program. See
  [NAND.md](NAND.md).
- Rockbox talks to the same FMC registers from ARM (BootROM
  NANDReadPage, not FMISS bytecode). The map and wait opcode in
  [Documentation.md](Documentation.md) are what that driver needed.

This tree is still the disassembler and builder for Apple's FMISS
programs. Opcode coverage of the five production blobs is complete
enough to use; leftover `?` marks are the unused opcodes, not missing
NAND operations.

The instruction descriptions started as device tests against the
diagnostic NAND LBA screen. Treat anything still hedged in
Documentation.md as a guess; NAND.md is the later hardware picture.

## Building
See the `Makefile` to see how the tools are built.

## Tools
### `s5l8702_objdump`
The disassembler behaves more like `objdump` in that it prints columns of information telling you what's going on in sequential steps, showing you branch labels, and marking instructions that have yet to be confirmed. Production NAND programs no longer need a `?` on `0x07` (wait on `FMCSTAT`). It prints the representation of the instruction as written in the documentation. As an argument, it takes a binary file of raw FMISS instructions.

### `s5l8702_builder`
This isn't an assembler, but rather organizes and formats the instructions correctly based on the writing format in the documentation. It also constructs the preamble and ending bit (error handling?) that don't seem special, but does appear in every part of the firmware. It's far less refined than `objdump` simply because I've been using it in the [Compiler Explorer](https://godbolt.org/) to generate test code quickly.

## OK But Where Is The Code To Disassemble?
I can't put it here because it's Apple code, but if you have a decrypted NOR EFI bootloader, you can get them at the following locations (and verify them with these hashes). These are **file offsets in the decrypted EFI body**, not BootROM or raw SPI NOR addresses; see [NAND.md](NAND.md).
 - Erase Page: at: `0x85B0`, length: `0x400`, SHA1: `df1e15b83bc718d72d9fe86f167a1412cd71d8cf`
 - Read Page: at: `0x89B0`, length: `0xC58`, SHA1: `5d977654e55c04ddbb6b2e0ef5a8b9d04162ef7e`
 - Read ID: at: `0x9608`, length: `0x210`, SHA1: `8d29b032afdcc550c8ad3b5c9ef800d6fc7af9b0`
 - Read Page No ECC: at: `0x9818`, length: `0x448`, SHA1: `8e4b2d52d2eb2fec86efe93c6126852bba58d69d`
 - Write Page: at: `0x9C60`, length: `0x958`, SHA1: `f8b77c1aa4a25ed7b2fae5cf535f79e5c21c06c1`

## License
The Ghidra processor module in [`ghidra-processor/`](ghidra-processor/) is licensed under the MIT License; see [`ghidra-processor/LICENSE`](ghidra-processor/LICENSE).

The rest of the code (the disassembler, builder, and assembler tools) is GPL 3.0, and the research, documentation, and everything else in this repository is licensed under [CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/).
