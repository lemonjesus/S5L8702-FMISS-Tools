#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "s5l8702_explainer.h"

extern uint32_t known;

int main(int argc, char** argv) {
    if(argc < 2) {
        printf("S5L8702 FMISS objdump by Tucker Osman\n");
        printf("Usage: %s <file>\n\n", argv[0]);
        return 1;
    }

    initalize_explainer(argv[1]);

    uint32_t programSize = get_program_size();

    for(int i = 0; i < programSize; i++) {
        explain_instruction_addr(i*8);
    }

    free_explainer();

    printf("\n%d instructions confirmed of %d total. (%.2f%%)\n", known, programSize, 100*((double)known)/programSize);

    return 0;
}