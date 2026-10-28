# Ghidra Processor module for FMISS bytecode

Installation: copy FMISS to `<ghidra-directory>/Ghidra/Processors/FMISS`, then
compile the language: `<ghidra-directory>/support/sleigh FMISS/data/languages/fmiss.slaspec` (Ghidra also does this on first load).

Usage: import the bytecode as a raw binary, language `FMISS:LE:32:default`, base
address 0. The processor spec creates a volatile-style `NAND_MMIO` block at
`0x38a00000` (0x1000 bytes) and labels the known FMC registers.

Instructions are 8 bytes, so disassemble from address 0 (or run the Dump script);
jump targets are absolute offsets into the table.

Notes on the semantics:
* `wait N` spins on `FMC_STAT` (0x38a00048) until bit N is set.
* `mmio.ld`/`mmio.st` take an MMIO *offset* from a register; `mov [rs], rd` /
  `mov rd, [rs]` take a RAM pointer.
* `add`/`sub`/`shl`/`shr` with a zero immediate use the form `rd = rd op rs`.

## License
This Ghidra processor module is licensed under the MIT License; see [LICENSE](LICENSE).
