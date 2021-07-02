#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

uint8_t* hexData;
long hexSize;

typedef union _instruction {
    struct {
        uint8_t op;
        uint8_t arg1;
        uint16_t arg2;
        uint32_t imm;
    };
    uint64_t raw;
} instruction;

typedef struct _jump {
    uint32_t from;
    uint32_t to;
    char label;
} jump;

typedef struct _jmptable {
    uint32_t length;
    jump* jumps;
} jmptable;

instruction readCmd(uint32_t idx) {
    instruction inst;
    uint64_t x = *((uint64_t*) hexData + idx);
    inst.imm = (x >> 32);
    inst.op = (x >> 24);
    inst.arg1 = (x >> 16);
    inst.arg2 = (x >> 0);
    return inst;
}

void printCmd(instruction cmd) {
    printf("0x%.8X %.8X\n", cmd.op, cmd.imm);
}

// only a few of these we know for sure, so don't rely on these to drive your understanding
uint8_t* describeRegister(uint32_t offset) {
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

int known = 0;

void explainInstruction(instruction cmd, uint32_t offset, jmptable* jt) {
    char isTarget = 0;
    char isJump = 0;
    // is this address involved in a jump?
    for(int n = 0; n < jt->length; n++) {
        if(jt->jumps[n].to == offset) {
            isTarget = jt->jumps[n].label;
        }
        if(jt->jumps[n].from == offset) {
            isJump = jt->jumps[n].label;
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

    printf("%.4X%c%s ", offset, isKnown, jumpstring);

    printf("%.2X %.2X %.4X %.8X\t", cmd.op, cmd.arg1, cmd.arg2, cmd.imm);
    
    switch(cmd.op) {
    case 0x00:
        printf("RETURN");
        break;
    case 0x01:
        printf("write %s *(0x%.2X) = %.8X", describeRegister(cmd.arg2), cmd.arg2, cmd.imm);
        break;
    case 0x02:
        printf("write %s *(0x%.2X) = r%d", describeRegister(cmd.arg2), cmd.arg2, cmd.arg1);
        break;
    case 0x03:
        printf("r%d = *(r%d)", cmd.arg1, cmd.arg2);
        break;
    case 0x04:
        if(cmd.imm == 0xFFFFFFFF)
            printf("read r%d = *(%.4X) %s",cmd.arg1, cmd.arg2, describeRegister(cmd.arg2));
        else
            printf("read r%d = *(%.4X) & 0x%.8X %s",cmd.arg1, cmd.arg2, cmd.imm, describeRegister(cmd.arg2));
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
        printf("if r%d != %d, jump to 0x%.8X", cmd.arg1, cmd.arg2, cmd.imm);
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

void resolveJumps(jmptable* jt) {
    // loop through everything and find the jumps
    jt->length = 0;
    for(uint32_t i = 0; i < hexSize/8; i++) {
        instruction x = readCmd(i);
        if(!(x.op == 0x0E || x.op == 0x17)) continue;
        jump* j = &(jt->jumps[jt->length]);
        j->from = i*8;
        j->to = x.imm;

        // before assigning a label, let's find if this target was used before
        char label = 0;
        for(uint32_t n = 0; n < jt->length; n++) {
            if(jt->jumps[n].to == x.imm) {
                label = jt->jumps[n].label;
                break;
            }
        }
        if(label == 0) label = 'A' + (jt->length);
        j->label = label;
        
        jt->length++;
    }
}

int main(int argc, char** argv) {
    if(argc < 2) {
        printf("S5L8702 FMISS objdump by Tucker Osman\n");
        printf("Usage: %s <file>\n\n", argv[0]);
        return 1;
    }

    FILE* fp = fopen(argv[1], "r");
    fseek(fp, 0, SEEK_END);
    hexSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    hexData = (uint8_t*) malloc(hexSize);
    fread(hexData, hexSize, 1, fp);
    fclose(fp);

    jmptable jt;
    jt.jumps = malloc(sizeof(jump) * hexSize/8); // allocate the worst case scenario
    resolveJumps(&jt);

    for(int i = 0; i < hexSize/8; i++) {
        explainInstruction(readCmd(i), i*8, &jt);
    }

    free(jt.jumps);
    free(hexData);

    printf("\n%d instructions confirmed of %ld total. (%.2f%%)\n", known, hexSize/8, 100*((double)known)/(hexSize/8));

    return 0;
}