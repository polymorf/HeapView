#include "pin.H"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <string>
#include <list>
#include <map>

/* ===================================================================== */
/* Names of malloc and free */
/* ===================================================================== */
#if defined(TARGET_MAC)
#define CALLOC "_calloc"
#define MALLOC "_malloc"
#define FREE "_free"
#define REALLOC "_realloc"
#else
#define CALLOC "calloc"
#define MALLOC "malloc"
#define FREE "free"
#define REALLOC "realloc"
#endif


#define CHUNK_SIZE_ALIGN 0x20
using namespace std;


/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */

class Args;
class Bounds;

ofstream TraceFile;

Args* args = NULL;
Bounds* bounds = NULL;

string ADDRINTToHexString(ADDRINT a)
{
    ostringstream temp;
    temp << "0x" << hex <<a;
    return temp.str();
}

class Args
{
public:
    Args();
    ~Args();
    ADDRINT addr;
    ADDRINT num;
    ADDRINT size;
    ADDRINT retaddr;
};

Args::Args()
{

}

Args::~Args()
{

}

class Bounds
{
public:
    Bounds();
    ~Bounds();
    ADDRINT start;
    ADDRINT end;
};

Bounds::Bounds()
{
    end=0;
    start=~end;
}

Bounds::~Bounds()
{

}

/* ===================================================================== */
/* Analysis routines                                                     */
/* ===================================================================== */

bool is_ld_linux(ADDRINT addr) {
    PIN_LockClient();
    IMG img = IMG_FindByAddress(addr);
    PIN_UnlockClient();
    if (IMG_Valid(img)){
        // todo handle 32 bit and different paths
        if (!IMG_Name(img).compare(0,16,"/lib64/ld-linux-")) {
            return true;
        }
    }
    return false;
}

VOID BeforeMalloc(ADDRINT size, ADDRINT return_ip)
{
    args->size = size;
    args->retaddr = return_ip;
}

VOID AfterMalloc(ADDRINT ret)
{
    if (is_ld_linux(args->retaddr)) {
        return;
    }
    ADDRINT real_alloc_size = args->size;
    if (real_alloc_size % CHUNK_SIZE_ALIGN != 0) {
        real_alloc_size += CHUNK_SIZE_ALIGN - (real_alloc_size % CHUNK_SIZE_ALIGN);
    }
    if(bounds->start > ret) {
        bounds->start = ret - 8;
    } 
    if(bounds->end < (ret + real_alloc_size)) {
        bounds->end = (ret + real_alloc_size);
    }
    TraceFile << "malloc(" << args->size << ") = " << ADDRINTToHexString(ret) << endl;
}

VOID Free(ADDRINT addr, ADDRINT return_ip)
{
    if (is_ld_linux(return_ip)) {
        return;
    }
    string formatted_addr = "";
    if(addr == 0){
        formatted_addr = "0";
    } else {
        formatted_addr = ADDRINTToHexString(addr);
    }
    TraceFile << "free(" + formatted_addr +") = <void>" << endl;
}

VOID BeforeCalloc(ADDRINT num, ADDRINT size, ADDRINT return_ip)
{
    args->num = num;
    args->size = size;
    args->retaddr = return_ip;
}

VOID AfterCalloc(ADDRINT ret)
{
    if (is_ld_linux(args->retaddr)) {
        return;
    }
    ADDRINT real_alloc_size = args->size * args->num;
    if (real_alloc_size % CHUNK_SIZE_ALIGN != 0) {
        real_alloc_size += CHUNK_SIZE_ALIGN - (real_alloc_size % CHUNK_SIZE_ALIGN);
    }
    if(bounds->start > ret) {
        bounds->start = ret - 8;
    } 
    if(bounds->end < (ret + real_alloc_size)) {
        bounds->end = (ret + real_alloc_size);
    }
    TraceFile << "calloc(" << args->num << "," << ADDRINTToHexString(args->size) +") = " + ADDRINTToHexString(ret) << endl;
}

VOID BeforeRealloc(ADDRINT addr, ADDRINT size, ADDRINT return_ip)
{
    args->addr = addr;
    args->size = size;
    args->retaddr = return_ip;
}

VOID AfterRealloc(ADDRINT ret)
{
    if (is_ld_linux(args->retaddr)) {
        return;
    }
    ADDRINT real_alloc_size = args->size;
    if (real_alloc_size % CHUNK_SIZE_ALIGN != 0) {
        real_alloc_size += CHUNK_SIZE_ALIGN - (real_alloc_size % CHUNK_SIZE_ALIGN);
    }
    if(bounds->start > ret) {
        bounds->start = ret - 8;
    } 
    if(bounds->end < (ret + real_alloc_size)) {
        bounds->end = (ret + real_alloc_size);
    }
    cout << "realloc(" << ADDRINTToHexString(args->addr) << "," << args->size << ") = " << ADDRINTToHexString(ret) << endl;
    TraceFile << "realloc(" << ADDRINTToHexString(args->addr) << "," << args->size << ") = " << ADDRINTToHexString(ret) << endl;
}




/* ===================================================================== */
/* Instrumentation routines                                              */
/* ===================================================================== */
   
VOID Image(IMG img, VOID *v)
{
    // Instrument the malloc() and free() functions.  Print the input argument
    // of each malloc() or free(), and the return value of malloc().
    //
    //  Find the malloc() function.
    RTN mallocRtn = RTN_FindByName(img, MALLOC);
    if (RTN_Valid(mallocRtn))
    {
        RTN_Open(mallocRtn);

        // Instrument malloc() to print the input argument value and the return value.
        RTN_InsertCall(mallocRtn, IPOINT_BEFORE, (AFUNPTR)BeforeMalloc,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_RETURN_IP,
                       IARG_END);
        RTN_InsertCall(mallocRtn, IPOINT_AFTER, (AFUNPTR)AfterMalloc,
                       IARG_FUNCRET_EXITPOINT_VALUE, IARG_END);

        RTN_Close(mallocRtn);
    }

    // Find the free() function.
    RTN freeRtn = RTN_FindByName(img, FREE);
    if (RTN_Valid(freeRtn))
    {
        RTN_Open(freeRtn);
        // Instrument free() to print the input argument value.
        RTN_InsertCall(freeRtn, IPOINT_BEFORE, (AFUNPTR)Free,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_RETURN_IP,
                       IARG_END);

        RTN_Close(freeRtn);
    }

    //Find the calloc() function
    RTN callocRtn = RTN_FindByName(img, CALLOC);
    if (RTN_Valid(callocRtn))
    {
        RTN_Open(callocRtn);

        // Instrument callocRtn to print the input argument value and the return value.
        RTN_InsertCall(callocRtn, IPOINT_BEFORE, (AFUNPTR)BeforeCalloc,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                       IARG_RETURN_IP,
                       IARG_END);
        RTN_InsertCall(callocRtn, IPOINT_AFTER, (AFUNPTR)AfterCalloc,
                       IARG_FUNCRET_EXITPOINT_VALUE, IARG_END);

        RTN_Close(callocRtn);
    }
    //Find the realloc() function
    RTN reallocRtn = RTN_FindByName(img, REALLOC);
    if (RTN_Valid(reallocRtn))
    {
        RTN_Open(reallocRtn);

        // Instrument malloc() to print the input argument value and the return value.
        RTN_InsertCall(reallocRtn, IPOINT_BEFORE, (AFUNPTR)BeforeRealloc,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                       IARG_RETURN_IP,
                       IARG_END);
        RTN_InsertCall(reallocRtn, IPOINT_AFTER, (AFUNPTR)AfterRealloc,
                       IARG_FUNCRET_EXITPOINT_VALUE, IARG_END);

        RTN_Close(reallocRtn);
    }
}

VOID WriteMem(ADDRINT insAddr, string insDis, ADDRINT memOp, UINT32 size) {
    if (memOp >= (bounds->start - CHUNK_SIZE_ALIGN) && memOp <= (bounds->end + CHUNK_SIZE_ALIGN) ) {
        PIN_LockClient();
        RTN rtn = RTN_FindByAddress(insAddr);
        PIN_UnlockClient();
        if (RTN_Valid(rtn)) {
            ADDRINT func_addr = RTN_Address(rtn);
            long delta = (long)insAddr - (long)func_addr;
            TraceFile << "memory_write(" << RTN_Name(rtn) << "+" << ADDRINTToHexString(delta) << "," << ADDRINTToHexString(memOp) << "," << size << ")" << endl;
        }else{
            TraceFile << "memory_write(unknown_" << ADDRINTToHexString(insAddr) << "," << ADDRINTToHexString(memOp) << "," << size << ")" << endl;
        }
    }
    return; 
}

VOID CheckMemoryWrites(INS ins, VOID *v)
{
    PIN_LockClient();
    IMG img = IMG_FindByAddress(INS_Address(ins));
    PIN_UnlockClient();
    if (IMG_Valid(img)){
        for (unsigned int i = 0; i < INS_MemoryOperandCount(ins); i++) {
            if (INS_MemoryOperandIsWritten(ins, i)){
                INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteMem,
                 IARG_ADDRINT, INS_Address(ins),
                 IARG_PTR, new string(INS_Disassemble(ins)),
                 IARG_MEMORYOP_EA, i, IARG_MEMORYWRITE_SIZE,
                 IARG_END);
            }
        }
    }
}

/* ===================================================================== */

VOID Fini(INT32 code, VOID *v)
{
    TraceFile.close();
}

/* ===================================================================== */

/* ===================================================================== */
/* Commandline Switches */
/* ===================================================================== */

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "trace", "specify trace file name");
/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */
   
INT32 Usage()
{
    cerr << "This tool produces a visualisation is memory allocator activity." << endl;
    cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char *argv[])
{
    // Initialize pin & symbol manager
    PIN_InitSymbols();
    if( PIN_Init(argc,argv) )
    {
        return Usage();
    }
    TraceFile.open(KnobOutputFile.Value().c_str());
    // Write to a file since TraceFile and cerr maybe closed by the application
    Args* initial = new Args();
    args = initial;
    bounds = new Bounds();
    // Register Image to be called to instrument functions.

    IMG_AddInstrumentFunction(Image, 0);
    INS_AddInstrumentFunction(CheckMemoryWrites, 0);
    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();
    
    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
