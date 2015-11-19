#include "pin.H"
#include <iostream>
#include <sstream>
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

using namespace std;


/*
 * GLOBALS
 */

// forward declaration
class chunklist_t;

/*
 * housekeeping classes
 */
class chunk_t {
public:
    chunk_t(unsigned long address, unsigned long size)
        : mAddress(address), mSize(size)
    {};
    chunk_t(const chunk_t &chunk)
        : mAddress(chunk.mAddress), mSize(chunk.mSize)
    {};
    chunk_t(const chunk_t *chunk)
        : mAddress(chunk->mAddress), mSize(chunk->mSize)
    {};
    ~chunk_t() {};

    bool contains(unsigned long addr) {
        if (addr >= mAddress && addr <= (mAddress+mSize))
            return true;
        return false;
    }

    bool operator< (chunk_t *rhs) {
        return mAddress < rhs->mAddress;
    }
    bool operator< (const chunk_t &rhs) {
        return mAddress < rhs.mAddress;
    };
    bool operator< (const unsigned long address) {
        return mAddress < address;
    }

    const unsigned long address() { return mAddress; };
    const unsigned long size() { return mSize; };

private:
    unsigned long   mAddress;
    unsigned long   mSize;
};


class chunklist_t {
public:
    chunklist_t() 
        : mInserts(0), mRemoves(0)
    {};
    ~chunklist_t() {};

    void insert(unsigned long address, unsigned long size);
    void remove(unsigned long address);
    bool contains(unsigned long address);
    bool has_address(unsigned long address);
    bool in_range(unsigned long address);

    void set_ntdll(unsigned long low, unsigned long high) {
        mNTDLL_low = low; mNTDLL_high =high;
    };
    bool is_ntdll(unsigned long address) {
        if (address >= mNTDLL_low && address <= mNTDLL_high)
            return true;
        return false;
    };

    list<chunk_t>::iterator    begin() {return mChunks.begin(); };
    list<chunk_t>::iterator    end() {return mChunks.end(); };
    unsigned int size() const { return mChunks.size(); }

    unsigned long ntdll_low() const { return mNTDLL_low; }
    unsigned long ntdll_high() const { return mNTDLL_high; }

private:
    list<chunk_t>  mChunks;
    unsigned int        mInserts;
    unsigned int        mRemoves;
    unsigned long       mNTDLL_low;
    unsigned long       mNTDLL_high;
};



/* 
 * GLOBALS (again)
 */
chunklist_t  ChunksList;

void
chunklist_t::insert(unsigned long address, unsigned long size)
{
    size = size + 8 + (0x10-1);
    size = size - (size%0x10);
    chunk_t chunk(address, size);
    list<chunk_t>::iterator    low;

    low = lower_bound(mChunks.begin(), mChunks.end(), chunk);

    mChunks.insert(low, chunk);
}

void
chunklist_t::remove(unsigned long address)
{
    list<chunk_t>::iterator    low;

    low = lower_bound(mChunks.begin(), mChunks.end(), address);

    if (low != mChunks.end() && (*low).address() == address)
        mChunks.erase(low);
}


// address is in a chunk range
bool
chunklist_t::contains(unsigned long address)
{
    list<chunk_t>::iterator    low;

    low = lower_bound(mChunks.begin(), mChunks.end(), address);

    if (low != mChunks.end() && ((*low).contains(address)))
        return true;

    low--; // preceding chunk ? 
    if (low != mChunks.end() && (*low).contains(address))
        return true;
    return false;
}

// has the exact address
bool
chunklist_t::has_address(unsigned long address)
{
    list<chunk_t>::iterator    low;

    low = lower_bound(mChunks.begin(), mChunks.end(), address);

    if (low != mChunks.end() && ((*low).address() == address))
        return true;
    return false;
}

bool
chunklist_t::in_range(unsigned long address)
{
    if (address >= mChunks.front().address() && 
        address <= (mChunks.back().address() + mChunks.back().size()))
        return true;
    return false;
}

/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */

class Args;

ofstream TraceFile;

Args* args = NULL;

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
};

Args::Args()
{

}

Args::~Args()
{

}

/* ===================================================================== */
/* Analysis routines                                                     */
/* ===================================================================== */

VOID BeforeMalloc(ADDRINT size)
{
    args->size = size;
}

VOID AfterMalloc(ADDRINT ret)
{
    TraceFile << "malloc(" << args->size << ") = " << ADDRINTToHexString(ret) << endl;
    ChunksList.insert((unsigned long) ret-8, args->size);
}

VOID Free(ADDRINT addr)
{
    string formatted_addr = "";
    if(addr == 0){
        formatted_addr = "0";
    } else {
        formatted_addr = ADDRINTToHexString(addr);
    }
    TraceFile << "free(" + formatted_addr +") = <void>" << endl;
    ChunksList.remove((unsigned long)addr-8);
}

VOID BeforeCalloc(ADDRINT num, ADDRINT size)
{
    args->num = num;
    args->size = size;
}

VOID AfterCalloc(ADDRINT ret)
{
    TraceFile << "calloc(" << args->num << "," << ADDRINTToHexString(args->size) +") = " + ADDRINTToHexString(ret) << endl;
}

VOID BeforeRealloc(ADDRINT addr, ADDRINT size)
{
    args->addr = addr;
    args->size = size;
}

VOID AfterRealloc(ADDRINT ret)
{
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
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
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
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
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
                       IARG_END);
        RTN_InsertCall(reallocRtn, IPOINT_AFTER, (AFUNPTR)AfterRealloc,
                       IARG_FUNCRET_EXITPOINT_VALUE, IARG_END);

        RTN_Close(reallocRtn);
    }
}

VOID WriteMem(ADDRINT insAddr, string insDis, ADDRINT memOp, UINT32 size) {
    if (ChunksList.contains(memOp)) {
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
