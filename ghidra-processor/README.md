# Ghidra Processor module for FMISS bytecode

Installation: copy FMISS to `<ghidra-directory>/Ghidra/Processors/FMISS`.

Usage: when loading a file, select the FMISS architecture. Load at address 0. Create a volatile memory segment for the NAND controller at 0x38a00000, 0x1000 bytes long.
