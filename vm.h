// stack based virtual machine
#pragma once

// the instructions of the virtual machine
// FORMAT: OP_<name>.<data_type> [argument] effect
//		OP_ - common prefix (operation code)
//		<name> - instruction name
//		<data_type> - if present, the data type on which the instruction acts
//			.i - int
//			.f - double
//			.c - char
//			.p - pointer
//		[argument] - if present, the instruction argument
//		effect - the effect of the instruction
typedef enum
{
	OP_HALT,			  // OP_HALT - ends the code execution
	OP_PUSH_INT,		  // OP_push.i [const] - puts on stack the constant [const]
	OP_CALL,			  // OP_call [instr] - calls a VM function which starts with the given instruction
	OP_CALL_EXTERNAL,	  // OP_call_ext [native_addr] - calls a host function (machine code) at the given address
	OP_ENTER,			  // OP_enter [num_locals] - creates a function frame with the given number of local variables
	OP_RETURN,			  // OP_return [num_params] - returns from a function which has the given number of parameters and returns a value
	OP_RETURN_VOID,		  // OP_return_void [num_params] - returns from a function which has the given number of parameters without returning a value
	OP_CONVERT_INT_FLOAT, // OP_convert_int_float - converts the value from stack from int to double
	OP_JMP,				  // OP_jmp [instr] - unconditional jump to the specified instruction
	OP_JF,				  // OP_jmp_false [instr] - jumps to the specified instruction if the value from stack is false
	OP_JT,				  // OP_jmp_true [instr] - jumps to the specified instruction if the value from stack is true
	OP_FPLOAD,			  // OP_fpload [idx] - puts on stack the value from FP[idx]
	OP_FPSTORE,			  // OP_fpstore [idx] - puts in FP[idx] the value from stack
	OP_ADD_INT,			  // OP_add.i - adds 2 int values from stack and puts the result on stack
	OP_LESS_INT			  // OP_less.i - compares 2 int values from stack and puts the result on stack as int
} Opcode;

typedef struct Instruction Instruction;

// Instruction argument:
typedef union
{
	int i;				  // int and index values
	double f;			  // float values
	void *p;			  // function pointers
	void (*externalFn)(); // pointer to an extern (host) function
	Instruction *instr;	  // pointer to an instruction
} Argument;

// Stack Cell:
typedef union
{
	int i;				  // int and index values
	double f;			  // float values
	void *p;			  // function pointers
	void (*externalFn)(); // pointer to an extern (host) function
	Instruction *instr;	  // pointer to an instruction
} StackCell;

// a VM instruction
struct Instruction
{
	Opcode op; // opcode: OP_*
	Argument arg;
	Instruction *next; // the link to the next instruction in list
};

// adds a new instruction to the end of list and sets its "op" field
// returns the newly added instruction
Instruction *addInstruction(Instruction **list, Opcode op);

// add an instruction which has an argument of type int
Instruction *addInstructionWithInt(Instruction **list, Opcode op, int argVal);

// add an instruction which has an argument of type double
Instruction *addInstructionWithDouble(Instruction **list, Opcode op, double argVal);

// MV initialisation
void vmInit();

// executes the code starting with the given instruction (IP - Instruction Pointer)
void run(Instruction *IP);

// generates a test program
Instruction *genTestProgram();
