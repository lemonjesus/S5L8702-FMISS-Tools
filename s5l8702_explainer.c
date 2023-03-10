#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "s5l8702_explainer.h"

void resolveJumps();

instruction* program;
uint32_t programSize;
jmptable jt;

void initalize_explainer(char* hexFile) {
    FILE* fp = fopen(hexFile, "r");
    fseek(fp, 0, SEEK_END);
    programSize = ftell(fp)/sizeof(instruction);
    fseek(fp, 0, SEEK_SET);

    program = (instruction*) malloc(programSize*sizeof(instruction));
    fread(program, programSize, sizeof(instruction), fp);
    fclose(fp);

    // transform to the readable format set forth in the docs
    for(uint32_t i = 0; i < programSize; i++) {
        program[i].top = __builtin_bswap32(program[i].top);
        program[i].bottom = __builtin_bswap32(program[i].bottom);
        program[i].raw = __builtin_bswap64(program[i].raw);
    }
    
    jt.jumps = malloc(sizeof(jump) * programSize); // allocate the worst case scenario
    resolveJumps(&jt);
}

void free_explainer() {
    free(program);
    free(jt.jumps);
}

uint32_t get_program_size() {
    return programSize;
}

instruction get_instruction(uint32_t address) {
    return program[address/8];
}

void resolveJumps() {
    // loop through everything and find the jumps
    jt.length = 0;
    for(uint32_t i = 0; i < programSize; i++) {
        instruction x = program[i];
        if(!(x.op == 0x0E || x.op == 0x17)) continue;
        jump* j = &(jt.jumps[jt.length]);
        j->from = i*8;
        j->to = x.imm;

        // before assigning a label, let's find if this target was used before
        char label = 0;
        for(uint32_t n = 0; n < jt.length; n++) {
            if(jt.jumps[n].to == x.imm) {
                label = jt.jumps[n].label;
                break;
            }
        }
        if(label == 0) label = 'A' + (jt.length);
        j->label = label;
        
        jt.length++;
    }
}

// only a few of these we know for sure, so don't rely on these to drive your understanding
char* describe_register(uint32_t offset) {
    if(offset < 0x400) {
        switch(offset) {
        case 0x00: return "FMC_CTRL0";
        case 0x04: return "FMC_CTRL1";
        case 0x08: return "FMC_CMD";
        case 0x0C: return "FMC_ADDR0";
        case 0x10: return "FMC_ADDR1";
        // case 0x14: return "FMC_ADDR2";
        // case 0x18: return "FMC_ADDR3";
        // case 0x1C: return "FMC_ADDR4";
        // case 0x20: return "FMC_ADDR5";
        // case 0x24: return "FMC_ADDR6";
        // case 0x28: return "FMC_ADDR7";
        case 0x2C: return "FMC_ANUM";
        case 0x30: return "FMC_DNUM";
        case 0x34: return "FMC_DATAW0";
        case 0x38: return "FMC_DATAW1";
        case 0x3C: return "FMC_DATAw2";
        case 0x40: return "FMC_DATAW3";
        case 0x48: return "FMC_STAT";
        case 0x80: return "FMC_FIFO";
        default: return "FMC_UNKNOWN";
        }
    } else if(offset >= 0x800 && offset < 0xC00) {
        // likely ECC control registers
        return "ECC_UNKNOWN";
    } else if(offset >= 0xC00 && offset < 0xD00) {
        // used to communicate state and other various control things to/from the FMISS
        return "FMICTRL_UNKNOWN";
    } else if(offset >= 0xD00) {
        // used to get data in/out of the FMISS like addresses and data to/from the flash
        return "FMIDATA_UNKNOWN";
    }
    return "UNKNOWN";
}

// List of operations confirmed in testing won't be marked with a '?'
const char knownInstructions[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x11, 0x13, 0x14};
uint32_t known = 0;

void explain_instruction(uint32_t address) {
    instruction cmd = program[address/8];
    char isTarget = 0;
    char isJump = 0;
    // is this address involved in a jump?
    for(int n = 0; n < jt.length; n++) {
        if(jt.jumps[n].to == address) {
            isTarget = jt.jumps[n].label;
        }
        if(jt.jumps[n].from == address) {
            isJump = jt.jumps[n].label;
        }
    }

    char isLabel = isTarget | isJump;

    char jumpstring[4];
    jumpstring[0] = isTarget ? (isJump ? '*' : '>') : (isJump ? '<' : '|');
    jumpstring[1] = isLabel ? isLabel : ' ';
    jumpstring[2] = isLabel ? ':' : ' ';
    jumpstring[3] = 0;

    char isKnown = memchr(knownInstructions, cmd.op, sizeof(knownInstructions)) ? ' ' : '?';
    if(isKnown == ' ') known++;

    printf("%.4X%c%s ", address, isKnown, jumpstring);

    printf("%.2X %.2X %.4X %.8X\t", cmd.op, cmd.arg1, cmd.arg2, cmd.imm);
    
    switch(cmd.op) {
    case 0x00:
        printf("RETURN");
        break;
    case 0x01:
        printf("write %s *(0x%.2X) = %.8X", describe_register(cmd.arg2), cmd.arg2, cmd.imm);
        break;
    case 0x02:
        printf("write %s *(0x%.2X) = r%d", describe_register(cmd.arg2), cmd.arg2, cmd.arg1);
        break;
    case 0x03:
        printf("r%d = *(r%d)", cmd.arg1, cmd.arg2);
        break;
    case 0x04:
        if(cmd.imm == 0xFFFFFFFF)
            printf("read r%d = *(%.4X) %s",cmd.arg1, cmd.arg2, describe_register(cmd.arg2));
        else
            printf("read r%d = *(%.4X) & 0x%.8X %s",cmd.arg1, cmd.arg2, cmd.imm, describe_register(cmd.arg2));
        break;
    case 0x05:
        printf("read r%d = 0x%.8X",cmd.arg1, cmd.imm);
        break;
    case 0x06:
        printf("r%d = r%d", cmd.arg1, cmd.arg2);
        break;
    case 0x07:
        printf("wait for flash ready on FMCSTAT[%d]", cmd.arg1);
        break;
    case 0x0A:
        printf("and r%d = r%d & r%d & 0x%.8X", cmd.arg1, cmd.arg1, cmd.arg2, cmd.imm);
        break;
    case 0x0B:
        printf("or r%d = r%d | r%d | 0x%.8X", cmd.arg1, cmd.arg1, cmd.arg2, cmd.imm);
        break;
    case 0x0C:
        printf("add r%d = r%d + %d", cmd.arg2, cmd.arg1, cmd.imm);
        break;
    case 0x0D:
        printf("subtract r%d = r%d - %d", cmd.arg2, cmd.arg1, cmd.imm);
        break;
    case 0x0E:
        printf("if r%d != 0, jump to 0x%.8X", cmd.arg1, cmd.imm);
        break;
    case 0x11:
        printf("store *r%d = r%d", cmd.arg2, cmd.arg1);
        break;
    case 0x13:
        printf("lshift r%d = r%d << %d", cmd.arg1, cmd.arg2, cmd.imm);
        break;
    case 0x14:
        printf("rshift r%d = r%d >> %d", cmd.arg1, cmd.arg2, cmd.imm);
        break;
    case 0x17:
        printf("some kind of jump to 0x%.8X based on r%d", cmd.imm, cmd.arg1);
        break;
    default:
        printf("unknown %.2X", cmd.op);
    }

    printf("\n");
}
