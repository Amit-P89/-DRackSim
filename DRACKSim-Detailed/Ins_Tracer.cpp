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

#include "pin.H"
#include "instlib2.H"
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include<sstream>

std::ofstream TraceFile;

INSTLIB::ICOUNT icount;
UINT64 ram_count;

// Get process number from the script
KNOB<UINT32> KnobProcessNumber(KNOB_MODE_WRITEONCE, "pintool", "P", "1", "Process number");
// Get node number from the script
KNOB<UINT32> KnobNodeNumber(KNOB_MODE_WRITEONCE, "pintool", "N", "1", "Node number");
// Get number of instructions to skip from the script
KNOB<UINT64> KnobNumInsSkip(KNOB_MODE_WRITEONCE, "pintool", "S", "0", "INS_SKIP");
// Get number of instructions to skip from the script
KNOB<UINT64> KnobMultiThread(KNOB_MODE_WRITEONCE, "pintool", "T", "0", "Only Multi_thread");
// Get number of instructions to skip from the script
KNOB<UINT64> KnobMaxIns(KNOB_MODE_WRITEONCE, "pintool", "M", "10000000", "Only Multi_thread");

int Procid;
int Nodeid;
uint64_t INST_to_SKIP=0;
bool only_multi_thread=false;

using std::dec;
using std::endl;
using std::hex;
using std::ios;
using std::string;


// Force each thread's data to be in its own data cache line so that
// multiple threads do not contend for the same data cache line.
// This avoids the false sharing problem.
#define PADSIZE 56

INT32 numThreads = 0;

PIN_LOCK lock;
PIN_MUTEX *Mutex, Mutex1;

// a running count of the instructions
class thread_data_t
{
public:
    thread_data_t() : _count(0) {}
    UINT64 _count;
    UINT8 _pad[PADSIZE];
};

// key for accessing TLS storage in the threads. initialized once in main()
static TLS_KEY tls_key = INVALID_TLS_KEY;

// function to access thread−specific data
thread_data_t *get_tls(THREADID threadid)
{
    thread_data_t *tdata = static_cast<thread_data_t *>(PIN_GetThreadData(tls_key, threadid));
    return tdata;
}

VOID ThreadStart(THREADID threadid, CONTEXT *ctxt, INT32 flags, VOID *v)
{
    PIN_GetLock(&lock, threadid + 1);
    numThreads++;
    PIN_ReleaseLock(&lock);
    thread_data_t *tdata = new thread_data_t;
    PIN_SetThreadData(tls_key, tdata, threadid);
    /*    if (PIN_SetThreadData(tls_key, tdata, threadid) == FALSE)
        {
            cerr << "PIN_SetThreadData failed" << endl;
            PIN_ExitProcess(1);
        }
    */
}


int *main_start;

struct INST
{
    int proc_id;
    UINT64 ins_id;
    INT64 ins_addr;
    int16_t threadid;
    INT64 read_opr;
    int16_t read_size;
    INT64 read_opr2;
    int16_t read_size2; 
    INT64 write_opr;
    int16_t write_size;
    int32_t RR[4];
    int32_t WR[4];
    bool is_Brach;
    bool branch_taken;
    bool branch_prediction;
    int ins_type;
};


INST *q;
UINT64 *num_ins;
int n=100;
int z=0;


UINT64 total_ins=0;
UINT64 ins_to_simulate=0;

string dir_name;
const char *file;
std::ostringstream tf;
int FileID=0;
UINT32 ins_count=0;
uint64_t max_ins=1000000;
UINT32 a=0;
static VOID InsRef(UINT32 threadid, string ins, ADDRINT addr, ADDRINT read_op, UINT32 read_size, ADDRINT read_op2, UINT32 read_size2, 
    ADDRINT write_op, UINT32 write_size, int32_t *R, int num_rr, int32_t *W, int num_wr, bool is_Brach, bool branch_taken,UINT32 category,UINT32 opcode)
{   
    if(only_multi_thread)
    {
       if(numThreads<2)
        {
            PIN_MutexLock(Mutex);
            *main_start=*main_start+1;
            PIN_MutexUnlock(Mutex);
            ins_count=1;
            return;
        }
    }

    PIN_MutexLock(&Mutex1);
    ins_count++;
    // if(ins_count%100000==0)
    //     cout<<ins_count;
    PIN_MutexUnlock(&Mutex1);

    if(ins_count<INST_to_SKIP)
    {
        return;
    }


    if(ins_to_simulate<=total_ins)
    {
        PIN_ExitApplication(0);
    }
    //while(*main_start!=2) { sleep(.001);};

    PIN_MutexLock(&Mutex1);
    
    if((*num_ins)<=max_ins)
    {
        INST temp;
        //strcpy(temp.ins,ins.c_str());
        temp.proc_id=0;
        temp.threadid=threadid;
        temp.ins_addr=addr;
        temp.read_opr=read_op;
        temp.read_size=read_size;
        temp.read_opr2=read_op2;
        temp.read_size2=read_size2;
        temp.write_opr=write_op;
        temp.write_size=write_size;
        temp.is_Brach=is_Brach;
        temp.branch_taken=branch_taken;
        // category = category;//CATEGORY_StringShort(category);
        // string opcode = 
        // temp.opcode = opcode;//OPCODE_StringShort(opcode);
         // string opcode = ins.opcode;
        // string category = ins.category;
        // //
        int pos = 0;
        // str1.find(str2, pos)) != string::npos
        if(CATEGORY_StringShort(category)=="SSE"){
                temp.ins_type =  6;
        } 
        else if(CATEGORY_StringShort(category) == "X87_ALU"){
                if(OPCODE_StringShort(opcode).find("MUL",pos)!= string::npos){
                        temp.ins_type =  5;
                }
                else if(OPCODE_StringShort(opcode).find("DIV",pos)!= string::npos){
                        temp.ins_type =  4;
                }
                else{
                        temp.ins_type =  3;
                }
        } 
        else if(CATEGORY_StringShort(category)=="NOP"||CATEGORY_StringShort(category)=="WIDENOP"){
                temp.ins_type =  7;
        }
        else{
                if(OPCODE_StringShort(opcode).find("MUL",pos)!= string::npos){
                        temp.ins_type =  1;
                }
                else if(OPCODE_StringShort(opcode).find("DIV",pos)!= string::npos){
                        temp.ins_type =  2;
                }
                else{
                        temp.ins_type =  0;
                } 
        }
        // cout<<CATEGORY_StringShort(category)<<setw(30)<<OPCODE_StringShort(opcode)<<setw(30)<<temp.ins_type<<endl;
        
        for(int i=0;i<num_rr;i++)
        {
            if(R[i]>1024)
                return;
            temp.RR[i]=R[i];
            // if(temp.RR[i]>155)
            //     cin.get();
        }
        for(int i=num_rr;i<4;i++)
        {
            temp.RR[i]=-1;
            // if(temp.RR[i]>155)
            //     cin.get();
        }

        for(int i=0;i<num_wr;i++)
        {
            if(W[i]>1024)
                return;
            temp.WR[i]=W[i];
            // if(temp.WR[i]>155)
            //     cin.get();
        }
        for(int i=num_wr;i<4;i++)
        {
            temp.WR[i]=-1;
            // if(temp.WR[i]>155)
            //     cin.get();
        }

       if((*num_ins)%100000 == 0) cout<<hex<<"\n addr"<<temp.ins_addr;//<<" op1 "<<read_op<<" op2 "<<read_op2<<" wop "<<write_op<<dec<<" r0 "<<temp.RR[0]<<" r1 "<<temp.RR[1]<<" r2 "<<temp.RR[2]<<" r3 "<<temp.RR[3]<<
        //" w0 "<<temp.WR[0]<<" w1 "<<temp.WR[1]<<" w2 "<<temp.WR[2]<<" w3 "<<temp.WR[3];
        TraceFile.write((char*)&temp,sizeof(temp));
        (*num_ins)++;
        total_ins++;
        if((*num_ins)%100000 == 0)cout<<"  "<<dec<<*num_ins<<" ";
    }
    else if((*num_ins)>max_ins)
    {
        TraceFile.close();
        while(1)
        {
            cout<<*num_ins;
            while((*num_ins)>0)
            {
                sleep(1);
            }
            if((*num_ins)==0)
            {
                // remove(file);
                FileID++;
                tf.str("");
                tf<<FileID;
                string file_name = dir_name + "/TraceFile"+ tf.str() + ".trc";
                cout<<"\nnext file "<<file_name;
                file = file_name.c_str();
                TraceFile.open(file);
                break;
            }
        }
    }
    //TraceFile<<"\n Tid: "<<dec<<threadid<<" ins_addr: "<<hex<<addr<<dec<<" Op_Count:"<<OP_Count<<hex<<"  "<<op1<<"  "<<dec<<op1_size<<"  "<<hex<<op2<<"  "<<dec<<op2_size<<"  "<<hex<<op3<<"  "<<dec<<op3_size;
    PIN_MutexUnlock(&Mutex1);
}



// called on instruction reference and stores the access in a buffer
static VOID Instruction(INS ins, VOID *v)
{
    UINT32 readOperandCount = 0, writeOperandCount = 0;
    int max_r = INS_MaxNumRRegs(ins);
    int max_w = INS_MaxNumWRegs(ins);

  //  UINT32 Operands = INS_OperandCount(ins);
    UINT32 memOperands = INS_MemoryOperandCount(ins);
 //   UINT32 readsize=0;
    for (UINT32 opIdx = 0; opIdx < memOperands; opIdx++)
    {
        if (INS_MemoryOperandIsRead(ins, opIdx))
        {
            /*if(opIdx==1)
                readsize=INS_MemoryOperandSize(ins, opIdx);
*/            readOperandCount++;
        }
        if (INS_MemoryOperandIsWritten(ins, opIdx))
        {
            writeOperandCount++;
        }
    }

/*    if(readOperandCount==2)
        cout<<"2";*/

    REG *reg_R=new REG[max_r];
    REG *reg_W=new REG[max_w];
    for(int i=0;i<max_r;i++)
    {
        reg_R[i]=INS_RegR(ins,i);
    }
    for(int i=0;i<max_w;i++)
    {
        reg_W[i]=INS_RegW(ins,i);
    }
     

    if (!INS_IsStandardMemop(ins) || INS_MemoryOperandCount(ins) == 0)
    {
            INS_InsertCall(
        ins, IPOINT_BEFORE, (AFUNPTR)InsRef,
        IARG_THREAD_ID, IARG_PTR, new string(INS_Disassemble(ins)),
        IARG_INST_PTR, IARG_ADDRINT, 0, IARG_UINT32, 0, IARG_ADDRINT, 0, IARG_UINT32, 0, IARG_ADDRINT, 0, IARG_UINT32, 0,
        IARG_PTR, reg_R, IARG_UINT32, max_r, IARG_PTR, reg_W, IARG_UINT32, max_w, IARG_BOOL, INS_IsControlFlow(ins), 
        IARG_BRANCH_TAKEN,IARG_UINT32,INS_Category(ins),IARG_UINT32,INS_Opcode(ins), IARG_END);
    }
    else if(readOperandCount==1 && writeOperandCount==0)
    {
            INS_InsertPredicatedCall(
        ins, IPOINT_BEFORE, (AFUNPTR)InsRef,
        IARG_THREAD_ID, IARG_PTR, new string(INS_Disassemble(ins)),
        IARG_INST_PTR, IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE, IARG_ADDRINT, 0, IARG_UINT32, 0, IARG_ADDRINT, 0, IARG_UINT32, 0,
        IARG_PTR, reg_R, IARG_UINT32, max_r, IARG_PTR, reg_W, IARG_UINT32, max_w, IARG_BOOL, INS_IsControlFlow(ins), 
        IARG_BRANCH_TAKEN, IARG_UINT32,INS_Category(ins),IARG_UINT32,INS_Opcode(ins),IARG_END);
    }
    else if(readOperandCount==0 && writeOperandCount==1)
    {
            INS_InsertPredicatedCall(
        ins, IPOINT_BEFORE, (AFUNPTR)InsRef,
        IARG_THREAD_ID, IARG_PTR, new string(INS_Disassemble(ins)),
        IARG_INST_PTR, IARG_ADDRINT, 0, IARG_UINT32, 0, IARG_ADDRINT, 0, IARG_UINT32, 0, IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE, 
        IARG_PTR, reg_R, IARG_UINT32, max_r, IARG_PTR, reg_W, IARG_UINT32, max_w, IARG_BOOL, INS_IsControlFlow(ins), 
        IARG_BRANCH_TAKEN,IARG_UINT32,INS_Category(ins),IARG_UINT32,INS_Opcode(ins), IARG_END);
    }
    else if(readOperandCount==1 && writeOperandCount==1)
    {
            INS_InsertPredicatedCall(
        ins, IPOINT_BEFORE, (AFUNPTR)InsRef,
        IARG_THREAD_ID, IARG_PTR, new string(INS_Disassemble(ins)),
        IARG_INST_PTR, IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE, IARG_ADDRINT, 0, IARG_UINT32, 0, IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE, 
        IARG_PTR, reg_R, IARG_UINT32, max_r, IARG_PTR, reg_W, IARG_UINT32, max_w, IARG_BOOL, INS_IsControlFlow(ins), 
        IARG_BRANCH_TAKEN,IARG_UINT32,INS_Category(ins),IARG_UINT32,INS_Opcode(ins), IARG_END);
    }
    else if(readOperandCount==2 && writeOperandCount==0)
    {
            INS_InsertPredicatedCall(
        ins, IPOINT_BEFORE, (AFUNPTR)InsRef,
        IARG_THREAD_ID, IARG_PTR, new string(INS_Disassemble(ins)),
        IARG_INST_PTR, IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE, IARG_MEMORYREAD2_EA, IARG_MEMORYREAD_SIZE, IARG_ADDRINT, 0, IARG_UINT32, 0, 
        IARG_PTR, reg_R, IARG_UINT32, max_r, IARG_PTR, reg_W, IARG_UINT32, max_w, IARG_BOOL, INS_IsControlFlow(ins), 
        IARG_BRANCH_TAKEN, IARG_UINT32,INS_Category(ins),IARG_UINT32,INS_Opcode(ins),IARG_END);
    }
    else if(readOperandCount==2 && writeOperandCount==1)
    {
            INS_InsertPredicatedCall(
        ins, IPOINT_BEFORE, (AFUNPTR)InsRef,
        IARG_THREAD_ID, IARG_PTR, new string(INS_Disassemble(ins)),
        IARG_INST_PTR, IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE, IARG_MEMORYREAD2_EA, IARG_MEMORYREAD_SIZE, IARG_ADDRINT, 0, IARG_UINT32, 0,
        IARG_PTR, reg_R, IARG_UINT32, max_r, IARG_PTR, reg_W, IARG_UINT32, max_w, IARG_BOOL, INS_IsControlFlow(ins), 
        IARG_BRANCH_TAKEN, IARG_UINT32,INS_Category(ins),IARG_UINT32,INS_Opcode(ins),IARG_END);
    }

}

// Shared memory keys and IDs
key_t  ShmKEY1, ShmKEY2, ShmKEY4;
int ShmID1, ShmID2, ShmID4;

// finish function for the pintool
static VOID Fini(int code, VOID *v)
{
    // for(UINT32 i=0;i<*buf_no;i++)
    // TraceFi<<app->buf[i].threadid<<"  "<<app->buf[i].procid<<" "<<app->buf[i].addr<<"  "<<app->buf[i].size<<"  bufNo= "<<app->buf[i].index<<"\n";
    *main_start=10;
        
    PIN_MutexLock(&Mutex1);

    PIN_MutexUnlock(&Mutex1);

    shmdt(Mutex);
    shmdt(main_start);

    shmctl(ShmID1, IPC_RMID, NULL);
    shmctl(ShmID2, IPC_RMID, NULL);

    PIN_MutexFini(Mutex);
    PIN_MutexFini(&Mutex1);

    TraceFile.close();

    cout<<"\nIns_count is: ";
    cout<<" \nwith funct: "<<icount.MultiThreadCount();
    cout<<" \nwith count: "<<num_ins;
    // OUT(ul2);//s[0],ul2s[1],ul2s[2],ul2s[3]);
}

extern int main(int argc, char *argv[])
{
    PIN_Init(argc, argv);

    shmdt(Mutex);
    shmdt(main_start);
    shmdt(num_ins);

    shmctl(ShmID1, IPC_RMID, NULL);
    shmctl(ShmID2, IPC_RMID, NULL);
    shmctl(ShmID4, IPC_RMID, NULL);
    
    // Initialize lock
    PIN_InitLock(&lock);
    // LOCALVAR UL2::CACHE ul2

 
    Nodeid = KnobNodeNumber.Value();
    INST_to_SKIP = KnobNumInsSkip.Value();
    ins_to_simulate = KnobMaxIns.Value();
    
    int multi_thread_knob = KnobMultiThread.Value();
    if(multi_thread_knob==1)
    {
        only_multi_thread=true;
    }

    // Shared mutex between processes on single node to synchronize access on shared instruction buffer
    ShmKEY1 = 10*Nodeid;
    ShmID1 = shmget(ShmKEY1, sizeof(PIN_MUTEX), IPC_CREAT | 0666);
    Mutex = (PIN_MUTEX *)shmat(ShmID1, NULL, 0);
    PIN_MutexInit(Mutex);

    // Variable to inform other processes on this node about start of actual tracing
    ShmKEY2 = 10*Nodeid+1;
    ShmID2 = shmget(ShmKEY2, sizeof(int), IPC_CREAT | 0666);
    main_start = (int *)shmat(ShmID2, NULL, 0);
    if(Nodeid==1)
        *main_start = 0;

    ShmKEY4 = 10*Nodeid+3;
    ShmID4 = shmget(ShmKEY4, sizeof(UINT64), IPC_CREAT | 0666);
    num_ins = (UINT64 *)shmat(ShmID4, NULL, 0);
    *num_ins = 1;

    icount.Activate(INSTLIB::ICOUNT::ModeBoth);

    // Obtain a key for TLS storage
    tls_key = PIN_CreateThreadDataKey(0);

    // Register ThreadStart to be called when a thread starts
    PIN_AddThreadStartFunction(ThreadStart, 0);
    

    // Arguments for StringDec
    UINT32 x = 1;
    char y = 'P';
    string nodeID = StringDec(Nodeid, x, y);

    cout<<"\nnodeID= "<<Nodeid<<"\n";
    dir_name = "./Output/Node" + nodeID;
    mkdir("Output", 0777);
    const char *dir1 = dir_name.c_str();
    mkdir(dir1, 0777);
	
	tf<<FileID;
    
    string file_name = dir_name + "/TraceFile"+ tf.str() + ".trc";
    file = file_name.c_str();
    TraceFile.open(file);
    cout<<file;

    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();

    return 0; // make compiler happy
}
