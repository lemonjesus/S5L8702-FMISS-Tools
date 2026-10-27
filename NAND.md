# Nano 3G NAND, after the FMISS work

This repo started as ISA notes for the S5L8702 FMISS coprocessor, with two
practical goals: talk to a different NAND chip, and get Rockbox a working
storage stack. Those are far enough along that the remaining FMISS work is
maintenance of this disassembler, not blocking either goal.

What follows is the hardware and firmware picture that fell out of that
work, plus a later Rockbox port on the same SoC. It does not replace
[Documentation.md](Documentation.md); it says how that ISA sits on the
chip, where the bytecode actually lives, and what the NAND *is*.

## Two ways to move a page

`0x38A00000` is one NAND controller block. Apple firmware talks to it
mostly through FMISS. The BootROM, and the Rockbox driver that copied the
BootROM, poke the same FMC registers from ARM.

| Path | Who uses it | How |
|---|---|---|
| FMISS bytecode | EFI bootloader, RetailOS (`osos`), diagnostics | ARM writes a program pointer and kicks `FMI_SS_START`. The VM at `+0xC00` issues the bus cycles. |
| Direct FMC | BootROM, Rockbox `nand-nano3g.c` | ARM writes `FMCTRL` / `FMCMD` / `FMADDR*` and waits on `FMCSTAT`, same registers FMISS opcode `0x01`/`0x07` would touch. |

FMISS is not a library in NOR that later images call. Each Apple image
carries its own copy of the programs. Rockbox does not execute FMISS at
all for storage; it still needs this map, because the BootROM sequences
are the same controller.

Useful BootROM entry points (Nano 3G DFU window at `0x20000000`):

- `0x200096e4` bank reset
- `0x20009614` ready wait
- `0x20009910` NANDReadPage
- `0x2000ac1c` per-chunk ECC helper

wInd3x's `NANDIdentify` is the READ ID sequence the Rockbox driver
followed for chip detect.

Apple's *firmware* still programs and erases through FMISS sequencer
programs in DRAM (1.1.3: erase around `0x22008c30`, program around
`0x22009e98`, two-plane rows around `0x2200ac38`). Disassembling those
with `s5l8702_objdump` is how you see which FMC bits a write actually
needs. You do not have to run that bytecode from Rockbox once the
register sequence is known.

## Where the blobs in the README live

The offsets in this repo's README (`0x85B0` erase, `0x89B0` read, …,
`0x9C60` write) are **file offsets in the decrypted EFI bootloader
body**, not BootROM addresses and not raw SPI NOR addresses.

During DFU:

- `0x20000000` is the 64 KiB BootROM (also at `0x0`)
- the 1 MiB utility NOR is SPI flash on SPI0 (SysCfg at 0, IMG1
  bootloader at `0x8000`)

Decrypt that IMG1 (or an IPSW `osos`) and search by the SHA-1s in the
README. `osos` contains the same programs at different offsets.

## Controller map

Offsets from `0x38A00000`, matching [Documentation.md](Documentation.md)
and the names in `s5l8702_explainer.c`. Rockbox's `s5l87xx.h` uses the
Samsung-style `FM*` names on the same addresses.

| Offset | Role |
|---|---|
| `0x0000`–`0x03FF` | FMC: `FMCTRL0/1`, `FMCMD`, `FMADDR*`, `FMANUM`, `FMDNUM`, `FMCSTAT`, FIFO |
| `0x0400` | Possible second FMC. Unused by the Rockbox driver. |
| `0x0800`–`0x0BFF` | ECC / transfer assist. Rockbox uses `FMTRANS0` (`0x80C`), `FMUNK810` (`0x810`), `FMTRANS1` (`0x814`), `FMTRANSSTAT` (`0x840`). |
| `0x0C00`–`0x0CFF` | FMISS control (program pointer, start, GPRs if mapped) |
| `0x0D00`– | FMISS parameter RAM |

Opcode `0x07` is a wait on `FMCSTAT` bits. That was the last production
opcode this disassembler did not treat as confirmed. The BootROM
ready-wait and NANDReadPage do the same poll from ARM; the bit guesses
in Documentation.md are the right model.

`FMCTRL0 = 0x43803` is Apple's timing for bank 0 (`TUNK1=4`, `TWP=3`).
Chip enables for banks 1–3 only work after `gpio_preinit()` muxes
`PCON(9)`; the BootROM does not do that, which is why a DFU helper that
only talks to CE0 will under-count banks. PMU register `0x10` must keep
bit 2 set or the NAND dies.

The controller moves a 2 KiB page as **four 512-byte chunks**. ECC
verdicts on a written page are normally "corrected"; erased pages read
clean. That is a controller property, not a chip-id property.

## What Apple actually fitted

Every Nano 3G is model A1236. Capacity is not in the USB/DFU identity;
it is the NAND READ ID plus how many chip enables answer.

Apple's chip table in RetailOS 1.1.3 (at `0x089269bc` in that image)
holds the id, chip enables, blocks per enable, pages per block, spare
bytes, timings, user blocks, and a program mode byte. Rockbox only
enables rows that have been used on hardware. `utils/ipodnano3g/nandcheck`
in the Rockbox tree is a DFU image that reports the id without writing
anything; that is the right way to add a new row.

Geometry that matters for a 2 KiB-page part:

```
vflspares = (blocks - userblocks - 23 * planes) / planes
```

`planes` is **2 for this VFL**, not a property of the NAND die. The
context has one remap table per plane (820 slots between them). Program
mode 8 means whole rows are programmed two-plane (`0x80` / `0x81` /
`0x11` / `0x10`).

| id (LE) | Maker | Typical SKU | CEs | Pages/block | Blocks / userblocks | vflspares |
|---|---|---|---|---|---|---|
| `A514D3AD` | Hynix | 4 GB | 4 | 128 | 4096 / 3872 | 89 |
| `A555D5AD` | Hynix | 8 GB | 4 | 128 | 8192 / 7744 | 201 |

Same VFL, same two-plane program, twice the blocks per CE. The 8 GB
unit is a factory SKU, not a "new FMISS program". A chip-swap only
works if the id is in that table (or you add a measured row) **and**
the page size is still 2 KiB. Apple also shipped 4 KiB-page parts;
those need a different page path (`NAND_PAGE_SIZE` is 2048 in the
current driver) and are not solved by this ISA work.

Other 2 KiB ids exist in Apple's table (Micron `A5D5D52C`, Samsung
`2555D5EC`, Intel `A5D5D589`, …). Treat them as untested until a
nandcheck archive says otherwise.

## FTL (Whimory)

Logical storage is Apple's FTL, the Nano 3G variant of what Rockbox
already decoded on the Nano 2G. Roughly:

- **VFL** remaps a virtual block of a bank to a reserved block from
  that plane's spare pool (`0xFFF0` free, `0xFFFF` bad).
- A **superblock** is the same virtual block of every bank, both
  planes. Page index inside it is
  `page * banks * planes + plane * banks + bank`.
- **FTL** maps logical pages through a block map plus log blocks.
  `ftl_sync` / Apple's `FTL_Flush` folds logs and commits. A mount
  after a crash rebuilds from the medium.

4 GB needs about half of `FTL_MAX_SB` 4096; 8 GB is still inside that
cap (~3895 superblocks, 3872 logical blocks). Sector size towards the
filesystem is 2048 bytes.

Writes that only hit CE0, or that skip `ftl_sync` before a DFU reboot,
look fine in RAM and vanish after reset. That is an FTL/commit issue,
not an FMISS encoding issue.

## Rockbox

The port that uses this map:

- Gerrit **7856–7860**: FMC registers, PMU bit, NAND driver, FTL,
  storage
- **7866**: nandcheck DFU + collector (read-only, all Apple rows)
- **7862**: `mks5lboot` NOR install (same on 4 GB and 8 GB; NAND is not
  visible in DFU)

Enabling a new 2 KiB chip is a row in `nand_chip_table[]`, not a new
FMISS binary. 8 GB Hynix `A555D5AD` is that kind of row.

## What this repo is for now

Keep using `s5l8702_objdump` / `s5l8702_builder` on Apple's programs
when a register sequence is still opaque (especially the ECC window at
`+0x800` and any 4 KiB-page path). The ISA coverage of production
opcodes is enough to do that.

Do not expect FMISS to be the thing Rockbox runs on every page. The
research that made a Rockbox driver possible was: this map, the wait
opcode, and the proof that Apple's write program is "just" FMC
register traffic the BootROM already knew how to do for reads.
