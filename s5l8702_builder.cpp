#include <stdio.h>
#include <vector>
#include <stdint.h>

void pinst(uint64_t inst) {
    uint32_t x, y;
    x = __builtin_bswap32(inst >> 32);
    y = __builtin_bswap32(inst);
    printf("%08X%08X\n", x, y);
}

int main() {
    std::vector<uint64_t> instructions;

    // Read in params (r5 = pointer to bank number array, r6 = pointer to bank count int (8), r7 = pointer to dest array for ids)
    instructions.push_back(0x04070D08FFFFFFFF); // read r7 = *(0D08) UNKNOWN
    instructions.push_back(0x04060D0CFFFFFFFF); // read r6 = *(0D0C) UNKNOWN
    instructions.push_back(0x04050D10FFFFFFFF); // read r5 = *(0D10) UNKNOWN

    // put 0x6968 into a register, increment it to 0x6969 to test adding
    instructions.push_back(0x0500000000000001); // read r0 = 0x00000001
    instructions.push_back(0x0501000000000000); // read r1 = 0x00000000
    instructions.push_back(0x1300000000000001); // r0 << 0x1
    instructions.push_back(0x0C01000100000001); // r1 = r1 - 0x1
    instructions.push_back(0x1701000500000048); // if r1 > 1, jump to 0x00F0

    // read it into the dest buffer
    instructions.push_back(0x1100000700000000); // store *r7 = r0

    // build and output the program, including preamble and exception handler(?)
    uint32_t progsize = (instructions.size()+5) * 8;

    pinst(0x01000C0C000000FF); // set pending interrupts?
    pinst(0x01000C1000000001); // unknown
    pinst(0x01000C5800000004); // unknown
    pinst(0x01000C4C00000000 | progsize); //seems to be the length of the program before the first return, maybe an error handler?

    for(uint64_t instruction : instructions) {
        pinst(instruction);
    }

    // begin the end
    pinst(0x0000000000000000); // return

    pinst(0x04020C3CFFFFFFFF); // read from a seemingly unused register?
    pinst(0x04000000000001FE); // read some bits from FMC_CTRL0
    pinst(0x1400000000000001); // we don't care about the bottom byte, make room for r6
    pinst(0x0A060006FFFFFF00); // r6 is our loop counter, so take the top 3-bytes (?)
    pinst(0x0B06000000000000); // and combine it with r0
    pinst(0x04000C5000000001); // read 0xC50
    pinst(0x01000C5000000001); // write 1 to 0xC50 ("an error occured" register?)
    pinst(0x0B06000608000000); // or another bit into r6
    pinst(0x0000000000000000); // and then do nothing with it?

    return 0;
}

