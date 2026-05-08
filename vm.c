#include <stdio.h>
#include "utils.h"
#include "domain.h"

Instruction *addInstruction(Instruction **program, Opcode op)
{
	Instruction *i = (Instruction *)safeAlloc(sizeof(Instruction));
	i->op = op;
	i->next = NULL;
	if (*program)
	{
		Instruction *p = *program;
		while (p->next)
			p = p->next;
		p->next = i;
	}
	else
	{
		*program = i;
	}
	return i;
}

Instruction *addInstructionWithInt(Instruction **program, Opcode op, int argVal)
{
	Instruction *i = addInstruction(program, op);
	i->arg.i = argVal;
	return i;
}

Instruction *addInstructionWithDouble(Instruction **program, Opcode op, double argVal)
{
	Instruction *i = addInstruction(program, op);
	i->arg.f = argVal;
	return i;
}

StackCell stack[10000];

/**
 * Stack Pointer - Upwards growing stack
 */
StackCell *SP = stack - 1;

/**
 * Frame Pointer
 */
StackCell *FP = NULL;

void pushv(StackCell v)
{
	if (SP + 1 == stack + 10000)
		err("trying to push into a full stack");
	*++SP = v;
}

StackCell popv()
{
	if (SP == stack - 1)
		err("trying to pop from empty stack");
	return *SP--;
}

void pushi(int i)
{
	if (SP + 1 == stack + 10000)
		err("trying to push into a full stack");
	(++SP)->i = i;
}

int popi()
{
	if (SP == stack - 1)
		err("trying to pop from empty stack");
	int i = SP->i;
	SP--;
	return i;
}

void pushp(void *p)
{
	if (SP + 1 == stack + 10000)
		err("trying to push into a full stack");
	(++SP)->p = p;
}

void *popp()
{
	if (SP == stack - 1)
		err("trying to pop from empty stack");
	return SP->p;
}

void put_i()
{
	printf("=> %d", popi());
}

void vmInit()
{
	Symbol *fn = addExternalFunction("put_i", put_i, (Type){TB_VOID, NULL, -1});
	addFnParam(fn, "i", (Type){TB_INT, NULL, -1});
}

void run(Instruction *IP)
{
	StackCell v;
	int iArg, iTop, iBefore;
	void (*extFnPtr)();
	for (;;)
	{
		// shows the index of the current Instructionuction and the number of values from stack
		printf("%p/%d\t", IP, (int)(SP - stack + 1));
		switch (IP->op)
		{
		case OP_HALT:
			printf("HALT");
			return;
		case OP_PUSH_INT:
			printf("PUSH.i\t%d", IP->arg.i);
			pushi(IP->arg.i);
			IP = IP->next;
			break;
		case OP_CALL:
			pushp(IP->next);
			printf("CALL\t%p", IP->arg.instr);
			IP = IP->arg.instr;
			break;
		case OP_CALL_EXTERNAL:
			extFnPtr = IP->arg.externalFn;
			printf("CALL_EXT\t%p\n", extFnPtr);
			extFnPtr();
			IP = IP->next;
			break;
		case OP_ENTER:
			pushp(FP);
			FP = SP;
			SP += IP->arg.i;
			printf("ENTER\t%d", IP->arg.i);
			IP = IP->next;
			break;
		case OP_RETURN_VOID:
			iArg = IP->arg.i;
			printf("RET_VOID\t%d", iArg);
			IP = FP[-1].p;
			SP = FP - iArg - 2;
			FP = FP[0].p;
			break;
		case OP_JMP:
			printf("JMP\t%p", IP->arg.instr);
			IP = IP->arg.instr;
			break;
		case OP_JF:
			iTop = popi();
			printf("JF\t%p\t// %d", IP->arg.instr, iTop);
			IP = iTop ? IP->next : IP->arg.instr;
			break;
		case OP_FPLOAD:
			v = FP[IP->arg.i];
			pushv(v);
			printf("FPLOAD\t%d\t// i:%d, f:%g", IP->arg.i, v.i, v.f);
			IP = IP->next;
			break;
		case OP_FPSTORE:
			v = popv();
			FP[IP->arg.i] = v;
			printf("FPSTORE\t%d\t// i:%d, f:%g", IP->arg.i, v.i, v.f);
			IP = IP->next;
			break;
		case OP_ADD_INT:
			iTop = popi();
			iBefore = popi();
			pushi(iBefore + iTop);
			printf("ADD.i\t// %d+%d -> %d", iBefore, iTop, iBefore + iTop);
			IP = IP->next;
			break;
		case OP_LESS_INT:
			iTop = popi();
			iBefore = popi();
			pushi(iBefore < iTop);
			printf("LESS.i\t// %d<%d -> %d", iBefore, iTop, iBefore < iTop);
			IP = IP->next;
			break;
		default:
			err("run: Instructionuctiune neimplementata: %d", IP->op);
		}
		putchar('\n');
	}
}

/*
// The program `genTestProgram` implements the following AtomC source code:
```
f(2);
void f(int n){		// stack frame: FP[0] = oldFP, FP[-1] = ret_EIP, FP[-2] = n
	int i=0;
	while(i<n){
		put_i(i);
		i=i+1;
		}
	}
```

Which in our VM implementation should produce the following byte code:
```
f:
	ENTER 1
	PUSH.i 0

f_loop: 	; stack frame/layout: [TOP][i, oldFP, ret_EIP, n][BOTTOM]
	FPSTORE -2 ; now the stack layout is: [TOP][n, i, oldFP, ret_EIP, n][BOTTOM]
	LESS_INT ; compares `TOP > BEFORE_TOP`, which here is: `n > i`
	JF f_ret ; i >= n, so jump to return
	PUSH.i i
	CALL_EXTERNAL put_i
	PUSH.i 1 ; now the stack layout is [TOP][1, i, n, i, oldFP, ret_EIP, n][BOTTOM]
	ADD ; adds `TOP + BEFORE_TOP` which here is `1 + i`
	JMP f_loop

f_ret:

PUSH.i 2
CALL f
```
*/
Instruction *genTestProgram()
{
	Instruction *code = NULL;
	addInstructionWithInt(&code, OP_PUSH_INT, 2);
	Instruction *fCall = addInstruction(&code, OP_CALL);
	addInstruction(&code, OP_HALT);

	// create an implementation for "f"
	Instruction *f = addInstructionWithInt(&code, OP_ENTER, 1);

	// int i=0;
	addInstructionWithInt(&code, OP_PUSH_INT, 0);
	addInstructionWithInt(&code, OP_FPSTORE, 1);
	// while(i<n){
	Instruction *whilePos = addInstructionWithInt(&code, OP_FPLOAD, 1);
	addInstructionWithInt(&code, OP_FPLOAD, -2);
	addInstruction(&code, OP_LESS_INT);
	Instruction *jfAfter = addInstruction(&code, OP_JF);
	// put_i(i);
	addInstructionWithInt(&code, OP_FPLOAD, 1);
	Symbol *s = findSymbol("put_i");
	if (!s)
		err("undefined: put_i");
	addInstruction(&code, OP_CALL_EXTERNAL)->arg.externalFn = s->fn.externalFunctionPointer;
	// i=i+1;
	addInstructionWithInt(&code, OP_FPLOAD, 1);
	addInstructionWithInt(&code, OP_PUSH_INT, 1);
	addInstruction(&code, OP_ADD_INT);
	addInstructionWithInt(&code, OP_FPSTORE, 1);
	// } ( the next iteration)
	addInstruction(&code, OP_JMP)->arg.instr = whilePos;
	// returns from function
	jfAfter->arg.instr = addInstructionWithInt(&code, OP_RETURN_VOID, 1);

	fCall->arg.instr = f;

	return code;
}
