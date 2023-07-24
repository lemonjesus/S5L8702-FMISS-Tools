# Documentation
The instructions that are confirmed below were confirmed by testing simple sequences and extracting the value into the NAND_LBA to read in the diagnostic menu as I've been doing with the NAND ID previously. I haven't figured out all of the instructions, but across all five subroutines found in the EFI firmware, this `objdump` currently achieves 89.7% coverage of used instructions.

## Memory Regions and What They're Used For
The NAND DMA space can be broken up into several regions that do/are used for different things. All addresses below are offsets from the base address `0x38A00000` and extend until the next offset listed. These are educated guesses based on the code that uses them, and are not confirmed by any means.

* `0x0000` - NAND Controller registers. Seldom directly written to by the firmware, the FMISS layer uses these to control the NAND controller.
* `0x0400` - Possibly a second NAND controller? _Very unconfirmed._
* `0x0800` - NAND ECC calculations? Not much is known about this region, but it's used by the more FMISS programs.
* `0x0C00` - FMISS registers. This is how the FMISS layer communicates with the outside world. It's used to pass parameters into the FMISS layer.
* `0x0D00` - FMISS memory. This is where the FMISS layer stores its data. It's used to pass parameters into the FMISS layer, and is used by the FMISS layer to store data.

Specific guesses on individual registers and memory locations can be found in the explainer code.

## Instruction Format
Instructions executed by the FMISS Layer are – for all intents and purposes – 64-bit. The first 32-bits are an immediate, followed by other data. To make it look prettier, I’m reformatting the bytes so that they’re human readable left to right. For example:
```
0x3C0C0204FFFFFFFF
```
Will be written from here on out as:
```
0x04020C3CFFFFFFFF
```

Instructions written like this make sense given their structure:
```
0x 04 02 0C3C FFFFFFFF
   ↑  ↑  ↑    ↑
   │  │  │    └ 32-bit immediate
   │  │  └ 16-bit immediate or source register
   │  └ 8-bit destination register identifier
   └ 8-bit op-code
```
Exceptions are noted below. See the assembler for how the bytes get reorganized from the above format to the correct format.

## Memory Parameter Considerations
Some operands are memory locations, but the kind of memory location differs between instructions. I just came up with these names to differentiate between the two kinds of memory parameters:

 * **RAM Addresses** refer to a location in RAM. These are 32-bit addresses that usually start with `0x09******` or `0x89******`. These pointers are provided by the code that initiates the execution of FMISS.
 * **DMA Offset Addresses** refer to a location in the NAND DMA memory space. These are usually 16-bit values that are added to the NAND DMA Base address (`0x38A00000`) to get the actual address. These are used for reading and writing to NAND registers or to FMISS memory (unconfirmed if this is what we should call it, but it's how parameters are passed into the FMISS layer, usually starting with `0x0D**` or FMISS Memory as mentioned above).

For example, the difference between instructions `0x11` and `0x19` is the kind of memory location that the provided register points to. `0x11` uses a RAM address, while `0x19` uses a DMA offset address.

## Registers
There are 8 registers, I've named them `r0` through `r7`. They all seem to be general purpose. They might be mapped into memory from `0x38A00C18` to `0x38A00C34` respectively, but that's _currently unconfirmed_.

## Op-codes

### `0x00` RETURN or TERMINATE *(mostly confirmed)*
This instruction, always without parameters, seems to indicate that the bytecode is finished executing. At the end of normal program flow, an `0x0000000000000000` is seen.

### `0x01` Write Immediate to DMA Memory
Used to write an immediate value to a memory location relative to the DMA base address. This is often used to write directly to NAND controller registers.

For example:
```
0x01000030000001FF
```

seems to mean:
```
*(0x38A00030) = 0x000001FF
```

### `0x02` Store Register Value to DMA Memory Given by Immediate Offset
Used to write register contents to a memory location relative to the DMA base address. Compare to `0x19` which takes the offset from a register instead of an immediate.

For example:
```
0x02010D1000000000
```

seems to mean:
```
*(0x38A00D10) = r1
```

It appears that the immediate is not used in this function, but that hasn't been tested.

### `0x03` Dereference RAM Pointer In Register
Used to dereference a RAM address in a register. For example, if `r1` has an address in it, you can get the value at the address into `r0` by saying

```
0x0300000100000000
```

which seems to mean

```
r0 = *(r1)
```

If you try to dereference something that isn't a valid pointer, or points to an unreadable spot of memory, the iPod will lock up.

### `0x04` Read from DMA Memory into Register
Read a value from a memory location relative to the DMA base address.

For example:
```
0x04070D08FFFFFFFF
```

seems to mean:
```
r7 = *(0x38A00D08)
```
The 32-bit immediate is a bit-mask `and`ed with the contents read before storing it in the register.

### `0x05` Read Immediate into Register
Sets a register to the 32-bit immediate.

For example:
```
0x0500000000006968
```

seems to mean:
```
r0 = 0x00006968
```
The 16-bit immediate doesn't seem to be used, but that's *unconfirmed*.

### `0x06` Read Register into Register
This one appears to be a simple `mov` like instruction.

For example:
```
0x0600000100000000
```

seems to mean:
```
r0 = r1
```
The 32-bit immediate doesn't seem to be used, but that's *unconfirmed*.

### `0x07` Wait for NAND Status Bit to be Set *(very unconfirmed)*
This one is a wild guess based on when it's used. I think it waits for a certain bit in the FMCSTATUS register of the NAND peripheral to be set, but I don't know for sure.

For example:
```
0x0701000000000000
```

could mean:
```
wait for flash ready on FMCSTAT[1]
```
but the format of the instruction would be wildly different from all of the other ones, as well as the fact that it blocks.

The specific bits waited for seem slightly out of line with what's documented in the datasheet for the S5L8700X. Here are my current guesses:

* `FMCSTAT[0]` - wait for command transfer completion?
* `FMCSTAT[1]` - wait for address transfer completion?
* `FMCSTAT[2]` - unknown, used after reading, maybe the status of a FIFO?
* `FMCSTAT[3]` - wait for data transfer completion?
* `FMCSTAT[4]` - wait for ECC completion?
* `FMCSTAT[5]` - unknown, used before writing to unknown `0xC**` registers

This is the last unconfirmed instruction that's used in any production FMISS programs.

### `0x0A` AND

This instruction has two forms:

#### Immediate == 0
`AND` the destination register with a source register, saving the result in the destination register. The source register is given in the 16-bit immediate.

For example:
```
0x0A06000700000000
```

could mean:
```
r6 = r6 & r7
```

#### Immediate != 0
`AND` a source register and a value together, saving the result in the destination register. The source register is given in the 16-bit immediate.

For example:
```
0x0A060007FFFFFF00
```

could mean:
```
r6 = r7 & 0xFFFFFF00
```

### `0x0B` OR
This instruction has two forms:

#### Immediate == 0
`OR` the destinations register with a source register, saving the result in the destination register. The source register is given in the 16-bit immediate.

For example:
```
0x0B06000700000000
```

seems to mean:
```
r6 = r6 | r7
```

#### Immediate != 0
`OR` a source register and a value together, saving the result in the destination register. The source register is given in the 16-bit immediate.

For example:
```
0x0B060007FFFFFF00
```

seems to mean:
```
r6 = r7 | 0xFFFFFF00
```

### `0x0C` Add 
This instruction has two forms:

#### Immediate == 0
Adds the number stored in the source register to the number in the destination register.

For example:
```
0x0C01000200000000
```

seems to mean:
```
r1 += r2
```

#### Immediate != 0
Adds the 32-bit immediate to the content stored in the register referenced by the 16-bit immediate and stores it in the register given in the 8-bit immediate.

For example:
```
0x0C01000200000004
```

seems to mean:
```
r1 = r2 + 4
```

### `0x0D` Subtract
This instruction has two forms:

#### Immediate == 0
Subtracts the number stored in the source register from the number in the destination register.

For example:
```
0x0D01000200000000
```

seems to mean:
```
r1 -= r2
```

#### Immediate != 0
Subtracts the 32-bit immediate from the content stored in the register referenced by the 16-bit immediate and stores it in the register given in the 8-bit immediate.

For example:
```
0x0D01000200000004
```

seems to mean:
```
r1 = r2 - 4
```

### `0x0E` Jump If Not Zero
If a register does not equal zero, then jump to the address given by the 32-bit immediate. The jump is absolute, meaning it's relative to the beginning of the bytecode, not relative to the instruction.

For example:
```
0x0E040000000008F0
```

seems to mean:
```
if r4 != 0, jump to 0x000008F0
```

### `0x11` Store a Register Value to a RAM Memory Location Pointed to by a Register
This instruction saves a value to a place in memory pointed to by another register. The address in the destination register must be an address in RAM, not in the NAND DMA region.

For example:
```
0x1100000700000000
```

seems to mean:
```
*r7 = r0
```

### `0x13` Left Shift
This instruction has two forms:

#### Immediate == 0
Left shifts the destination register by the value given in the source register.

For example:
```
0x1303000200000000
```

seems to mean:
```
r3 <<= r2
```

#### Immediate != 0
Left shifts a register given by the 16-bit immediate by the 32-bit immediate and saves it to the register given by the 8-bit immediate.

For example:
```
0x1303000200000001
```

seems to mean:
```
r3 = r2 << 1
```
There are cases where the shift value is zero, which would make this just an assignment which makes no sense.

### `0x14` Right Shift
This instruction has two forms:

#### Immediate == 0
Right shifts the destination register by the value given in the source register.

For example:
```
0x1403000200000000
```

seems to mean:
```
r3 >>= r2
```

#### Immediate != 0
Right shifts a register given by the 16-bit immediate by the 32-bit immediate and saves it to the register given by the 8-bit immediate.

For example:
```
0x1403000200000001
```

seems to mean:
```
r3 = r2 >> 1
```
There are cases where the shift value is zero, which would make this just an assignment which makes no sense. It's also unclear if this is a logical shift or an arithmatic shift.


### `0x17` Jump If Zero
If a register equals zero, then jump to the address given by the 32-bit immediate. The jump is absolute, meaning it's relative to the beginning of the bytecode, not relative to the instruction.

For example:
```
0x17040000000008F0
```

seems to mean:
```
if r4 == 0, jump to 0x000008F0
```

### `0x18` Load Register Value from DMA Memory Given by Offset in a Register
This instruction loads a value from a DMA offset given by the register specified in the 16-bit field. Compare to `0x04` which uses the 16-bit immediate to specify the offset.

For example:
```
0x1802000400000000
```

seems to mean:
```
*r4 = r2
```

If `r4` is `0xD10` then `r2` would be loaded from `0x38A00D10`.

### `0x19` Store Register Value to DMA Memory Given by Offset in a Register
This instruction stores a value to a DMA offset given by the register specified in the 16-bit field. The address in the destination register must be an offset into the NAND DMA region. Compare to `0x02` which uses the 16-bit immediate to specify the offset, and to `0x11` which expects the pointer to be in RAM.

For example:
```
0x1901000300000000
```

seems to mean:
```
*r3 = r1
```

If `r3` is `0xD10` then `r1` would be saved to `0x38A00D10`.
