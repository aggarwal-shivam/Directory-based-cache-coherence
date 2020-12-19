#include<stdio.h>
#include "pin.H"
#include<string.h>

FILE *trace;
PIN_LOCK pinLock;

VOID RecordMemRead(VOID *ip, VOID *addr_temp, UINT32 op_size, THREADID tid, UINT32 typo)
{
	PIN_GetLock(&pinLock, tid+1);
	// char op_type;
	// (typo==0) ? op_type
	int op_type=(int)typo;
	unsigned long long addr = (unsigned long long)addr_temp;
    unsigned long long block_capacity=64 - addr%64;

	while(op_size>0)
	{
        while(block_capacity >=8 && op_size >=8)
		{
			fprintf(trace,"%d %d %lld\n", tid, op_type, addr);
			addr = addr+8;
			op_size = op_size-8;
            block_capacity -= 8;
		}
		while(block_capacity >=4 && op_size >=4)
        {
			fprintf(trace,"%d %d %lld\n", tid, op_type, addr);
			addr = addr+4;
			op_size = op_size-4;
            block_capacity -= 4;
		}
		while(block_capacity >=2 && op_size >=2)
        {
			fprintf(trace,"%d %d %lld\n", tid, op_type, addr);
			addr = addr+2;
			op_size = op_size-2;
            block_capacity -= 2;
		}
		while(block_capacity >=1 && op_size >=1)
        {
			fprintf(trace,"%d %d %lld\n", tid, op_type, addr);
			addr = addr+1;
			op_size = op_size-1;
            block_capacity -= 1;
		}
		if(op_size>0)
		{
            block_capacity+=64;
		}
	}
	fflush(trace);
	PIN_ReleaseLock(&pinLock);
}

VOID Instruction(INS ins, VOID *v)
{
	UINT32 num_operands = INS_MemoryOperandCount(ins);

	for(UINT32 operand=0; operand<num_operands; operand++)
	{
		UINT32 op_size = INS_MemoryOperandSize(ins, operand);
		UINT32 typo;
		if(INS_MemoryOperandIsRead(ins, operand))
		{
			typo=0;
			INS_InsertPredicatedCall(
				ins, IPOINT_BEFORE, (AFUNPTR)RecordMemRead,
				IARG_INST_PTR,
				IARG_MEMORYOP_EA, operand,
				IARG_UINT32, op_size,
				IARG_THREAD_ID,
				IARG_UINT32, typo,
				IARG_END);
		}

		if(INS_MemoryOperandIsWritten(ins, operand))
		{
			typo=1;
			INS_InsertPredicatedCall(
				ins, IPOINT_BEFORE, (AFUNPTR)RecordMemRead,
				IARG_INST_PTR,
				IARG_MEMORYOP_EA, operand,
				IARG_UINT32, op_size,
				IARG_THREAD_ID,
				IARG_UINT32, typo,
				IARG_END);
		}
	}
}


VOID Fini(INT32 code, VOID *v)
{
    fclose(trace);
}


INT32 Usage()
{
    PIN_ERROR("This tool collects the thread wise memory access trace\n" 
    			+ KNOB_BASE::StringKnobSummary() + "\n");
    return -1;
}

int main(int argc, char *argv[])
{
	trace=fopen("trace.out","w");
	
    if (PIN_Init(argc, argv)) return Usage();

	INS_AddInstrumentFunction(Instruction,0);

	PIN_AddFiniFunction(Fini, 0);

	PIN_StartProgram();

	return 0;
}

