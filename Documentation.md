# Documentation
The instructions that are confirmed below were confirmed by testing simple sequences and extracting the value into the NAND_LBA to read in the diagnostic menu as I've been doing with the NAND ID previously. I haven't figured out all of the instructions, but across all five subroutines found in the EFI firmware, this `objdump` currently achieves 88.2% coverage of used instructions.

Registers are another thing altogether. I've been able to deduce what some of them do, but not all of them are clear to me. Some of the bitfields potentially do not match what can be found in the datasheet for the S5L8700X. That makes sense, they're different chips, but I've only guessed at the bitfield for `FMC_STATUS` in instruction `0x07` below.

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

## Immediate Memory Offsets
Sometimes instructions will use their 16-bit immediate to store a memory location. This is offset from the NAND/FMISS DMA Base address (`0x38A00000`)

## Registers
There are 8 registers, I've named them `r0` through `r7`. They all seem to be general purpose. They might be mapped into memory from `0x38A00C18` to `0x38A00C34` respectively, but that's _currently unconfirmed_.

## Op-codes

### `0x00` RETURN or TERMINATE *(mostly confirmed)*
This instruction, always without parameters, seems to indicate that the bytecode is finished executing. At the end of normal program flow, an `0x0000000000000000` is seen.

### `0x01` Write Immediate to Memory
Used to write an immediate value to a memory location relative to the DMA base address. This is often used to write directly to NAND controller registers.

For example:
```
0x01000030000001FF
```

seems to mean:
```
*(0x38A00030) = 0x000001FF
```

### `0x02` Write Register to Memory
Used to write register contents to a memory location relative to the DMA base address.

For example:
```
0x02010D1000000000
```

seems to mean:
```
*(0x38A00D10) = r1
```

It appears that the immediate is not used in this function, but that hasn't been tested.

### `0x03` Dereference Pointer In Register
Used to dereference the contents of a register. For example, if `r1` has an address in it, you can get the value at the address into `r0` by saying

```
0x0300000100000000
```

which seems to mean

```
r0 = *(r1)
```

If you try to dereference something that isn't a valid pointer, or points to an unreadable spot of memory, the iPod will lock up.

### `0x04` Read from Memory into Register
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

### `0x08` *Unknown, possibly unused*
### `0x09` *Unknown, possibly unused*

### `0x0A` AND Two Registers and an Immediate
`AND` two registers (one of which is the destination register) and a bitmask together. The second register is given in the 16-bit immediate.

For example:
```
0x0A060007FFFFFF00
```

could mean:
```
r6 = r6 & r7 & 0xFFFFFF00
```
Sometimes there are `AND`s with a null bitmask `0x00000000`. It's unclear if that means the bitmask is disabled in this case (then why wouldn't you just set the bitmask to `0xFFFFFFFF`) or that's intentional for setting the destination to zero (but then why not just write a zero immediate?). It's weird, so I'm not so sure about this one.

### `0x0B` OR Two Registers and an Immediate
`OR` two registers (one of which is the destination register) and a bitmask together. The second register is given in the 16-bit immediate.

For example:
```
0x0B060007FFFFFF00
```

seems to mean:
```
r6 = r6 | r7 | 0xFFFFFF00
```
This one makes sense to have two different registers and a null bitmask, or the same register twice and a non-null bitmask. I haven't seen a case where the registers are the same and the bitmask is null, as that would just be a noop.

### `0x0C` Add a Register and an Immediate 
Adds the 32-bit immediate to the content stored in the register referenced by the 16-bit immediate and stores it in the register given in the 8-bit immediate.

For example:
```
0x0C01000200000004
```

seems to mean:
```
r1 = r2 + 4
```

### `0x0D` Subtract an Immediate from a Register
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

### `0x0F` *Unknown, possibly unused*
### `0x10` *Unknown, possibly unused*

### `0x11` Store a Register Value to a Memory Location Pointed to by a Register
This instruction saves a value to a place in memory pointed to by another register.

For example:
```
0x1100000700000000
```

seems to mean:
```
*r7 = r0
```

### `0x12` *Unknown, possibly unused*

### `0x13` Left Shift a Register by an Immediate
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

### `0x14` Right Shift a Register by an Immediate
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


### `0x17` Some kind of jump *(unconfirmed)*
Based on how it looks the same as the other jump instruction, this one is probably some kind of jump too. But I've tested the basics (`jeq`, `jgt`, `jlt`, etc.) and I cant quite figure out what this one does. Maybe it's some kind of "jump if zero" but that has yet to be tested.

For example:
```
0x17040002000008F0
```

### `0x18` *Used, Not yet investigated*

### `0x19` *Used, Not yet investigated*
