#ifndef S5L8702_EXPLAINER_H
#define S5L8702_EXPLAINER_H

typedef union _instruction {
    struct {
        uint32_t imm;
        uint16_t arg2;
        uint8_t arg1;
        uint8_t op;
    };
    struct {
        uint32_t top;
        uint32_t bottom;
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

// loads a binary file with S5L8702 FMISS instructions into the explainer
void initalize_explainer(char* hexFile);

// get the number of instructions in the explainer
uint32_t get_program_size();

// get an instruction from the explainer at a given address
instruction get_instruction(uint32_t address);

// deinitializes the explainer
void free_explainer();

// returns a string of what a memory register could mean
char* describe_register(uint32_t offset);

// prints a full objdump like explination of an instruction at an address
void explain_instruction(uint32_t address);

#endif