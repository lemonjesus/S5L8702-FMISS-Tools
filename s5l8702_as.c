/*
 * s5l8702_as.c - Assembler and disassembler for S5L8702 FMISS microcode
 *
 * Usage:
 *   s5l8702_as -a <input.fmiss> -o <output.bin>    assemble
 *   s5l8702_as -d <input.bin> [-o <output.fmiss>]  disassemble (stdout if no -o)
 *
 * Assembly syntax:
 *   ; comment
 *   label:
 *   ret
 *   mmio.st  [0xOFFSET], 0xVAL     ; write immediate to MMIO (0x01)
 *   mmio.st  [0xOFFSET], rN        ; write register to MMIO, immediate offset (0x02)
 *   mmio.st  [rN], rM              ; write register to MMIO, register offset (0x19)
 *   mmio.ld  rN, [0xOFFSET]        ; read from MMIO, no mask (0x04)
 *   mmio.ld  rN, [0xOFFSET], 0xMSK ; read from MMIO with mask (0x04)
 *   mmio.ld  rN, [rM]              ; read from MMIO, register offset (0x18)
 *   ld       rN, [rM]              ; dereference RAM pointer: rN = *rM (0x03)
 *   st       [rN], rM              ; store to RAM pointer: *rN = rM (0x11)
 *   mov      rN, rM                ; copy register (0x06)
 *   mov      rN, 0xVAL             ; load immediate (0x05)
 *   wait     N                     ; wait for FMCSTAT bit N (0x07)
 *   and      rD, rS                ; rD &= rS (0x0A, imm=0)
 *   and      rD, rS, 0xVAL         ; rD = rS & val (0x0A)
 *   or       rD, rS                ; rD |= rS (0x0B, imm=0)
 *   or       rD, rS, 0xVAL         ; rD = rS | val (0x0B)
 *   add      rD, rS                ; rD += rS (0x0C, imm=0)
 *   add      rD, rS, 0xVAL         ; rD = rS + val (0x0C)
 *   sub      rD, rS                ; rD -= rS (0x0D, imm=0)
 *   sub      rD, rS, 0xVAL         ; rD = rS - val (0x0D)
 *   lsl      rD, rS                ; rD <<= rS (0x13, imm=0)
 *   lsl      rD, rS, N             ; rD = rS << N (0x13)
 *   lsr      rD, rS                ; rD >>= rS (0x14, imm=0)
 *   lsr      rD, rS, N             ; rD = rS >> N (0x14)
 *   jnz      rN, label             ; jump if rN != 0 (0x0E)
 *   jz       rN, label             ; jump if rN == 0 (0x17)
 *
 * On-disk encoding (big-endian):
 *   byte 0: opcode
 *   byte 1: arg1 (destination register)
 *   byte 2-3: arg2 (source register or 16-bit MMIO offset)
 *   byte 4-7: 32-bit immediate
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef union {
    struct {
        uint32_t imm;
        uint16_t arg2;
        uint8_t  arg1;
        uint8_t  op;
    };
    struct {
        uint32_t top;
        uint32_t bottom;
    };
    uint64_t raw;
} instruction;

static void emit(FILE *f, uint8_t op, uint8_t arg1, uint16_t arg2, uint32_t imm) {
    uint8_t buf[8] = {
        arg2 & 0xFF,
        (arg2 >> 8) & 0xFF,
        arg1,
        op,
        imm & 0xFF,
        (imm >>  8) & 0xFF,
        (imm >> 16) & 0xFF,
        (imm >> 24) & 0xFF,
    };
    fwrite(buf, 1, 8, f);
}

static instruction *load_binary(const char *path, uint32_t *count_out) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return NULL; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint32_t count = (uint32_t)(sz / 8);
    instruction *prog = malloc(count * sizeof(instruction));
    fread(prog, 8, count, f);
    fclose(f);
    for (uint32_t i = 0; i < count; i++) {
        prog[i].top    = __builtin_bswap32(prog[i].top);
        prog[i].bottom = __builtin_bswap32(prog[i].bottom);
        prog[i].raw    = __builtin_bswap64(prog[i].raw);
    }
    *count_out = count;
    return prog;
}

static char *skip_ws(char *p) {
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

static int parse_reg(char **p) {
    char *s = *p;
    if ((*s != 'r' && *s != 'R') || !isdigit((unsigned char)s[1])) return -1;
    int n = s[1] - '0';
    if (n > 7) return -1;
    /* don't consume if followed by another digit */
    if (isdigit((unsigned char)s[2])) return -1;
    *p = s + 2;
    return n;
}

static int parse_imm(char **p, uint32_t *out) {
    char *s = *p, *end;
    unsigned long v = strtoul(s, &end, 0);
    if (end == s) return 0;
    *out = (uint32_t)v;
    *p = end;
    return 1;
}

static int parse_bracket(char **p, int *reg, uint32_t *imm) {
    char *s = *p;
    if (*s != '[') return 0;
    s = skip_ws(s + 1);
    int r = parse_reg(&s);
    if (r >= 0) {
        s = skip_ws(s);
        if (*s != ']') return 0;
        *reg = r;
        *p = s + 1;
        return 1;
    }
    uint32_t v;
    if (!parse_imm(&s, &v)) return 0;
    s = skip_ws(s);
    if (*s != ']') return 0;
    *imm = v;
    *p = s + 1;
    return 2;
}

#define MAX_LABELS 512

typedef struct { char name[64]; uint32_t addr; } label_entry;
static label_entry labels[MAX_LABELS];
static int label_count;

static void add_label(const char *name, uint32_t addr) {
    if (label_count >= MAX_LABELS) { fprintf(stderr, "Too many labels\n"); exit(1); }
    strncpy(labels[label_count].name, name, 63);
    labels[label_count].addr = addr;
    label_count++;
}

static int find_label(const char *name, uint32_t *addr) {
    for (int i = 0; i < label_count; i++) {
        if (strcmp(labels[i].name, name) == 0) {
            *addr = labels[i].addr;
            return 1;
        }
    }
    return 0;
}

static int assemble_line(char *line, uint32_t addr, FILE *out, int pass, int lineno) {
    char *p = skip_ws(line);
    if (*p == '\0' || *p == ';' || *p == '\r' || *p == '\n') return 0;

    char *semi = strchr(p, ';');
    if (semi) *semi = '\0';
    int len = (int)strlen(p);
    while (len > 0 && (p[len-1] == ' ' || p[len-1] == '\t' || p[len-1] == '\r' || p[len-1] == '\n'))
        p[--len] = '\0';
    if (len == 0) return 0;

    /* label? */
    char *colon = strchr(p, ':');
    if (colon) {
        /* extract label name (everything before colon, trimmed) */
        *colon = '\0';
        char lname[64]; int li = 0;
        for (char *c = p; *c && li < 63; c++) if (*c != ' ' && *c != '\t') lname[li++] = *c;
        lname[li] = '\0';
        if (pass == 1) add_label(lname, addr);
        /* anything after the colon on the same line? */
        p = skip_ws(colon + 1);
        if (*p == '\0') return 0;
    }

    /* mnemonic */
    char mnem[32] = {0}; int mi = 0;
    while (*p && !isspace((unsigned char)*p) && mi < 31) mnem[mi++] = *p++;
    p = skip_ws(p);

    uint8_t  op   = 0;
    uint8_t  arg1 = 0;
    uint16_t arg2 = 0;
    uint32_t imm  = 0;

#define EXPECT_COMMA() do { \
    p = skip_ws(p); \
    if (*p != ',') { fprintf(stderr, "line %d: expected ','\n", lineno); return -1; } \
    p = skip_ws(p + 1); \
} while(0)

#define NEED_REG(var) do { \
    (var) = parse_reg(&p); \
    if ((var) < 0) { fprintf(stderr, "line %d: expected register\n", lineno); return -1; } \
} while(0)

#define NEED_IMM(var) do { \
    if (!parse_imm(&p, &(var))) { fprintf(stderr, "line %d: expected immediate\n", lineno); return -1; } \
} while(0)

    /* Two-operand instructions with optional immediate third operand.
     * Macro for: and/or/add/sub/lsl/lsr
     *   rD, rS       → imm=0  (register form)
     *   rD, rS, val  → imm=val (immediate form)
     */
#define TWO_OR_THREE(OP) do { \
    int _d, _s; \
    NEED_REG(_d); EXPECT_COMMA(); NEED_REG(_s); \
    p = skip_ws(p); \
    if (*p == ',') { p = skip_ws(p + 1); NEED_IMM(imm); } \
    op = (OP); arg1 = _d; arg2 = _s; \
} while(0)

    if (strcmp(mnem, "ret") == 0) {
        op = 0x00;

    } else if (strcmp(mnem, "mmio.st") == 0) {
        int breg; uint32_t bimm;
        int btype = parse_bracket(&p, &breg, &bimm);
        if (!btype) { fprintf(stderr, "line %d: expected [offset]\n", lineno); return -1; }
        EXPECT_COMMA();
        int src = parse_reg(&p);
        if (src >= 0) {
            /* register source */
            if (btype == 2) { op = 0x02; arg1 = src; arg2 = (uint16_t)bimm; }
            else             { op = 0x19; arg1 = src; arg2 = (uint16_t)breg; }
        } else {
            /* immediate source: only immediate offset makes sense */
            if (btype != 2) { fprintf(stderr, "line %d: mmio.st with register offset requires register source\n", lineno); return -1; }
            NEED_IMM(imm);
            op = 0x01; arg2 = (uint16_t)bimm;
        }

    } else if (strcmp(mnem, "mmio.ld") == 0) {
        int dst; NEED_REG(dst);
        EXPECT_COMMA();
        int breg; uint32_t bimm;
        int btype = parse_bracket(&p, &breg, &bimm);
        if (!btype) { fprintf(stderr, "line %d: expected [offset]\n", lineno); return -1; }
        arg1 = dst;
        if (btype == 2) {
            op = 0x04; arg2 = (uint16_t)bimm; imm = 0xFFFFFFFF;
            p = skip_ws(p);
            if (*p == ',') { p = skip_ws(p + 1); NEED_IMM(imm); }
        } else {
            op = 0x18; arg2 = (uint16_t)breg;
        }

    } else if (strcmp(mnem, "ld") == 0) {
        /* ld rN, [rM]  →  0x03: rN = *rM  (RAM dereference) */
        int dst; NEED_REG(dst);
        EXPECT_COMMA();
        int breg; uint32_t bimm;
        if (parse_bracket(&p, &breg, &bimm) != 1) {
            fprintf(stderr, "line %d: ld requires [rN] operand\n", lineno); return -1;
        }
        op = 0x03; arg1 = dst; arg2 = (uint16_t)breg;

    } else if (strcmp(mnem, "st") == 0) {
        /* st [rN], rM  →  0x11: *rN = rM  (RAM store) */
        int breg; uint32_t bimm;
        if (parse_bracket(&p, &breg, &bimm) != 1) {
            fprintf(stderr, "line %d: st requires [rN] operand\n", lineno); return -1;
        }
        EXPECT_COMMA();
        int src; NEED_REG(src);
        op = 0x11; arg1 = src; arg2 = (uint16_t)breg;

    } else if (strcmp(mnem, "mov") == 0) {
        int dst; NEED_REG(dst);
        EXPECT_COMMA();
        int src = parse_reg(&p);
        if (src >= 0) { op = 0x06; arg1 = dst; arg2 = (uint16_t)src; }
        else { NEED_IMM(imm); op = 0x05; arg1 = dst; }

    } else if (strcmp(mnem, "wait") == 0) {
        uint32_t bit; NEED_IMM(bit);
        op = 0x07; arg1 = (uint8_t)bit;

    } else if (strcmp(mnem, "and") == 0) { TWO_OR_THREE(0x0A);
    } else if (strcmp(mnem, "or")  == 0) { TWO_OR_THREE(0x0B);
    } else if (strcmp(mnem, "add") == 0) { TWO_OR_THREE(0x0C);
    } else if (strcmp(mnem, "sub") == 0) { TWO_OR_THREE(0x0D);
    } else if (strcmp(mnem, "lsl") == 0) { TWO_OR_THREE(0x13);
    } else if (strcmp(mnem, "lsr") == 0) { TWO_OR_THREE(0x14);

    } else if (strcmp(mnem, "jnz") == 0 || strcmp(mnem, "jz") == 0) {
        int reg; NEED_REG(reg);
        EXPECT_COMMA();
        /* label name or hex address */
        char lname[64] = {0}; int li = 0;
        while (*p && !isspace((unsigned char)*p) && *p != ';' && li < 63) lname[li++] = *p++;
        op   = (strcmp(mnem, "jnz") == 0) ? 0x0E : 0x17;
        arg1 = reg;
        if (pass == 2) {
            if (!find_label(lname, &imm)) {
                char *lp = lname;
                if (!parse_imm(&lp, &imm)) {
                    fprintf(stderr, "line %d: unknown label '%s'\n", lineno, lname); return -1;
                }
            }
        }

    } else {
        fprintf(stderr, "line %d: unknown mnemonic '%s'\n", lineno, mnem);
        return -1;
    }

    if (pass == 2) emit(out, op, arg1, arg2, imm);
    return 1;

#undef EXPECT_COMMA
#undef NEED_REG
#undef NEED_IMM
#undef TWO_OR_THREE
}

static int assemble(const char *inpath, const char *outpath) {
    FILE *f = fopen(inpath, "r");
    if (!f) { perror(inpath); return 1; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char *src = malloc(sz + 1);
    fread(src, 1, sz, f); src[sz] = '\0'; fclose(f);

    /* Pass 1: collect labels */
    label_count = 0;
    uint32_t addr = 0; int lineno = 1;
    char *line = src;
    while (*line) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        char *copy = strdup(line);
        int r = assemble_line(copy, addr, NULL, 1, lineno);
        free(copy);
        if (r < 0) { free(src); return 1; }
        if (r > 0) addr += 8;
        if (nl) { *nl = '\n'; line = nl + 1; } else break;
        lineno++;
    }

    /* Pass 2: emit */
    FILE *out = fopen(outpath, "wb");
    if (!out) { perror(outpath); free(src); return 1; }
    addr = 0; lineno = 1; line = src;
    while (*line) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        char *copy = strdup(line);
        int r = assemble_line(copy, addr, out, 2, lineno);
        free(copy);
        if (r < 0) { fclose(out); free(src); return 1; }
        if (r > 0) addr += 8;
        if (nl) { *nl = '\n'; line = nl + 1; } else break;
        lineno++;
    }
    fclose(out); free(src);
    return 0;
}

static void disassemble(const char *inpath, FILE *out) {
    uint32_t count;
    instruction *prog = load_binary(inpath, &count);
    if (!prog) return;

    /* collect jump targets so we can emit labels */
    uint32_t targets[256]; int ntargets = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (prog[i].op != 0x0E && prog[i].op != 0x17) continue;
        uint32_t t = prog[i].imm;
        int dup = 0;
        for (int j = 0; j < ntargets; j++) if (targets[j] == t) { dup = 1; break; }
        if (!dup && ntargets < 256) targets[ntargets++] = t;
    }

    for (uint32_t i = 0; i < count; i++) {
        uint32_t addr = i * 8;
        instruction x = prog[i];

        /* label before this instruction? */
        for (int j = 0; j < ntargets; j++) {
            if (targets[j] == addr) { fprintf(out, "L%04X:\n", addr); break; }
        }

        /* jump target label string (built ahead of switch for jnz/jz) */
        char lbl[16] = {0};
        if (x.op == 0x0E || x.op == 0x17) snprintf(lbl, sizeof(lbl), "L%04X", x.imm);

        switch (x.op) {
        case 0x00: fprintf(out, "    ret\n"); break;
        case 0x01: fprintf(out, "    mmio.st [0x%04X], 0x%08X\n", x.arg2, x.imm); break;
        case 0x02: fprintf(out, "    mmio.st [0x%04X], r%d\n", x.arg2, x.arg1); break;
        case 0x03: fprintf(out, "    ld r%d, [r%d]\n", x.arg1, x.arg2); break;
        case 0x04:
            if (x.imm == 0xFFFFFFFF)
                fprintf(out, "    mmio.ld r%d, [0x%04X]\n", x.arg1, x.arg2);
            else
                fprintf(out, "    mmio.ld r%d, [0x%04X], 0x%08X\n", x.arg1, x.arg2, x.imm);
            break;
        case 0x05: fprintf(out, "    mov r%d, 0x%08X\n", x.arg1, x.imm); break;
        case 0x06: fprintf(out, "    mov r%d, r%d\n", x.arg1, x.arg2); break;
        case 0x07: fprintf(out, "    wait %d\n", x.arg1); break;
        case 0x0A:
            if (x.imm == 0) fprintf(out, "    and r%d, r%d\n", x.arg1, x.arg2);
            else            fprintf(out, "    and r%d, r%d, 0x%08X\n", x.arg1, x.arg2, x.imm);
            break;
        case 0x0B:
            if (x.imm == 0) fprintf(out, "    or r%d, r%d\n", x.arg1, x.arg2);
            else            fprintf(out, "    or r%d, r%d, 0x%08X\n", x.arg1, x.arg2, x.imm);
            break;
        case 0x0C:
            if (x.imm == 0) fprintf(out, "    add r%d, r%d\n", x.arg1, x.arg2);
            else            fprintf(out, "    add r%d, r%d, 0x%08X\n", x.arg1, x.arg2, x.imm);
            break;
        case 0x0D:
            if (x.imm == 0) fprintf(out, "    sub r%d, r%d\n", x.arg1, x.arg2);
            else            fprintf(out, "    sub r%d, r%d, 0x%08X\n", x.arg1, x.arg2, x.imm);
            break;
        case 0x0E: fprintf(out, "    jnz r%d, %s\n", x.arg1, lbl); break;
        case 0x11: fprintf(out, "    st [r%d], r%d\n", x.arg2, x.arg1); break;
        case 0x13:
            if (x.imm == 0) fprintf(out, "    lsl r%d, r%d\n", x.arg1, x.arg2);
            else            fprintf(out, "    lsl r%d, r%d, %u\n", x.arg1, x.arg2, x.imm);
            break;
        case 0x14:
            if (x.imm == 0) fprintf(out, "    lsr r%d, r%d\n", x.arg1, x.arg2);
            else            fprintf(out, "    lsr r%d, r%d, %u\n", x.arg1, x.arg2, x.imm);
            break;
        case 0x17: fprintf(out, "    jz r%d, %s\n", x.arg1, lbl); break;
        case 0x18: fprintf(out, "    mmio.ld r%d, [r%d]\n", x.arg1, x.arg2); break;
        case 0x19: fprintf(out, "    mmio.st [r%d], r%d\n", x.arg2, x.arg1); break;
        default:
            fprintf(out, "    ; unknown opcode 0x%02X 0x%02X 0x%04X 0x%08X\n",
                    x.op, x.arg1, x.arg2, x.imm);
            break;
        }
    }
    free(prog);
}

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage:\n"
        "  %s -a <input.fmiss> -o <output.bin>    assemble\n"
        "  %s -d <input.bin> [-o <output.fmiss>]  disassemble\n",
        prog, prog);
}

int main(int argc, char **argv) {
    if (argc < 3) { usage(argv[0]); return 1; }

    const char *mode    = argv[1];
    const char *infile  = NULL;
    const char *outfile = NULL;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) outfile = argv[++i];
        else infile = argv[i];
    }

    if (!infile) { usage(argv[0]); return 1; }

    if (strcmp(mode, "-a") == 0) {
        if (!outfile) { fprintf(stderr, "Output file required for assembly (-o)\n"); return 1; }
        return assemble(infile, outfile);
    } else if (strcmp(mode, "-d") == 0) {
        FILE *out = outfile ? fopen(outfile, "w") : stdout;
        if (!out) { perror(outfile); return 1; }
        disassemble(infile, out);
        if (outfile) fclose(out);
        return 0;
    } else {
        usage(argv[0]); return 1;
    }
}
