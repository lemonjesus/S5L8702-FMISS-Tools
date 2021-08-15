#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "s5l8702_explainer.h"

uint32_t* vmem;
uint32_t* regs;
uint32_t pc = 0;

void execute_instruction(instruction x) {
    explain_instruction(x);

    switch (x.op) {
    case 0x00: // return
        printf("execution termination\n");
        pc -= 8; // so we don't proceed past the return instruction
        break;
    case 0x01: // write immediate to memory
        vmem[x.arg2/4] = x.imm;
        vmem[x.arg2/4-1] = x.imm >> 8;
        vmem[x.arg2/4-2] = x.imm >> 16;
        vmem[x.arg2/4-3] = x.imm >> 24;
        break;
    case 0x02: // write register to memory
        vmem[x.arg2/4] = regs[x.arg1];
        break;
    case 0x03: // dereference pointer in register
        regs[x.arg1] = *((uint32_t*) regs[x.arg1]);
        break;
    case 0x04: // read from memory to register
        regs[x.arg1] = vmem[x.arg2/4] & x.imm;
        break;
    case 0x05: // read immediate into register
        regs[x.arg1] = x.imm;
        break;
    case 0x06: // read register into register
        regs[x.arg1] = regs[x.arg2];
        break;
    case 0x07: // wait for nand (ignore)
        printf("wait for flash ready on FMCSTAT[%d]? assuming this condition is true\n", x.arg1);
        break;
    case 0x0A: // AND two registers and an immediate
        regs[x.arg1] = regs[x.arg1] & regs[x.arg2] & x.imm;
        break;
    case 0x0B: // OR two registers and an immediate
        regs[x.arg1] = regs[x.arg1] | regs[x.arg2] | x.imm;
        break;
    case 0x0C: // Add a register and an immediate
        regs[x.arg1] = regs[x.arg2] + x.imm;
        break;
    case 0x0D: // Subtract a register and an immediate
        regs[x.arg1] = regs[x.arg2] - x.imm;
        break;
    case 0x0E: // Jump if not equal
        if(regs[x.arg1] != x.arg2) pc = regs[x.imm];
        break;
    case 0x11: // Store a register value to a memory location pointed to by a register
        *((uint32_t*) regs[x.arg2]) = regs[x.arg1];
        break;
    case 0x13: // Left shift register by an immediate
        regs[x.arg1] = regs[x.arg2] << x.imm;
        break;
    case 0x14: // Right shift register by an immediate
        regs[x.arg1] = regs[x.arg2] >> x.imm;
        break;
    case 0x17: // unknown jump
        printf("unknown jump\n");
        break;
    default:
        printf("unknown instruction\n");
        break;
    }
}

int main(int argc, char** argv) {
    if(argc < 2) {
        printf("S5L8702 FMISS Simulator by Tucker Osman\n");
        printf("Usage: %s <file>\n\n", argv[0]);
        return 1;
    }

    initalize_explainer(argv[1]);

    // set up virtual memory, we'll treat this as the base DMA address
    vmem = (uint8_t*) malloc(0x10000/4);
    regs = (uint32_t*) malloc(8*sizeof(uint32_t));

    rl_bind_key('\t', rl_insert);

    printf("ready to execute\n");

    while(1) {
        char* line = readline("s5l8702> ");

        if(!line) {
            continue;
        }

        if (strlen(line) > 0) {
            add_history(line);
        }

        char* command = strtok(line, " ");

        if(strcmp(command, "help") == 0) {
            printf("\nS5L8702 FMISS Simulator by Tucker Osman\n");
            printf("help - display help\n");
            printf("(n)ext - execute next instruction\n");
            printf("(w)here - show the next instruction to be executed\n");
            printf("set [reg] [value] - set register value\n");
            printf("get [reg] - get register value\n");
            printf("setm [addr] [value] - set memory value at addr\n");
            printf("getm [addr] - get memory value at addr\n");
            printf("alloc [addr] [size] - allocate memory and put its address in addr\n");
            printf("show [addr] [size] - show memory pointed to by addr (potentially dangerous, can cause seg fault)\n");
            printf("exec [64-bit hex] - execute 64-bit instruction presented in readable format\n");
            printf("quit - quit\n");
        } else if(strcmp(command, "next") == 0 || strcmp(command, "n") == 0) {
            execute_instruction(get_instruction(pc));
            pc += 8;
        } else if(strcmp(command, "where") == 0 || strcmp(command, "w") == 0) {
            explain_instruction(pc);
        } else if(strcmp(command, "set") == 0) {
            char* reg = strtok(NULL, " ");
            char* value = strtok(NULL, " ");
            if(!reg || !value) {
                printf("set <reg> <value>\n");
                continue;
            }
            int regIndex = strtol(reg+1, NULL, 16);
            int valueIndex = strtol(value, NULL, 16);

            if(regIndex < 0 || regIndex > 7) {
                printf("register must be r0 - r7\n");
                continue;
            }
            if(valueIndex < 0 || valueIndex > 0xffffffff) {
                printf("value must be 0 - 0xffffffff\n");
                continue;
            }
            regs[regIndex] = valueIndex;
        } else if(strcmp(command, "get") == 0) {
            char* reg = strtok(NULL, " ");
            if(!reg) {
                printf("get <reg>\n");
                continue;
            }
            int regIndex = strtol(reg+1, NULL, 16);
            if(regIndex < 0 || regIndex > 7) {
                printf("register must be r0 - r7\n");
                continue;
            }
            printf("0x%08X\n", regs[regIndex]);
        } else if(strcmp(command, "setm") == 0) {
            char* addr = strtok(NULL, " ");
            char* value = strtok(NULL, " ");
            if(!addr || !value) {
                printf("setm <addr> <value>\n");
                continue;
            }
            int addrIndex = strtol(addr, NULL, 16);
            int valueIndex = strtol(value, NULL, 16);
            if(addrIndex < 0 || addrIndex > 0xffff) {
                printf("address must be 0 - 0xffff\n");
                continue;
            }
            vmem[addrIndex/4] = valueIndex;
        } else if(strcmp(command, "getm") == 0) {
            char* addr = strtok(NULL, " ");
            if(!addr) {
                printf("getm <addr>\n");
                continue;
            }

            int addrIndex = strtol(addr, NULL, 16);
            if(addrIndex < 0 || addrIndex > 0xffff) {
                printf("address must be 0 - 0xffff\n");
                continue;
            }
            printf("0x%08X\n", (uint32_t)vmem[addrIndex/4]);
        } else if(strcmp(command, "alloc") == 0) {
            char* addr = strtok(NULL, " ");
            char* size = strtok(NULL, " ");
            if(!addr || !size) {
                printf("alloc <addr> <size>\n");
                continue;
            }
            int addrIndex = strtol(addr, NULL, 16);
            int sizeIndex = strtol(size, NULL, 16);
            if(addrIndex < 0 || addrIndex > 0xffff) {
                printf("address must be 0 - 0xffff\n");
                continue;
            }
            if(sizeIndex < 0 || sizeIndex > 0x10000) {
                printf("size must be 0 - 0x10000\n");
                continue;
            }
            vmem[addrIndex/4] = malloc(sizeIndex);
        } else if(strcmp(command, "show") == 0) {
            char* addr = strtok(NULL, " ");
            char* size = strtok(NULL, " ");
            if(!addr || !size) {
                printf("show <addr> <size>\n");
                continue;
            }
            int addrIndex = strtol(addr, NULL, 16);
            int sizeIndex = strtol(size, NULL, 16);
            if(addrIndex < 0 || addrIndex > 0xffff) {
                printf("address must be 0 - 0xffff\n");
                continue;
            }
            if(sizeIndex < 0 || sizeIndex > 0x10000) {
                printf("size must be 0 - 0x10000\n");
                continue;
            }
            // prints a nice hex dump of the memory in 16 byte columns
            for(int i = 0; i < sizeIndex; i += 16) {
                printf("%08X: ", addrIndex+i);
                for(int j = 0; j < 16; j++) {
                    if(i+j < sizeIndex) {
                        printf("%02x ", vmem[addrIndex+i+j]);
                    } else {
                        printf("   ");
                    }
                }
                printf("\n");
            }
        } else if(strcmp(command, "exec") == 0) {
            char* hex = strtok(NULL, " ");
            if(!hex) {
                printf("exec <64-bit hex>\n");
                continue;
            }
            uint64_t x = strtoull(hex, NULL, 16);
            execute_instruction((instruction) x);
        } else if(strcmp(command, "quit") == 0) {
            free(line);
            break;
        } else {
            printf("unknown command\n");
        }
        free(line);
    }

    free(vmem);
    free(regs);
    free_explainer();   
}
