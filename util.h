#ifndef UTIL_H
#define UTIL_H

#define ZERO 0
#define ZERO_STRING "0"
#define ONE 1
#define TWO 2
#define THREE 3
#define FOUR 4
#define FIVE 5
#define SIX 6
#define SEVEN 7
#define EIGHT 8
#define NINE 9
#define TEN 10

#define NULL_CHAR '\0'
#define NEW_LINE_STRING "\n"
#define NEWLINE_CHAR '\n'
#define SPACE_CHAR ' '
#define EMPTY_STRING ""

#define SQUARE_BRACKET_START_CHAR '['
#define SQUARE_BRACKET_END_CHAR ']'

#define SPLITTING_DELIM " ,\t\r\n"

#define REGISTER_LEN 2
#define R_CHAR 'r'

#define MINUS_CHAR '-'
#define PLUS_CHAR '+'
#define IMMEDIATE_CHAR '#'
#define DOT_CHAR '.'
#define COMMA_STRING ","
#define SEMI_COLON_CHAR ':'
#define SEMI_COLON_STRING ":"

#define MAX_LINE_LENGTH 82
#define MAX_FILENAME 100

#define MAX_LABEL_LEN 30
#define LABEL_NULL_CHAR_LOCATION (MAX_LABEL_LEN - 1)

#define MAX_OPERAND_LEN (MAX_LINE_LENGTH - MAX_LABEL_LEN)
#define OPERAND_NULL_CHAR_LOCATION (MAX_OPERAND_LEN - 1)

#define NUMBER_OF_COMMANDS 16
#define NUMBER_OF_DATA_TYPES 3

#define MACRO_KEYWORD "mcro"
#define MACRO_END_KEYWORD "mcroend"
#define MACRO_LEN 4
#define MACRO_END_LEN 7
#define DEFAULT_MACRO_CAPACITY 10
#define INITIAL_LINE_CAPACITY 4
#define GROWTH_FACTOR 2

#define TRUE 1
#define FALSE 0
#define NOT_FOUND -1

#define ENTRY ".entry"
#define EXTERN ".extern"

// label type
typedef enum {
    UNKNOWN = 0,
    DATA = 1,
    CODE = 2,
    EXT = 3
}LabelTypes;

// register numbers
typedef enum {
    R0 = 48,
    R1 = 49,
    R2 = 50,
    R3 = 51,
    R4 = 52,
    R5 = 53,
    R6 = 54,
    R7 = 55
}RegisterNumbers;

// command type enum
typedef enum {
    MOV = 0,
    CMP = 1,
    ADD = 2,
    SUB = 3,
    NOT = 4,
    CLR = 5,
    LEA = 6,
    INC = 7,
    DEC = 8,
    JMP = 9,
    BNE = 10,
    RED = 11,
    PRN = 12,
    JSR = 13,
    RTS = 14,
    STP = 15
} CommandType;

// data type enum
typedef enum {
    STR = 16,
    DAT = 17,
    MAT = 18
}DataType;

// number of op for command
static const int command_operands[] = {
    [MOV] = 2,
    [CMP] = 2,
    [ADD] = 2,
    [SUB] = 2,
    [NOT] = 1,
    [CLR] = 1,
    [LEA] = 2,
    [INC] = 1,
    [DEC] = 1,
    [JMP] = 1,
    [BNE] = 1,
    [RED] = 1,
    [PRN] = 1,
    [JSR] = 1,
    [RTS] = 0,
    [STP] = 0,
    [STR] = 0,
    [DAT] = 0,
    [MAT] = 0
};

static const char* command_names[] = {
    [MOV] = "mov",
    [CMP] = "cmp",
    [ADD] = "add",
    [SUB] = "sub",
    [NOT] = "not",
    [CLR] = "clr",
    [LEA] = "lea",
    [INC] = "inc",
    [DEC] = "dec",
    [JMP] = "jmp",
    [BNE] = "bne",
    [RED] = "red",
    [PRN] = "prn",
    [JSR] = "jsr",
    [RTS] = "rts",
    [STP] = "stop",
    [STR] = ".string",
    [DAT] = ".data",
    [MAT] = ".mat"
};

// addressing mode enum
typedef enum {
    IMMEDIATE_ADDRESSING = 0,
    DIRECT_ADDRESSING = 1,
    MATRIX_ACCESS_ADDRESSING = 2,
    DIRECT_REGISTER_ADDRESSING = 3
} AddressingMode;

// addressing characteristic enum
typedef enum {
    ABSOLUTE = 0,
    EXTERNAL = 1,
    RELOCATABLE = 2,
} AddressingCharacteristic;

int find_command(char *word, char *label);
int is_number(const char *s, double *out);
int is_register(const char *op);
int is_immediate(const char *op);
int is_matrix(const char *op);
void print_error(const char *filename, int line_number, const char *msg);

/* Forward-declare Labels so we can reference it without including labels.h (to avoid cycles). */
struct Labels;

#endif //UTIL_H
