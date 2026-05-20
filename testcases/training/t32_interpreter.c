/* t32_interpreter.c — Simple bytecode interpreter.
 * Pattern: large switch in a loop, computed dispatch, stack operations.
 */

#define STACK_SIZE 256

enum Opcode {
    OP_PUSH, OP_POP, OP_ADD, OP_SUB, OP_MUL, OP_DIV,
    OP_DUP, OP_SWAP, OP_NEG, OP_ABS,
    OP_CMP_LT, OP_CMP_EQ, OP_CMP_GT,
    OP_JMP, OP_JZ, OP_JNZ,
    OP_LOAD, OP_STORE,
    OP_NOP, OP_HALT
};

struct VM {
    int stack[STACK_SIZE];
    int sp;
    int mem[256];
    int ip;
    int halted;
};

void vm_init(struct VM *vm) {
    vm->sp = 0;
    vm->ip = 0;
    vm->halted = 0;
    for (int i = 0; i < 256; i++) vm->mem[i] = 0;
}

int vm_run(struct VM *vm, const int *code, int code_len) {
    int steps = 0;
    while (!vm->halted && vm->ip < code_len && steps < 10000) {
        int op = code[vm->ip++];
        steps++;
        switch (op) {
        case OP_PUSH:
            if (vm->ip < code_len && vm->sp < STACK_SIZE)
                vm->stack[vm->sp++] = code[vm->ip++];
            break;
        case OP_POP:
            if (vm->sp > 0) vm->sp--;
            break;
        case OP_ADD:
            if (vm->sp >= 2) { vm->sp--; vm->stack[vm->sp-1] += vm->stack[vm->sp]; }
            break;
        case OP_SUB:
            if (vm->sp >= 2) { vm->sp--; vm->stack[vm->sp-1] -= vm->stack[vm->sp]; }
            break;
        case OP_MUL:
            if (vm->sp >= 2) { vm->sp--; vm->stack[vm->sp-1] *= vm->stack[vm->sp]; }
            break;
        case OP_DIV:
            if (vm->sp >= 2 && vm->stack[vm->sp-1] != 0) {
                vm->sp--; vm->stack[vm->sp-1] /= vm->stack[vm->sp];
            }
            break;
        case OP_DUP:
            if (vm->sp > 0 && vm->sp < STACK_SIZE)
                vm->stack[vm->sp] = vm->stack[vm->sp-1], vm->sp++;
            break;
        case OP_SWAP:
            if (vm->sp >= 2) {
                int t = vm->stack[vm->sp-1];
                vm->stack[vm->sp-1] = vm->stack[vm->sp-2];
                vm->stack[vm->sp-2] = t;
            }
            break;
        case OP_NEG:
            if (vm->sp > 0) vm->stack[vm->sp-1] = -vm->stack[vm->sp-1];
            break;
        case OP_ABS:
            if (vm->sp > 0 && vm->stack[vm->sp-1] < 0)
                vm->stack[vm->sp-1] = -vm->stack[vm->sp-1];
            break;
        case OP_CMP_LT:
            if (vm->sp >= 2) {
                vm->sp--; vm->stack[vm->sp-1] = vm->stack[vm->sp-1] < vm->stack[vm->sp];
            }
            break;
        case OP_CMP_EQ:
            if (vm->sp >= 2) {
                vm->sp--; vm->stack[vm->sp-1] = vm->stack[vm->sp-1] == vm->stack[vm->sp];
            }
            break;
        case OP_CMP_GT:
            if (vm->sp >= 2) {
                vm->sp--; vm->stack[vm->sp-1] = vm->stack[vm->sp-1] > vm->stack[vm->sp];
            }
            break;
        case OP_JMP:
            if (vm->ip < code_len) vm->ip = code[vm->ip];
            break;
        case OP_JZ:
            if (vm->ip < code_len && vm->sp > 0) {
                if (vm->stack[--vm->sp] == 0) vm->ip = code[vm->ip];
                else vm->ip++;
            }
            break;
        case OP_JNZ:
            if (vm->ip < code_len && vm->sp > 0) {
                if (vm->stack[--vm->sp] != 0) vm->ip = code[vm->ip];
                else vm->ip++;
            }
            break;
        case OP_LOAD:
            if (vm->sp > 0) {
                int addr = vm->stack[vm->sp-1] & 0xFF;
                vm->stack[vm->sp-1] = vm->mem[addr];
            }
            break;
        case OP_STORE:
            if (vm->sp >= 2) {
                int addr = vm->stack[vm->sp-2] & 0xFF;
                vm->mem[addr] = vm->stack[vm->sp-1];
                vm->sp -= 2;
            }
            break;
        case OP_HALT:
            vm->halted = 1;
            break;
        default: /* NOP */ break;
        }
    }
    return steps;
}

int vm_stack_top(const struct VM *vm) {
    return vm->sp > 0 ? vm->stack[vm->sp - 1] : 0;
}
