/*
 * Copyright 2002-2019 Intel Corporation.
 * 
 * This software is provided to you as Sample Source Code as defined in the accompanying
 * End User License Agreement for the Intel(R) Software Development Products ("Agreement")
 * section 1.L.
 * 
 * This software and the related documents are provided as is, with no express or implied
 * warranties, other than those that are expressly stated in the License.
 */

/*! @file
 *  This file contains an ISA-portable PIN tool for functional simulation of
 *  instruction+data TLB+cache hierarchies
 */

using namespace std;
#define CORE_NUM 8
#define TLB_PENALTY 10
#define TLB_MISS 60
#define L1_PENALTY 4
#define L2_PENALTY 12
#define L3_PENALTY 42

#include "pin.H"
#include "instlib2.H"
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
typedef UINT32 CACHE_STATS; // type of cache hit/miss counters
#include "I_D_Cache.H"
#include <boost/preprocessor.hpp>

std::ofstream TraceFile;
std::ofstream CacheStats;
std::ofstream ins_stat;

//Get process number from the script
KNOB<UINT32> KnobProcessNumber(KNOB_MODE_WRITEONCE, "pintool", "P", "1", "Process number");
//Get node number from the script
KNOB<UINT32> KnobNodeNumber(KNOB_MODE_WRITEONCE, "pintool", "N", "1", "Node number");

int Procid;
int Nodeid;


//maximum number of CPU memory accesses to be traced 
#define Max_Accesses 1000000000


using std::hex;
using std::string;
using std::ios;
using std::dec;
using std::endl;


static VOID OUT(CACHE_BASE a,CACHE_BASE b,CACHE_BASE c,CACHE_BASE d)
{
	CACHE_BASE::ACCESS_TYPE L=CACHE_BASE::ACCESS_TYPE_LOAD;
	CACHE_BASE::ACCESS_TYPE S=CACHE_BASE::ACCESS_TYPE_STORE;

	CacheStats<<"\n"<<setfill('-')<<setw(140)<<"\n";
	CacheStats<<"\t\t\t\t"<<a.GetName()<<"\t\t\t\t\t"<<b.GetName()<<"\t\t\t\t\t"<<c.GetName()<<"\t\t\t\t\t"<<d.GetName()<<"\n\n";

	CacheStats<<"Load Hits:"<<setw(14)<<a.Hits(L)<<"\t|\t"<<"Load Hits:"<<setw(14)<<b.Hits(L)<<"\t|\t"<<"Load Hits:"<<setw(14)<<c.Hits(L)<<"\t|\t"<<"Load Hits:"<<setw(14)<<d.Hits(L)<<"\n";

	CacheStats<<"Load Misses:"<<setw(12)<<a.Misses(L)<<"\t|\t"<<"Load Misses:"<<setw(12)<<b.Misses(L)<<"\t|\t"<<"Load Misses:"<<setw(12)<<c.Misses(L)<<"\t|\t"<<"Load Misses:"<<setw(12)<<d.Misses(L)<<"\n";

	CacheStats<<"Load Accesses:"<<setw(10)<<a.Accesses(L)<<"\t|\t"<<"Load Accesses:"<<setw(10)<<b.Accesses(L)<<"\t|\t"<<"Load Accesses:"<<setw(10)<<c.Accesses(L)<<"\t|\t"<<"Load Accesses:"<<setw(10)<<d.Accesses(L)<<"\n";

	CacheStats<<"Load Miss Rate:"<<setw(9)<<(100.0*a.Misses(L)/a.Accesses(L))<<"%\t|\t"<<"Load Miss Rate:"<<setw(9)<<(100.0*b.Misses(L)/b.Accesses(L))<<"%\t|\t"<<"Load Miss Rate:"<<setw(9)<<(100.0*c.Misses(L)/c.Accesses(L))<<"%\t|\t"<<"Load Miss Rate:"<<setw(9)<<(100.0*d.Misses(L)/d.Accesses(L))<<"%\n\n";


	CacheStats<<"Store Hits:"<<setw(13)<<a.Hits(S)<<"\t|\t"<<"Store Hits:"<<setw(13)<<b.Hits(S)<<"\t|\t"<<"Store Hits:"<<setw(13)<<c.Hits(S)<<"\t|\t"<<"Store Hits:"<<setw(13)<<d.Hits(S)<<"\n";

	CacheStats<<"Store Misses:"<<setw(11)<<a.Misses(S)<<"\t|\t"<<"Store Misses:"<<setw(11)<<b.Misses(S)<<"\t|\t"<<"Store Misses:"<<setw(11)<<c.Misses(S)<<"\t|\t"<<"Store Misses:"<<setw(11)<<d.Misses(S)<<"\n";

	CacheStats<<"Store Accesses:"<<setw(9)<<a.Accesses(S)<<"\t|\t"<<"Store Accesses:"<<setw(9)<<b.Accesses(S)<<"\t|\t"<<"Store Accesses:"<<setw(9)<<c.Accesses(S)<<"\t|\t"<<"Store Accesses:"<<setw(9)<<d.Accesses(S)<<"\n";

	CacheStats<<"Store Miss Rate:"<<setw(8)<<(100.0*a.Misses(S)/a.Accesses(S))<<"%\t|\t"<<"Store Miss Rate:"<<setw(8)<<(100.0*b.Misses(S)/b.Accesses(S))<<"%\t|\t"<<"Store Miss Rate:"<<setw(8)<<(100.0*c.Misses(S)/c.Accesses(S))<<"%\t|\t"<<"Store Miss Rate:"<<setw(8)<<(100.0*d.Misses(S)/d.Accesses(S))<<"%\n\n";


	CacheStats<<"Total Hits:"<<setw(13)<<a.Hits()<<"\t|\t"<<"Total Hits:"<<setw(13)<<b.Hits()<<"\t|\t"<<"Total Hits:"<<setw(13)<<c.Hits()<<"\t|\t"<<"Total Hits:"<<setw(13)<<d.Hits()<<"\n";

	CacheStats<<"Total Misses:"<<setw(11)<<a.Misses()<<"\t|\t"<<"Total Misses:"<<setw(11)<<b.Misses()<<"\t|\t"<<"Total Misses:"<<setw(11)<<c.Misses()<<"\t|\t"<<"Total Misses:"<<setw(11)<<d.Misses()<<"\n";

	CacheStats<<"Total Accesses:"<<setw(9)<<a.Accesses()<<"\t|\t"<<"Total Accesses:"<<setw(9)<<b.Accesses()<<"\t|\t"<<"Total Accesses:"<<setw(9)<<c.Accesses()<<"\t|\t"<<"Total Accesses:"<<setw(9)<<d.Accesses()<<"\n";

	CacheStats<<"Total Miss Rate:"<<setw(8)<<(100.0*a.Misses()/a.Accesses())<<"%\t|\t"<<"Total Miss Rate:"<<setw(8)<<(100.0*b.Misses()/b.Accesses())<<"%\t|\t"<<"Total Miss Rate:"<<setw(8)<<(100.0*c.Misses()/c.Accesses())<<"%\t|\t"<<"Total Miss Rate:"<<setw(8)<<(100.0*d.Misses()/d.Accesses())<<"%\n\n";

	CacheStats<<"Total Flushes:"<<setw(10)<<a.Flushes()<<"\t|\t"<<"Total Flushes:"<<setw(10)<<b.Flushes()<<"\t|\t"<<"Total Flushes:"<<setw(10)<<c.Flushes()<<"\t|\t"<<"Total Flushes:"<<setw(10)<<d.Flushes()<<"\n";

	CacheStats<<"Stat Resets:"<<setw(12)<<a.Resets()<<"\t|\t"<<"Stat Resets:"<<setw(12)<<b.Resets()<<"\t|\t"<<"Stat Resets:"<<setw(12)<<c.Resets()<<"\t|\t"<<"Stat Resets:"<<setw(12)<<d.Resets()<<"\n";

}


// Force each thread's data to be in its own data cache line so that
// multiple threads do not contend for the same data cache line.
// This avoids the false sharing problem.
#define PADSIZE 56

INT32 numThreads = 0;

PIN_LOCK lock;
PIN_MUTEX *Mutex,*Mutex1;

// a running count of the instructions
class thread_data_t
{
  public:
    thread_data_t() : _count(0) {}
    UINT64 _count;
    UINT8 _pad[PADSIZE];
};	

// key for accessing TLS storage in the threads. initialized once in main()
static  TLS_KEY tls_key = INVALID_TLS_KEY;

//function to access thread−specific data
thread_data_t* get_tls(THREADID threadid)
{	
     thread_data_t* tdata = static_cast<thread_data_t*>(PIN_GetThreadData(tls_key , threadid));
     return tdata;
}

VOID ThreadStart(THREADID threadid, CONTEXT *ctxt, INT32 flags, VOID *v)
{
    PIN_GetLock(&lock, threadid+1);
    numThreads++;
    PIN_ReleaseLock(&lock);
    thread_data_t* tdata = new thread_data_t;
    PIN_SetThreadData(tls_key, tdata, threadid);
/*    if (PIN_SetThreadData(tls_key, tdata, threadid) == FALSE)
    {
        cerr << "PIN_SetThreadData failed" << endl;
        PIN_ExitProcess(1);
    }
*/
}

//Data Structures For 4-Core Cache Simulation
namespace ITLB
{
    // instruction TLB: 4 kB pages, 128 entries, 8-way associative
    const UINT32 lineSize = 4*KILO;
    const UINT32 cacheSize = 128 * lineSize;
    const UINT32 associativity = 8;
    const CACHE_ALLOC::STORE_ALLOCATION allocation = CACHE_ALLOC::STORE_ALLOCATE;

    const UINT32 max_sets = cacheSize / (lineSize * associativity);
    const UINT32 max_associativity = associativity;

    typedef CACHE_ROUND_ROBIN(max_sets, max_associativity, allocation) CACHE;
}extern ITLB::CACHE itlbs[CORE_NUM];

#define BOOST_PP_LOCAL_LIMITS   (0, CORE_NUM-1)
#define BOOST_PP_LOCAL_MACRO(n) ITLB::CACHE("ITLB"#n, ITLB::cacheSize, ITLB::lineSize, ITLB::associativity), 
ITLB::CACHE itlbs[]=
{
#include "boost/preprocessor/iteration/detail/local.hpp" 
};

namespace DTLB
{
    // data TLB: 4 kB pages, 64 entries, 4-way associative
    const UINT32 lineSize = 4*KILO;
    const UINT32 cacheSize = 64 * lineSize;
    const UINT32 associativity = 4;
    const CACHE_ALLOC::STORE_ALLOCATION allocation = CACHE_ALLOC::STORE_ALLOCATE;

    const UINT32 max_sets = cacheSize / (lineSize * associativity);
    const UINT32 max_associativity = associativity;

    typedef CACHE_ROUND_ROBIN(max_sets, max_associativity, allocation) CACHE;
}extern DTLB::CACHE dtlbs[CORE_NUM];

#define BOOST_PP_LOCAL_LIMITS   (0, CORE_NUM-1)
#define BOOST_PP_LOCAL_MACRO(n) DTLB::CACHE("DTLB"#n, DTLB::cacheSize, DTLB::lineSize, DTLB::associativity), 
DTLB::CACHE dtlbs[]=
{
#include "boost/preprocessor/iteration/detail/local.hpp" 
};

namespace IL1
{
    // 1st level instruction cache: 32 kB, 64 B lines, 8-way associative
    const UINT32 cacheSize = 32*KILO;
    const UINT32 lineSize = 64;
    const UINT32 associativity = 8;
    const CACHE_ALLOC::STORE_ALLOCATION allocation = CACHE_ALLOC::STORE_NO_ALLOCATE;

    const UINT32 max_sets = cacheSize / (lineSize * associativity);
    const UINT32 max_associativity = associativity;

    typedef CACHE_ROUND_ROBIN(max_sets, max_associativity, allocation) CACHE;
}extern IL1::CACHE il1s[CORE_NUM];

#define BOOST_PP_LOCAL_LIMITS   (0, CORE_NUM-1)
#define BOOST_PP_LOCAL_MACRO(n) IL1::CACHE("L1_I"#n, IL1::cacheSize, IL1::lineSize, IL1::associativity), 
IL1::CACHE il1s[]=
{
#include "boost/preprocessor/iteration/detail/local.hpp" 
};


namespace DL1
{
    // 1st level data cache: 32 kB, 64 B lines, 8-way associative
    const UINT32 cacheSize = 32*KILO;
    const UINT32 lineSize = 64;
    const UINT32 associativity = 8;
    const CACHE_ALLOC::STORE_ALLOCATION allocation = CACHE_ALLOC::STORE_NO_ALLOCATE;

    const UINT32 max_sets = cacheSize / (lineSize * associativity);
    const UINT32 max_associativity = associativity;

    typedef CACHE_ROUND_ROBIN(max_sets, max_associativity, allocation) CACHE;
}extern DL1::CACHE dl1s[CORE_NUM];

#define BOOST_PP_LOCAL_LIMITS   (0, CORE_NUM-1)
#define BOOST_PP_LOCAL_MACRO(n) DL1::CACHE("L1_D"#n, DL1::cacheSize, DL1::lineSize, DL1::associativity), 
DL1::CACHE dl1s[]=
{
#include "boost/preprocessor/iteration/detail/local.hpp" 
};

namespace L2
{
    // 2nd level cache: 256 KB, 64 B lines, 4-way associative
    const UINT32 cacheSize = 256*KILO;
    const UINT32 lineSize = 64;
    const UINT32 associativity = 4;
    const CACHE_ALLOC::STORE_ALLOCATION allocation = CACHE_ALLOC::STORE_NO_ALLOCATE;

    const UINT32 max_sets = cacheSize / (lineSize * associativity);
    const UINT32 max_associativity = associativity;

    typedef CACHE_ROUND_ROBIN(max_sets, max_associativity, allocation) CACHE;
}extern L2::CACHE l2s[CORE_NUM];

#define BOOST_PP_LOCAL_LIMITS   (0, CORE_NUM-1)
#define BOOST_PP_LOCAL_MACRO(n) L2::CACHE("L2_"#n, L2::cacheSize, L2::lineSize, L2::associativity), 
L2::CACHE l2s[]=
{
#include "boost/preprocessor/iteration/detail/local.hpp" 
};

/*LOCALVAR UL2::CACHE ul2s[4]={UL2::CACHE("L2_U1", UL2::cacheSize, UL2::lineSize, UL2::associativity), \
UL2::CACHE("L2_U2", UL2::cacheSize, UL2::lineSize, UL2::associativity), \
UL2::CACHE("L2_U3", UL2::cacheSize, UL2::lineSize, UL2::associativity), \
UL2::CACHE("L2_U4", UL2::cacheSize, UL2::lineSize, UL2::associativity) };*/


namespace UL3
{
    // 3rd level unified cache: 16 MB, 64 B lines, 16-way associative
    const UINT32 cacheSize = 16*MEGA;
    const UINT32 lineSize = 64;
    const UINT32 associativity = 16;
    const CACHE_ALLOC::STORE_ALLOCATION allocation = CACHE_ALLOC::STORE_ALLOCATE;

    const UINT32 max_sets = cacheSize / (lineSize * associativity);

    const UINT32 max_associativity = associativity;

    typedef CACHE_ROUND_ROBIN(max_sets, max_associativity, allocation) CACHE;
}
static UL3::CACHE ul3("L3 Unified_Cache", UL3::cacheSize, UL3::lineSize, UL3::associativity);

INSTLIB::ICOUNT icount;
UINT64 ram_count=0;
UINT64 ins_count = 0;
UINT32 window_count=0;

//Stores the traces of instruction
struct Instruction_Buffer
{
    int size;
    CACHE_BASE::ACCESS_TYPE accessType;
    CHAR r;
    ADDRINT addr;
    UINT32 threadid;
    int buf_type;
    int procid;
};

//Structure to create a shared memory instance of shared buffer of instruction trace 
struct window
{
   struct Instruction_Buffer buf[2000000];
}*app;

UINT64 *buf_no,*win;

bool *main_start;

//Called when instruction trace buffer limit is reached, the trace is passed through a 
//multi-core cache-model and misses are stored in a multi-dimensinal buffer, one for each core
static VOID Cache_Sim_And_Prepare_Buffer();

//Called on cache-miss at LLC to record the core-wise miss information in 2-D buffer
//1-D for each core and 2-D for each miss in each core
static VOID RecordAtTable (UINT32, UINT32, ADDRINT, CHAR, UINT32, int);

//Stores the cache penalty for each miss in the cachepenalties array[]
static VOID RecordCachePenalty(UINT32, UINT32);

//calculate penalty in cycles for each miss
static std::size_t PenaltyCalc(BOOL, BOOL, BOOL, BOOL);

//prepare the 1-D final buffer of LLC miss with timing information and write into a trace file
static VOID Prepare_Buffer();

//Reset variables for next window of trace generation
static VOID Refresh_Window();

UINT64 a=0,b=0;



UINT64 total_access_count=0;
//function called on each instruction/memory reference
static VOID FillBuffer(UINT32 threadid, ADDRINT addr, UINT32 size, CACHE_BASE::ACCESS_TYPE accessType, CHAR r, int buf_type)
{
    PIN_MutexLock(Mutex);

//    UINT32 c = icount.MultiThreadCount();

    if((*win)==Max_Accesses)
    {
        UINT32 c = icount.MultiThreadCount();
        ins_count=ins_count+c;
        icount.SetMultithreadCount(0);
        PIN_MutexUnlock(Mutex);
        PIN_ExitApplication(0);
    }
  
    //TraceFi<<threadid<<"  "<<procid<<" "<<addr<<"  "<<size<<"    bufno="<<*buf_no<<"\n";
    if(*buf_no==2000000)
    {
        Cache_Sim_And_Prepare_Buffer();
        Refresh_Window();

        ins_stat<<"\n-----checkpoint-----";
        ins_stat<<"\nTotal memory accesses until now:\t"<<*win;
        ins_stat<<"\nTotal instructions until now:\t"<<icount.MultiThreadCount();
        ins_stat<<"\nTotal cache misses until now:\t"<<total_access_count<<"\n\n";
        //for(UINT32 i=0;i<*buf_no;i++)
            //TraceFi<<app->buf[i].threadid<<"  "<<app->buf[i].procid<<" "<<app->buf[i].addr<<"  "<<app->buf[i].size<<"  bufNo= "<<app->buf[i].index<<"\n";

        //cout<<"\nNode = "<<Nodeid<<" "<<*win; 
        *buf_no=0;      
    }

    *win=*win+1;
    
    //TraceFil<<"buf= "<<*buf_no<<"  "<<"B"<<endl;
    b++;

    if(b==1)
    {
         icount.SetMultithreadCount(0);
     //        *main_start=true;
    }

    if(buf_type==1 || buf_type==2 || buf_type==3)
    {
        app->buf[*buf_no].threadid=threadid;
        app->buf[*buf_no].procid=Procid;
        app->buf[*buf_no].addr=addr;
        app->buf[*buf_no].r=r;
        app->buf[*buf_no].size=size;
        app->buf[*buf_no].accessType=accessType;
    }
    app->buf[*buf_no].buf_type=buf_type;
    //TraceFi<<app->buf[*buf_no].threadid<<"  "<<app->buf[*buf_no].procid<<" "<<app->buf[*buf_no].addr<<"  "<<app->buf[*buf_no].size<<"  bufNo= "<<*win<<"\n";;
    (*buf_no)++;
    PIN_MutexUnlock(Mutex);
}

//3-(Not) Instrumentation functions for cache simulation
//Cache simulation functions to be called on Instruction/Memory trace buffer 

//Caled for cache simulation on Instrction refernce
static VOID InsRef_No_INS(UINT32 threadid, ADDRINT addr, UINT32 size, CACHE_BASE::ACCESS_TYPE accessType, CHAR r, UINT32 procid)
{
    BOOL Miss = false;
    BOOL ul2Hit = false;
    BOOL ul3Hit = false;
    
 /*   const UINT32 size = 1; // assuming access does not cross cache lines
    const CACHE_BASE::ACCESS_TYPE accessType = CACHE_BASE::ACCESS_TYPE_LOAD;*/

    // ITLB
    const BOOL itlbHit = itlbs[threadid % CORE_NUM].AccessSingleLine(addr, accessType, procid);

    // first level I-cache
    const BOOL il1Hit = il1s[threadid % CORE_NUM].AccessSingleLine(addr, accessType, procid);

    // second level unified Cache
    if ( ! il1Hit) 
    {
       ul2Hit = l2s[threadid % CORE_NUM].Access(addr, size, accessType, procid);

        //third level unified Cache
        if(!ul2Hit)
        {
            ul3Hit = ul3.Access(addr, size, accessType, procid);//, lines_read, &linehit[0]);
            if(!ul3Hit)
                Miss=true;
        }
    }


    UINT32 penalty= PenaltyCalc(itlbHit, il1Hit, ul2Hit, ul3Hit);
    
    int block_to_fetch=0;
    if(Miss)
    {
        RecordAtTable(procid, threadid, addr, r, penalty, block_to_fetch);
        ram_count++;
    }
    else
    {
        RecordCachePenalty(threadid,penalty);
    }
 }


//called while simulating cache for multi-line cache request
static VOID MemRefMulti_No_INS(UINT32 threadid, ADDRINT addr, UINT32 size, CACHE_BASE::ACCESS_TYPE accessType, CHAR r, UINT32 procid)
{
    BOOL Miss = false;
    BOOL ul2Hit = false;
    BOOL ul3Hit = false;
    // DTLB
    const BOOL dtlbHit = dtlbs[threadid % CORE_NUM].AccessSingleLine(addr, CACHE_BASE::ACCESS_TYPE_LOAD, procid);

    // first level D-cache
    const BOOL dl1Hit = dl1s[threadid % CORE_NUM].Access(addr, size, accessType, procid);
    
    int lines_read=0;
    bool linehit[2]={false};
    // second level unified Cache
    if ( ! dl1Hit) 
    {
        ul2Hit = l2s[threadid % CORE_NUM].Access(addr, size, accessType, procid);

        //third level unified Cache
        if(!ul2Hit)
        {
            ul3Hit = ul3.Access(addr, size, accessType, procid, lines_read, &linehit[0]);
            if(!ul3Hit)
                Miss=true;
        }
    }

    UINT32 penalty=PenaltyCalc(dtlbHit, dl1Hit, ul2Hit,ul3Hit);
    
    if(Miss)
    {  
        int block_to_fetch=-1;
        if(lines_read==1)
            block_to_fetch=0;
        else if(lines_read==2)
        {
            if(linehit[0]==false && linehit[1]==true)
                block_to_fetch=0;
            if(linehit[0]==true && linehit[1]==false)
                block_to_fetch=1;
            if(linehit[0]==false && linehit[1]==false)
                block_to_fetch=2;
        }
        RecordAtTable(procid, threadid, addr, r, penalty, block_to_fetch);
        ram_count++;
    }
    else
    {
        RecordCachePenalty(threadid,penalty);
    }

}

//called while simulating cache for single-line cache request
static VOID MemRefSingle_No_INS(UINT32 threadid, ADDRINT addr, UINT32 size, CACHE_BASE::ACCESS_TYPE accessType, CHAR r, UINT32 procid)
{
    //Physical_addr P_addr= get_page_frame_number_of_address((void *)addr);
    BOOL Miss = false;
    BOOL ul2Hit = false;
    BOOL ul3Hit = false;

    // DTLB
    const BOOL dtlbHit = dtlbs[threadid % CORE_NUM].AccessSingleLine(addr, CACHE_BASE::ACCESS_TYPE_LOAD, procid);

    // first level D-cache
    const BOOL dl1Hit = dl1s[threadid % CORE_NUM].AccessSingleLine(addr, accessType, procid);

    // second level unified Cache
    if ( ! dl1Hit) 
    {
        ul2Hit = l2s[threadid % CORE_NUM].Access(addr, size, accessType, procid);

        //third level unified Cache
        if(!ul2Hit)
        {
            ul3Hit = ul3.Access(addr, size, accessType, procid);
            if(!ul3Hit)
                Miss=true;
        }
    }


    UINT32 penalty= PenaltyCalc(dtlbHit, dl1Hit, ul2Hit, ul3Hit);
    if(Miss)
    {
        int block_to_fetch=0;
        RecordAtTable(procid, threadid, addr, r, penalty, block_to_fetch);
        ram_count++;
    }   
    else
    {
        RecordCachePenalty(threadid,penalty);
    }
}

UINT32  cachePenalties[CORE_NUM];   //Keep Track of Core-Wise Cache Penalty
static VOID RecordCachePenalty(UINT32 threadid, UINT32 penalty)
{
    //Hit_Miss_Penalty<<"Instruction Penalty is  "<<penalty<<"  ";
    cachePenalties[threadid % CORE_NUM] = cachePenalties[threadid % CORE_NUM] + penalty;
    //Hit_Miss_Penalty<<"Current penalty for thread \t"<<threadid<< "  is \t"<<cachePenalties[threadid % CORE_NUM]<<"\n";
}

//Calculate memory access penalty for each access using hit/miss information at different levels
static std::size_t PenaltyCalc(BOOL bool_tlbHit , BOOL bool_l1Hit, BOOL bool_l2Hit, BOOL bool_l3Hit)
{
    std::size_t int_tlbHit = (bool_tlbHit) ? 1 : 0;
    std::size_t int_tlbMiss = (bool_tlbHit) ? 0 : 1;
    std::size_t int_l1Hit = (bool_l1Hit) ? 1 : 0;
    std::size_t int_l2Hit = (bool_l2Hit) ? 1 : 0;
    std::size_t int_l3Hit = (bool_l3Hit) ? 1 : 0;
    return max(max((int_tlbHit * TLB_PENALTY), (int_tlbMiss * TLB_MISS)) , ((int_l1Hit * L1_PENALTY) + (int_l2Hit * L2_PENALTY) + (int_l3Hit * L3_PENALTY)));
}

//Cache simulation on a memory accesses Trace-buffer
static VOID Cache_Sim_And_Prepare_Buffer()
{
  //  printf("\n*bufno=%ld",*buf_no);
    for(UINT64 i=0;i<*buf_no;i++)
    {
        if(app->buf[i].buf_type==1)
            InsRef_No_INS(app->buf[i].threadid,app->buf[i].addr,app->buf[i].size,app->buf[i].accessType,app->buf[i].r, app->buf[i].procid);
        else if(app->buf[i].buf_type==2)
            MemRefMulti_No_INS(app->buf[i].threadid,app->buf[i].addr,app->buf[i].size,app->buf[i].accessType,app->buf[i].r,app->buf[i].procid);
        else if(app->buf[i].buf_type==3)
            MemRefSingle_No_INS(app->buf[i].threadid,app->buf[i].addr,app->buf[i].size,app->buf[i].accessType,app->buf[i].r,app->buf[i].procid);
    }
    Prepare_Buffer();
}


//Maintains Core-Wise Miss Count
UINT32 miss_count[CORE_NUM]={0};


//Multi-Threaded Cache-Miss, One Row for Each Core/Thread 
//Stores memory access panelty and cycle number for each instruction/memory operation
struct Record
{
    int procid;
    int threadid;
    ADDRINT addr;
    CHAR r;
    UINT32 penalty; 
    UINT64 cycle;
    int block_to_fetch;
}recs[CORE_NUM][1000000];


//Multi-Threaded Ordered Cache-Miss Trace with Cycle Information
struct Trace
{
    int procid;
    int threadid;
    unsigned long long addr;
    char r;
    unsigned long penalty; 
    unsigned long long cycle;
    int block_to_fetch;
};

vector <Trace> tra;
UINT64 trace_count=0;

//Store core-wise cache-miss trace in 2-D buffer (First dimension for each core)
static VOID RecordAtTable (UINT32 procid, UINT32 threadid, ADDRINT addr, CHAR r, UINT32 penalty, int block_to_fetch)
{ 
    recs[threadid % CORE_NUM] [miss_count[threadid % CORE_NUM]].procid = procid;
    recs[threadid % CORE_NUM] [miss_count[threadid % CORE_NUM]].threadid = threadid;
    recs[threadid % CORE_NUM] [miss_count[threadid % CORE_NUM ]].addr = addr ;
    recs[threadid % CORE_NUM] [miss_count[threadid % CORE_NUM ]].r = r ;
    recs[threadid % CORE_NUM] [miss_count[threadid % CORE_NUM ]].penalty = penalty + cachePenalties[threadid % CORE_NUM];    
    recs[threadid % CORE_NUM] [miss_count[threadid % CORE_NUM ]].cycle = (UINT64)0;
    recs[threadid % CORE_NUM] [miss_count[threadid % CORE_NUM ]].block_to_fetch=block_to_fetch;
    miss_count[threadid % CORE_NUM]++;//=miss_count[threadid % CORE_NUM]+1;
    cachePenalties[threadid % CORE_NUM] = 0;
}

UINT64 Last_window_cycle_no[CORE_NUM]={0};
//Combine 2-D multi-threaded cache-miss buffer to single buffer 
//ordered by cycle no. of the memory access
static VOID Prepare_Buffer()
{
    for(UINT32 cores=0; cores<CORE_NUM; cores++)
    {
        for(UINT32 i=0;i<miss_count[cores];trace_count++,i++)
        {
        //  printf("\nbuffer size is %ld",is);
            tra.push_back(Trace());
            tra[trace_count].procid=recs[cores][i].procid;
            tra[trace_count].threadid=recs[cores][i].threadid;
            tra[trace_count].addr=recs[cores][i].addr;
            tra[trace_count].r=recs[cores][i].r;
            tra[trace_count].penalty=recs[cores][i].penalty;
            tra[trace_count].block_to_fetch=recs[cores][i].block_to_fetch;
            if(i==0)
            {
                tra[trace_count].cycle=Last_window_cycle_no[cores];
            }
            else
            {
                tra[trace_count].cycle=tra[trace_count-1].cycle + tra[trace_count-1].penalty;
                Last_window_cycle_no[cores]=tra[trace_count].cycle;
            }   
          //TraceFil<<tra[trace_count].threadid<<"  "<<hex<<tra[trace_count].addr<<"  "<<tra[trace_count].r<<"  "<<dec<<tra[trace_count].penalty<<"  "<<tra[trace_count].cycle<<"  "<<tra[trace_count].cacheFlag<<"\n";              
        }
    }
    for(UINT64 i=0;i<trace_count;i++)
    {
        TraceFile.write((char*)&tra[i],sizeof(tra[i]));
        //TraceFile<<tra[i].procid<<"  "<<tra[i].threadid<<"  "<<hex<<tra[i].addr<<"  "<<tra[i].r<<"  "<<dec<<tra[i].cycle<<"  "<<tra[i].penalty<<"\n";     
    }
}

static VOID Refresh_Window()
{
    total_access_count=total_access_count + trace_count;
    trace_count=0;
    tra.clear();
    for(int i=0;i<CORE_NUM;i++)
    {
        miss_count[i]=0;
    }
}

UINT64 Inst=0;

static VOID InsRef(UINT32 threadid, ADDRINT addr)
{
    //comment this if statements to not skip initial phase of the workload, where only one thread is working and setting up the program 
    if(numThreads<2)    
    {
        Inst++;
        return;
    }
    *main_start=true;
    char r='F';
    CACHE_BASE::ACCESS_TYPE accessType= CACHE_BASE::ACCESS_TYPE_LOAD;
    FillBuffer(threadid,addr,1,accessType,r,1);
}

//3-instrumentation functions

//called on multi cache line memory reference and stores the access in a buffer
static VOID MemRefMulti(UINT32 threadid, ADDRINT addr, UINT32 size, CACHE_BASE::ACCESS_TYPE accessType, CHAR r)
{
    //if(numThreads<2 || 
    if(*main_start==false)
    {
        return;
    }
    FillBuffer(threadid,addr,size,accessType,r,2);
}

//called on single cache line memory reference and stores the access in a buffer
static VOID MemRefSingle(UINT32 threadid, ADDRINT addr, UINT32 size, CACHE_BASE::ACCESS_TYPE accessType, CHAR r)
{    
    //if(numThreads<2 || 
    if(*main_start==false)
    {
        return;
    }
    FillBuffer(threadid,addr,size,accessType,r,3);
}

//called on instruction reference and stores the access in a buffer
static VOID Instruction(INS ins, VOID *v)
{
    // all instruction fetches access I-cache
    INS_InsertCall(
        ins, IPOINT_BEFORE, (AFUNPTR)InsRef,
    IARG_THREAD_ID,
        IARG_INST_PTR,
        IARG_END);

    if (!INS_IsStandardMemop(ins)) return;
    if (INS_MemoryOperandCount(ins) == 0) return;
    ;

    UINT32 readSize = 0, writeSize = 0;
    UINT32 readOperandCount = 0, writeOperandCount = 0;

    for (UINT32 opIdx = 0; opIdx < INS_MemoryOperandCount(ins); opIdx++)
    {
        if (INS_MemoryOperandIsRead(ins, opIdx))
        {
            readSize = INS_MemoryOperandSize(ins, opIdx);
            readOperandCount++;
            break;
        }
        if (INS_MemoryOperandIsWritten(ins, opIdx))
        {
            writeSize = INS_MemoryOperandSize(ins, opIdx);
            writeOperandCount++;
            break;
        }
    }

    if (readOperandCount > 0)
    {
        const AFUNPTR countFun = (readSize <= 4 ? (AFUNPTR)MemRefSingle : (AFUNPTR)MemRefMulti);

        // only predicated-on memory instructions access D-cache
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, countFun, IARG_THREAD_ID, IARG_MEMORYREAD_EA, 
                                IARG_MEMORYREAD_SIZE, IARG_UINT32,CACHE_BASE::ACCESS_TYPE_LOAD, 
                                IARG_UINT32, 'R', IARG_END);
    }

    if (writeOperandCount > 0)
    {
        const AFUNPTR countFun = (writeSize <= 4 ? (AFUNPTR)MemRefSingle : (AFUNPTR)MemRefMulti);

        // only predicated-on memory instructions access D-cache
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, countFun, IARG_THREAD_ID,IARG_MEMORYWRITE_EA, 
                                 IARG_MEMORYWRITE_SIZE, IARG_UINT32, CACHE_BASE::ACCESS_TYPE_STORE, 
                                 IARG_UINT32, 'W', IARG_END);
    }
}


//Shared memory keys and IDs
key_t ShmKEY1,ShmKEY2,ShmKEY3,ShmKEY4,ShmKEY5,ShmKEY6;
int ShmID1,ShmID2,ShmID3,ShmID4,ShmID5,ShmID6;


//finish function for the pintool
static VOID Fini(int code, VOID * v)
{     
    //for(UINT32 i=0;i<*buf_no;i++)
        //TraceFi<<app->buf[i].threadid<<"  "<<app->buf[i].procid<<" "<<app->buf[i].addr<<"  "<<app->buf[i].size<<"  bufNo= "<<app->buf[i].index<<"\n";
    if(*buf_no>0)
    {
        Cache_Sim_And_Prepare_Buffer();
        Refresh_Window();
        ins_stat<<"\n-----checkpoint-----";
        ins_stat<<"\nTotal memory accesses until now:\t"<<*win;
        ins_stat<<"\nTotal instructions until now:\t"<<icount.MultiThreadCount();
        ins_stat<<"\nTotal cache misses until now:\t"<<total_access_count<<"\n\n";
    }
    
    PIN_MutexLock(Mutex1);
    ins_stat<<"\nTotal memory accesses(to cache) by CPU, traced in Node-"<<Nodeid<<" App-"<<Procid<<" : "<<b;
    ins_stat<<"\nTotal instructions traced in Node-"<<Nodeid<<" App-"<<Procid<<" : "<<(ins_count - Inst)<<endl;
    ins_stat<<"\nTotal memory accesses(to cache) by CPU, traced in Node-"<<Nodeid<<" : "<<*win;
    ins_stat<<endl<<endl;
    PIN_MutexUnlock(Mutex1);

    shmdt(app);
    shmdt(buf_no);
    shmdt(win);
    shmdt(Mutex);
    shmdt(main_start);
    shmdt(Mutex1);
    shmctl(ShmID1, IPC_RMID, NULL);
    shmctl(ShmID2, IPC_RMID, NULL);
    shmctl(ShmID3, IPC_RMID, NULL);
    shmctl(ShmID4, IPC_RMID, NULL);
    shmctl(ShmID5, IPC_RMID, NULL);
    shmctl(ShmID6, IPC_RMID, NULL);

    PIN_MutexFini(Mutex);
    PIN_MutexFini(Mutex1);
    CacheStats<<endl<<ram_count;
    TraceFile.close();    
   
    //  PIN_SemaphoreFini(&sem);
    CacheStats<<"\t"<<"Core_0"<<"\t\t\t\t\t"<<"Core_1"<<"\t\t\t\t\t"<<"Core_2"<<"\t\t\t\t\t"<<"Core_3"<<"\t\t\t\t\t\n";
    OUT(itlbs[0],itlbs[1],itlbs[2],itlbs[3]);
    OUT(itlbs[4],itlbs[5],itlbs[6],itlbs[7]);
    OUT(dtlbs[0],dtlbs[1],dtlbs[2],dtlbs[3]);
    OUT(dtlbs[4],dtlbs[5],dtlbs[6],dtlbs[7]);
    OUT(il1s[0],il1s[1],il1s[2],il1s[3]);
    OUT(il1s[4],il1s[5],il1s[6],il1s[7]);
    OUT(dl1s[0],dl1s[1],dl1s[2],dl1s[3]);
    OUT(dl1s[4],dl1s[5],dl1s[6],dl1s[7]);
    OUT(l2s[0],l2s[1],l2s[2],l2s[3]);
    OUT(l2s[4],l2s[5],l2s[6],l2s[7]);
    CacheStats<<ul3;
    //OUT(ul2);//s[0],ul2s[1],ul2s[2],ul2s[3]);
}

extern int main(int argc, char *argv[])
{
    PIN_Init(argc, argv);

    shmdt(app);
    shmdt(buf_no);
    shmdt(win);
    shmdt(Mutex);
    shmdt(main_start);
    shmdt(Mutex1);

    shmctl(ShmID1, IPC_RMID, NULL);
    shmctl(ShmID2, IPC_RMID, NULL);
    shmctl(ShmID3, IPC_RMID, NULL);
    shmctl(ShmID4, IPC_RMID, NULL);
    shmctl(ShmID5, IPC_RMID, NULL);
    shmctl(ShmID6, IPC_RMID, NULL);

	//Initialize lock
	PIN_InitLock(&lock);
	//LOCALVAR UL2::CACHE ul2

    Procid=KnobProcessNumber.Value();
    Nodeid=KnobNodeNumber.Value();

    ShmKEY1 = Nodeid*10+1;
    ShmID1 = shmget(ShmKEY1, sizeof(window), IPC_CREAT | 0666);
    app = (window *)shmat(ShmID1, NULL, 0);

    //Common instruction buffer between multiple processes on a single node
    ShmKEY2 = Nodeid*10+2;
    ShmID2 = shmget(ShmKEY2, sizeof(UINT64), IPC_CREAT | 0666);
    buf_no = (UINT64 *)shmat(ShmID2, NULL, 0);
    *buf_no=0;

    //Variable to terminate application and close pintool after certain no. of instructions
    ShmKEY3 = Nodeid*10+3;
    ShmID3 = shmget(ShmKEY3, sizeof(UINT64), IPC_CREAT | 0666);
    win = (UINT64 *)shmat(ShmID3, NULL, 0);
    *win=0;

    //Shared mutex between processes on single node to synchronize access on shared instruction buffer
    ShmKEY4 = Nodeid*10+4;
    ShmID4 = shmget(ShmKEY4, sizeof(PIN_MUTEX), IPC_CREAT | 0666);
    Mutex = (PIN_MUTEX *)shmat(ShmID4, NULL, 0);
    PIN_MutexInit(Mutex);

    //Variable to inform other processes on this node about start of actual tracing
    ShmKEY5 = Nodeid*10+5;
    ShmID5 = shmget(ShmKEY5, sizeof(UINT64), IPC_CREAT | 0666);
    main_start = (bool *)shmat(ShmID5, NULL, 0);
    *main_start=false;

    //Shared mutex between all processes of all nodes to synchronize the writing of trace meta-data on a file
    ShmKEY6 = 1000;
    ShmID6 = shmget(ShmKEY6, sizeof(PIN_MUTEX), IPC_CREAT | 0666);
    Mutex1 = (PIN_MUTEX *)shmat(ShmID6, NULL, 0);
    PIN_MutexInit(Mutex1);

    icount.Activate(INSTLIB::ICOUNT::ModeBoth);
	
	//Obtain a key for TLS storage
	tls_key=PIN_CreateThreadDataKey(0);

	//Register ThreadStart to be called when a thread starts
	PIN_AddThreadStartFunction(ThreadStart,0);
    
    //Arguments for StringDec
    UINT32 x=1;
    char y='P';
    string nID=StringDec(Nodeid,x,y);
    
    string d1="Output/Node" + nID;
    mkdir("Output",0777);
    const char *dir1=d1.c_str();
    mkdir(dir1,0777);
    
    string f1=d1+"/TraceFile.trc";
    string f2=d1+"/CacheStats.trc";
	string f3=d1+"/Ins_trace_info.log";

    const char *file1=f1.c_str();
    const char *file2=f2.c_str();
    const char *file3=f3.c_str();
    
    TraceFile.open(file1);
    CacheStats.open(file2);
    ins_stat.open(file3);

    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();

    return 0; // make compiler happy
}
