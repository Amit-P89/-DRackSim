#include "utils.h"
#include <boost/preprocessor.hpp>
#include "TLB.h"
#include "ID_Cache.h"


#define TLB_HIT 1
#define TLB_MISS 1
#define L1_HIT 2
#define L2_HIT 18
#define L3_HIT 12
#define page_fault_latency 1
#define chunk_allocation_plus_page_fault_latency 2

#define iqueue_size 64

uint64_t num_local[num_nodes]={0};
uint64_t num_remote[num_nodes]={0};
uint64_t num_remote_pages[num_nodes]={0};
int32_t  num_page_swaps[num_nodes]={0};
uint64_t avg_page_access_time[num_nodes]={0};

uint64_t num_icache_misses[num_nodes][core_count]={0};
uint64_t num_icache_hits[num_nodes][core_count]={0};
uint64_t num_dcache_misses[num_nodes][core_count]={0};
uint64_t num_unique_dcache_misses[num_nodes][core_count]={0};
uint64_t num_dcache_hits[num_nodes][core_count]={0};
uint64_t num_l2_access[num_nodes][core_count]={0};
uint64_t num_l2_hits[num_nodes][core_count]={0};
uint64_t num_l3_access[num_nodes]={0};
uint64_t num_l3_hits[num_nodes]={0};
uint64_t num_l3_misses[num_nodes]={0};
uint64_t num_dl1_writebacks[num_nodes][core_count]={0};
uint64_t num_l2_writebacks[num_nodes][core_count]={0};
uint64_t num_l3_writebacks[num_nodes]={0};

// queue for per-core instruction stream on a node shared by Pintool
queue<INST> core_inst_stream[num_nodes][core_count];

struct RDMA_queue_entry
{
    int pkt_count=0;// if  packets have received has been transferred
    uint64_t page_addr;
    int start_cycle=0;
    int complete_cycle=0;
};

map<uint64_t,RDMA_queue_entry>RDMA_request_queue[num_nodes];

#define mshr 8 // no. of enetries in mshr in each cache-level

// fetches sent to cache
struct INST_FETCH_QUEUE
{
    INST ins;
    // int ins_type;
    int64_t request_cycle;
    bool tlb_fetched;
    int64_t tlb_complete_cycle;
    bool mem_fetched;
    int64_t mem_complete_cycle;

    INST_FETCH_QUEUE()
    {
        tlb_fetched = false;
        mem_fetched = false;
        request_cycle = 0;
        tlb_complete_cycle = 0;
        mem_complete_cycle = 0;
    }

    bool operator<(const INST_FETCH_QUEUE &temp) const
    {
        return (ins.ins_id > temp.ins.ins_id);
    }
};

struct merged_LLC_mshr
{
    int core_id;
    uint64_t ins_id;
    char source_cache;
};
// this structure holds misses from tlb/cache in mshr
struct miss_queue
{
    INST_FETCH_QUEUE inst_fetch;
    uint64_t ins_id;
    uint64_t ins_vaddr;
    uint64_t paddr;
    int node_id;
    int thread_id;
    int64_t request_cycle;
    int64_t complete_cycle;
    char source_cache; // request coming from I or D cache
    int core_id;
    bool write;
    uint64_t mem_trans_id;
    int cache_access_size;
    uint64_t L1_cache_miss_complete_cycle;
    uint64_t L2_cache_miss_complete_cycle;
    uint64_t L3_cache_miss_complete_cycle;
    uint64_t memory_start_cycle;
    vector <uint64_t>ld_str_dest;
    vector <merged_LLC_mshr>merged_mshr_in_shared_LLC;
    miss_queue()
    {
    }
};

deque<miss_queue> temp[num_nodes][core_count]; // temp to store the some instruction belonging to same block if there is no space in ins queue
queue<miss_queue> load_store_buffer[num_nodes][core_count];

// void fill_load_buffer(int node_id)
// {
//     miss_queue temp;
//     int id = 0;
//     uint64_t vaddr = 0x7fa507f75e98;
//     for (int i = 0; i < 100000; i++)
//     {
//         temp.ins_id = id;
//         temp.ins_vaddr = vaddr;
//         temp.inst_fetch.ins.proc_id = 0;
//         id++;
//         uint64_t k = rand() % 100000;
//         int j = rand() % 2;
//         temp.write = j;
//         if (j)
//         {
//             temp.cache_access_size = 1 + rand() % 5;
//         }
//         else
//         {
//             temp.cache_access_size = 1 + rand() % 5;
//         }
//         if (i % 2)
//         {
//             vaddr += k;
//         }
//         else
//             vaddr -= k;
//         temp.node_id = node_id;
//         temp.core_id = (rand()) % core_count;
//         load_store_buffer[node_id][temp.core_id].push(temp);
//     }
// }

vector<miss_queue> itlb_miss_buffer[num_nodes][core_count];
vector<miss_queue> itlb_wait_buffer[num_nodes][core_count];

vector<miss_queue> il1_miss_buffer[num_nodes][core_count];

vector<miss_queue> dl1_miss_buffer[num_nodes][core_count];
vector<miss_queue> il1_wait_buffer[num_nodes][core_count];

vector<miss_queue> l2_miss_buffer[num_nodes][core_count];
vector<miss_queue> l3_miss_buffer[num_nodes];

vector<miss_queue> dtlb_miss_buffer[num_nodes][core_count];

map<uint64_t,pair<uint64_t,uint64_t>>page_fault_entry[num_nodes];

// for adding stalls due to branch misprediction
int branch_misprediction_penalty[num_nodes][core_count] = {0};
void update_LSQ_status(int, int, uint64_t);
INST_FETCH_QUEUE ITLB_HIT(INST &, int, int);
void ITLB_MISS(INST &, int, int);
void page_fault_entry_update(int);
void itlb_mshr_update(int, int);
void dtlb_search(int, int);
void DTLB_MISS(miss_queue &, int, int);
void dtlb_mshr_update(int, int);
bool dcache_hit(int, int, uint64_t, uint64_t, miss_queue &, int);
void dcache_miss(int, int, uint64_t, uint64_t, miss_queue);
void update_dcache_mshr(int, int);
void icache_search(int, int, uint64_t, uint64_t, INST_FETCH_QUEUE);
void update_icache_mshr(int, int);
void l2_cache_search(int, int, uint64_t, uint64_t, miss_queue, bool);
void update_l2_mshr(int, int);
void l3_cache_search(int, uint64_t, uint64_t, miss_queue, bool);
void update_l3_mshr(int);
void update_caches(int);
void get_ins_in_same_block(int, int, miss_queue);
void fetch_ins_in_same_block(int, int, INST_FETCH_QUEUE);
void send_to_memory(miss_queue, int,bool);
void perform_write_back(int, int, int, int16_t, uint64_t, CACHE_BASE::ACCESS_TYPE);
bool page_table_walk(pgd &, uint64_t, uint64_t &);
void handle_page_fault(pgd &, int, uint64_t, uint64_t &, local_addr_space &, bool &);
void memory_request_completed(int);
void LLC_to_memory(int);
void send_RDMA_read(uint64_t,int);
void broadcast_completed_LSQ(int, int, uint64_t);

// queue for fetched instructions
priority_queue<INST_FETCH_QUEUE> inst_queue[num_nodes][core_count];

// queue for storing (DTLB HIT OR MISS) AND DL1 HIT until it's complete cycle arrives.
queue<miss_queue> response_queue[num_nodes][core_count];

// used for giving a transaction-id to all the LLC cache misses
uint64_t mem_trans_id[num_nodes];// = {1,1};
// trans id is assigned separately and from reverse to distinguish it with cache line rem mem accesses
uint64_t RDMA_trans_id[num_nodes];

// used for sending the next instruction to iqueue, instructions are fetched out-of-order
// but are sent to ins_queue in-order
uint64_t last_ins_id[num_nodes][core_count] = {0};

void update_mshr(int node_id)
{
    page_fault_entry_update(node_id);
    memory_request_completed(node_id);
    update_caches(node_id);
    
    for (int i = 0; i < core_count; i++)
    {
        itlb_mshr_update(node_id, i);
        
        dtlb_mshr_update(node_id, i);
    }
    
    LLC_to_memory(node_id);
    update_l3_mshr(node_id);
    

    for (int i = 0; i < core_count; i++)
    {
        update_l2_mshr(node_id, i);

        update_icache_mshr(node_id, i);

        update_dcache_mshr(node_id, i);
    }
}

void itlb_search(int node_id, int core_id)
{
    if (branch_misprediction_penalty[node_id][core_id] > 0)
    {
        return;
    }
    INST temp;
    if (!core_inst_stream[node_id][core_id].empty())
    {
        temp = core_inst_stream[node_id][core_id].front();   
    }
    else
        return;

    uint64_t vaddr = temp.addr;
    // int core_id=temp.threadid%core_count;
    int itlb_id = (node_id * core_count) + core_id;
    uint64_t paddr = NULL;

    // if(core_id==i)
    //     cout<<"good";
    // else
    //     cout<<"bad";

    bool itlb_hit = false;
    itlb_hit = itlbs[itlb_id].AccessTLB(vaddr, TLB_BASE::ACCESS_TYPE_LOAD, temp.proc_id, paddr);
    
    if (itlb_hit)
    {
        // handle branch instructions, add suitable penalty on mis-prediction
        if (temp.is_Branch)
        {
            bool branch_prediction = predict_branch(temp.addr, node_id, core_id);

            if (temp.branch_taken != branch_prediction)
            {
                // wrong branch prediction, penalty will be added during execution stage of this instruction
                temp.branch_miss_predicted = 1;
            }
            else
            {
                // right branch prediction, but do not fetch in this cycle
                temp.branch_miss_predicted = 0;
            }

            last_branch_result(temp.addr, temp.branch_taken, node_id, core_id);
        }
        // perform action for ITLB hit
        INST_FETCH_QUEUE INST_temp = ITLB_HIT(temp, node_id, core_id);
        uint64_t vaddr_offset = vaddr & 0xfff;
        paddr = paddr << 12;
        uint64_t cache_access_paddr = paddr + vaddr_offset;

        // access_cache  //read instructions of whole cache-line,
        // don't access it for each instruction referring to same cacheline
       
        // search in core icache
        icache_search(node_id, core_id, cache_access_paddr, vaddr, INST_temp);
    }
    else
    {
        if (itlb_miss_buffer[node_id][core_id].size() < 1) // mshr for itlb is taken 1 (do not change, hard coded).
        {
            ITLB_MISS(temp, node_id, core_id);
        }
    }
}

INST_FETCH_QUEUE ITLB_HIT(INST &temp, int node_id, int coreid)
{
    INST_FETCH_QUEUE INST_temp;
    INST_temp.ins = temp;
    INST_temp.tlb_fetched = true;
    INST_temp.request_cycle = common_clock;
    INST_temp.tlb_complete_cycle = INST_temp.request_cycle + TLB_HIT;
    return INST_temp;
}

void ITLB_MISS(INST &temp, int node_id, int core_id)
{
    miss_queue miss_temp;
    miss_temp.ins_id = temp.ins_id;
    miss_temp.inst_fetch.ins.proc_id = temp.proc_id;
    miss_temp.ins_vaddr = temp.addr;
    miss_temp.node_id = node_id;
    miss_temp.thread_id = temp.threadid;
    miss_temp.request_cycle = common_clock;
    miss_temp.complete_cycle = -1;

    uint64_t vaddr = temp.addr;
    uint64_t paddr = NULL;
    bool page_found_in_page_table = false;
    uint64_t temp_vaddr = vaddr>>12;
    if(page_fault_entry[node_id].find(temp_vaddr) != page_fault_entry[node_id].end())
    {
       pair<uint64_t,uint64_t> p =  page_fault_entry[node_id][temp_vaddr];
       miss_temp.paddr = p.first;
       miss_temp.complete_cycle = p.second;
       itlb_miss_buffer[node_id][core_id].push_back(miss_temp);
       return;
    }
    page_found_in_page_table = page_table_walk(_pgd[node_id], vaddr, paddr);
    if (page_found_in_page_table)
    {
        miss_temp.complete_cycle = miss_temp.request_cycle + TLB_MISS;
    }
    
    else
    {
        bool new_remote_chunk_allocated = false;
        handle_page_fault(_pgd[node_id], node_id, vaddr, paddr, L[node_id], new_remote_chunk_allocated); // handle by allocating a new page in Local or Remote

        if (new_remote_chunk_allocated == false)
            miss_temp.complete_cycle = miss_temp.request_cycle + page_fault_latency; // page_fault complete after 'n' ms if free page (local/remote) is there
        else
        {
            // if we are allocating remote page and remote memory is not available,
            miss_temp.complete_cycle = miss_temp.request_cycle + chunk_allocation_plus_page_fault_latency; // add extra 'x' ms to allocate a remote memory chunk by global memory,
                                                                                                           // then page allocation
        }
    }
    miss_temp.paddr = paddr;
    itlb_miss_buffer[node_id][core_id].push_back(miss_temp);
}

void page_fault_entry_update(int node_id)
{
    for (auto it = page_fault_entry[node_id].cbegin(), next_it = it; it != page_fault_entry[node_id].cend(); it = next_it)
        {
            ++next_it;
            pair<uint64_t,uint64_t> p = (*it).second;
            if (p.second<=common_clock)
            {
                page_fault_entry[node_id].erase(it);
            }
        }
}

void itlb_mshr_update(int node_id, int core_id)
{
    if (itlb_miss_buffer[node_id][core_id].size() == 1)
    {
        miss_queue miss_buffer_entry = itlb_miss_buffer[node_id][core_id][0];
        if (miss_buffer_entry.complete_cycle == common_clock)
        {
            uint64_t vaddr = miss_buffer_entry.ins_vaddr;
            int proc_id = miss_buffer_entry.inst_fetch.ins.proc_id;
            uint64_t paddr = miss_buffer_entry.paddr;
            int itlb_id = (node_id * core_count) + core_id;
            uint64_t victim_paddr = NULL;
            uint64_t victim_vaddr = NULL;
            itlbs[itlb_id].ReplaceTLB(vaddr, TLB_BASE::ACCESS_TYPE_LOAD, proc_id, paddr, victim_paddr, victim_vaddr);
            itlb_miss_buffer[node_id][core_id].erase(itlb_miss_buffer[node_id][core_id].begin());
        }
    }
}

void icache_search(int node_id, int core_id, uint64_t cache_access_paddr, uint64_t vaddr, INST_FETCH_QUEUE INST_temp)
{
    // search in IL1 Cache
    INST inst = INST_temp.ins;
    bool il1hit = il1s[core_count * node_id + core_id].AccessSingleCacheLine(cache_access_paddr, CACHE_BASE::ACCESS_TYPE_LOAD, inst.proc_id);
    
    if (il1hit && il1_miss_buffer[node_id][core_id].empty() &&
        inst_queue[node_id][core_id].size() < iqueue_size &&
        il1_wait_buffer[node_id][core_id].empty())
    {
        num_icache_hits[node_id][core_id]++;
        INST_temp.mem_fetched = true;
        INST_temp.mem_complete_cycle = common_clock + L1_HIT; // commom cycle should be added in place of request cycle
        if (last_ins_id[node_id][core_id] == core_inst_stream[node_id][core_id].front().ins_id - 1)
        {
            INST_temp.mem_complete_cycle = common_clock;
            inst_queue[node_id][core_id].push(INST_temp);
            last_ins_id[node_id][core_id]++;
            core_inst_stream[node_id][core_id].pop();

            // fetch subsequent instructions in the same block and add to ins_queue
            fetch_ins_in_same_block(node_id, core_id, INST_temp);
        }
    }
    else if (!il1hit)
    {
        miss_queue mshr_temp;
        // Add instr in wait/miss buffer and check in L2.
        if (il1_miss_buffer[node_id][core_id].size() < mshr)
        {
            // check present in missbuffer or not. If not present, add in miss_buffer, otherwise in wait_buffer
            bool miss_buffer_present = false;
            for (auto miss_buffer_entry : il1_miss_buffer[node_id][core_id])
            {
                // 36 bits same = same page
                // next 6 same = same block so 42 bits same
                if ((miss_buffer_entry.ins_vaddr & 0xffffffffffc0) == (vaddr & 0xffffffffffc0))
                {
                    miss_buffer_present = true;
                    mshr_temp = miss_buffer_entry;
                }
            }

            miss_queue miss_new_entry;
            miss_new_entry.inst_fetch = INST_temp;
            miss_new_entry.ins_id = INST_temp.ins.ins_id;
            miss_new_entry.paddr = cache_access_paddr;
            miss_new_entry.ins_vaddr = vaddr;
            miss_new_entry.node_id = node_id;
            miss_new_entry.thread_id = inst.threadid;
            miss_new_entry.request_cycle = INST_temp.request_cycle;
            miss_new_entry.source_cache = 'I';
            miss_new_entry.cache_access_size = 1;
            miss_new_entry.core_id = core_id;
            miss_new_entry.complete_cycle = -1;
            miss_new_entry.write = false;
            miss_new_entry.L1_cache_miss_complete_cycle = common_clock + L1_HIT;
            if (!miss_buffer_present)
            {
                num_icache_misses[node_id][core_id]++;
                il1_miss_buffer[node_id][core_id].push_back(miss_new_entry);
                core_inst_stream[node_id][core_id].pop();
            }
            else if (miss_buffer_present)
            {
                // fetch all the subsequent instructions in same block if there and add to wait buffer
                get_ins_in_same_block(node_id, core_id, miss_new_entry);
            }
        }
    }
}

// On cache-hit store all the instructions belonging to same cache-line in temp queue
// will be pushed directly to ins_queue if space available, otherwise will be pushed later
void fetch_ins_in_same_block(int node_id, int core_id, INST_FETCH_QUEUE INST_temp)
{
    INST temp1 = core_inst_stream[node_id][core_id].front();
    uint64_t cur_miss_blockaddr = INST_temp.ins.addr & (0xffffffffffc0);
    miss_queue miss_new_entry;
    miss_new_entry.inst_fetch = INST_temp;
    miss_new_entry.ins_id = INST_temp.ins.ins_id;
    while (temp1.addr & (0xffffffffffc0) == cur_miss_blockaddr)
    {
        INST_temp.ins = temp1;
        temp[node_id][core_id].push_back(miss_new_entry);
        core_inst_stream[node_id][core_id].pop();
        last_ins_id[node_id][core_id]++;
        if (!core_inst_stream[node_id][core_id].empty())
        {
            temp1 = core_inst_stream[node_id][core_id].front();
        }
        else
            break;
    }
}

// on cache-miss, if subsequent instructions belong to same block, don't add to mshr,
// rather add to this wait buffer
void get_ins_in_same_block(int node_id, int core_id, miss_queue miss_new_entry)
{
    INST temp = core_inst_stream[node_id][core_id].front();
    uint64_t cur_miss_blockaddr = (miss_new_entry.ins_vaddr & 0xffffffffffc0);
    while ((temp.addr & 0xffffffffffc0) == (cur_miss_blockaddr))
    {
        miss_new_entry.inst_fetch.ins = temp;
        miss_new_entry.ins_vaddr = temp.addr;
        miss_new_entry.ins_id = temp.ins_id;
        il1_wait_buffer[node_id][core_id].push_back(miss_new_entry);
        core_inst_stream[node_id][core_id].pop();
        if (!core_inst_stream[node_id][core_id].empty())
            temp = core_inst_stream[node_id][core_id].front();
        else
            break;
    }
}

void update_icache_mshr(int node_id, int core_id)
{
    // replace the cache entry whenever a hit is completed

    if (il1_miss_buffer[node_id][core_id].size() > 0)
        if (il1_miss_buffer[node_id][core_id][0].complete_cycle <= common_clock && il1_miss_buffer[node_id][core_id][0].complete_cycle > 0)
            il1s[core_count * node_id + core_id].ReplaceCACHE((il1_miss_buffer[node_id][core_id][0]).paddr, CACHE_BASE::ACCESS_TYPE_LOAD, (il1_miss_buffer[node_id][core_id][0]).inst_fetch.ins.proc_id);

    // sending fetched instructions from temp queue to ins_queue
    // temp queue holds all the instructions belonging to same cache-line
    while (!temp[node_id][core_id].empty() && inst_queue[node_id][core_id].size() < iqueue_size)
    {
        INST_FETCH_QUEUE ins1 = temp[node_id][core_id].front().inst_fetch;
        // if(last_ins_id[node_id][core_id]==ins1.ins.ins_id-1)
        {
            ins1.mem_complete_cycle = common_clock;
            inst_queue[node_id][core_id].push(ins1);
            temp[node_id][core_id].pop_front();
            // last_ins_id[node_id][core_id]++;
        }
    }

    // if still space left in ins_queue, send more fetched instructions from L1-mshr completed enteries
    if (inst_queue[node_id][core_id].size() < iqueue_size)
    {
        // find the mshr entry with completed memory-access

        if (!il1_miss_buffer[node_id][core_id].empty())
        {
            if (il1_miss_buffer[node_id][core_id][0].complete_cycle <= common_clock &&
                il1_miss_buffer[node_id][core_id][0].complete_cycle > 0 &&
                last_ins_id[node_id][core_id] == il1_miss_buffer[node_id][core_id][0].inst_fetch.ins.ins_id - 1)
            {
                // push to ins_queue
                INST_FETCH_QUEUE ins1 = il1_miss_buffer[node_id][core_id][0].inst_fetch;
                last_ins_id[node_id][core_id]++;
                ins1.mem_complete_cycle = common_clock;
                inst_queue[node_id][core_id].push(ins1);

                uint64_t vaddr = il1_miss_buffer[node_id][core_id][0].ins_vaddr;
                uint64_t ins_id = il1_miss_buffer[node_id][core_id][0].inst_fetch.ins.ins_id;
                il1_miss_buffer[node_id][core_id].erase(il1_miss_buffer[node_id][core_id].begin());

                // get all the instruction enteries from the wait buffer that belongs to same cache-line
                // a related miss of which is just popped from mshr and pushed to ins_queue

                int id=0;
                while (!il1_wait_buffer[node_id][core_id].empty() && ((vaddr & 0xffffffffffc0) == (il1_wait_buffer[node_id][core_id][0].ins_vaddr & 0xffffffffffc0)))
                {
                    if (il1_wait_buffer[node_id][core_id][id].ins_id - 1 == last_ins_id[node_id][core_id] && id<il1_wait_buffer[node_id][core_id].size())
                    {
                        // cout<<"\nsize"<<il1_wait_buffer[node_id][core_id].size()<<" id"<<id;
                        // cin.get();
                        last_ins_id[node_id][core_id]++;
                        temp[node_id][core_id].push_back(il1_wait_buffer[node_id][core_id][id]);
                        id++;
                        // il1_wait_buffer[node_id][core_id].erase(il1_wait_buffer[node_id][core_id].begin());
                    }
                    else
                    {
                        // cout<<"\nsize ="<<il1_wait_buffer[node_id][core_id].size();
                        il1_wait_buffer[node_id][core_id].erase(il1_wait_buffer[node_id][core_id].begin(),il1_wait_buffer[node_id][core_id].begin()+id);
                        // cout<<"\nid="<<id;
                        // cout<<"\nsize ="<<il1_wait_buffer[node_id][core_id].size();
                        // cin.get();
                        break;
                    }
                    if (il1_wait_buffer[node_id][core_id].empty())
                    {
                        break;
                    }
                }
            }
        }
    }

    int64_t tempid = -1;
    if (!il1_miss_buffer[node_id][core_id].empty())
    {
        tempid = il1_miss_buffer[node_id][core_id][0].ins_id;
    }
    if (last_ins_id[node_id][core_id] + 1 != tempid &&
        temp[node_id][core_id].size() < iqueue_size && !il1_wait_buffer[node_id][core_id].empty())
    {
        if (last_ins_id[node_id][core_id] == il1_wait_buffer[node_id][core_id][0].ins_id - 1)
        {
            uint64_t tempaddr = il1_wait_buffer[node_id][core_id][0].ins_vaddr;
            tempaddr = tempaddr & 0xffffffffffc0;
            while ( temp[node_id][core_id].size()<iqueue_size+1000 && tempaddr == (il1_wait_buffer[node_id][core_id][0].ins_vaddr & 0xffffffffffc0))
            {
                if (last_ins_id[node_id][core_id] == il1_wait_buffer[node_id][core_id][0].ins_id - 1)
                {
                    temp[node_id][core_id].push_back(il1_wait_buffer[node_id][core_id][0]);
                    il1_wait_buffer[node_id][core_id].erase(il1_wait_buffer[node_id][core_id].begin());
                    last_ins_id[node_id][core_id]++;
                }
                else
                {
                    break;
                }
                if (il1_wait_buffer[node_id][core_id].empty())
                {
                    break;
                }
            }
        }
    }
    // from newly filled temp to ins_queue if space left in ins_queue, otherwise try next cycle
    while (inst_queue[node_id][core_id].size() < iqueue_size && !temp[node_id][core_id].empty())
    {
        //if(last_ins_id[node_id][core_id]==temp[node_id][core_id].front().ins_id-1)
        {
            temp[node_id][core_id].front().inst_fetch.mem_complete_cycle = common_clock;
            inst_queue[node_id][core_id].push(temp[node_id][core_id].front().inst_fetch);
            temp[node_id][core_id].pop_front();
            // last_ins_id[node_id][core_id]++;
        }
        // else
        //     break;
    }
}

bool dtlb_to_dcache[num_nodes][core_count] = {0};
void dtlb_search(int node_id, int core_id)
{
    if (branch_misprediction_penalty[node_id][core_id] > 0)
    {
        return;
    }
    miss_queue load_store_entry;
    if (!load_store_buffer[node_id][core_id].empty())
    {
        load_store_entry = load_store_buffer[node_id][core_id].front();
    }
    else
        return;
    uint64_t vaddr = load_store_entry.ins_vaddr;
    uint64_t paddr = NULL;
    int dtlb_id = (node_id * core_count) + core_id;
    bool dtlb_hit = false;
    dtlb_hit = dtlbs[dtlb_id].AccessTLB(vaddr, TLB_BASE::ACCESS_TYPE_LOAD, load_store_entry.inst_fetch.ins.proc_id, paddr);
    load_store_entry.paddr = paddr;
    if (dtlb_hit)
    {
        if (dtlb_to_dcache[node_id][core_id])
        {
            dtlb_to_dcache[node_id][core_id] = 0;
            return;
        }
        // if dtlb hit get the physical address from TLB and pass to cache

        uint64_t vaddr_offset = vaddr & 0xfff;
        paddr = paddr << 12;
        uint64_t cache_access_paddr = paddr + vaddr_offset;
        load_store_entry.inst_fetch.request_cycle = common_clock;
        load_store_entry.inst_fetch.tlb_complete_cycle = common_clock + TLB_HIT;
        load_store_entry.inst_fetch.tlb_fetched = true;

        // access dcache n case of dtlb hit
        bool dl1hit = dcache_hit(node_id, core_id, cache_access_paddr, vaddr, load_store_entry, 1);
        
        if (!dl1hit)
        {
            if (dl1_miss_buffer[node_id][core_id].size() < mshr)
            {
                dcache_miss(node_id, core_id, cache_access_paddr, vaddr, load_store_entry);
                load_store_buffer[node_id][core_id].pop();
            }
        }
    }
    else
    {
        for (auto i = 0; i < dtlb_miss_buffer[node_id][core_id].size(); i++)
        {
            if ((vaddr & 0xfffffffff000) == (dtlb_miss_buffer[node_id][core_id][i].ins_vaddr & 0xfffffffff000))
            {
                return;
            }
        }
        if (dtlb_miss_buffer[node_id][core_id].size() < mshr)
        {
            load_store_entry.request_cycle = common_clock;
            DTLB_MISS(load_store_entry, node_id, core_id);
        }
    }
}

void DTLB_MISS(miss_queue &miss_temp, int node_id, int core_id)
{
    uint64_t vaddr = miss_temp.ins_vaddr;
    uint64_t paddr = NULL;
    bool page_found_in_page_table = false;
    uint64_t temp_vaddr = vaddr>>12;
    if(page_fault_entry[node_id].find(temp_vaddr) != page_fault_entry[node_id].end())
    {
       pair<uint64_t,uint64_t> p =  page_fault_entry[node_id][temp_vaddr];
       miss_temp.paddr = p.first;
       miss_temp.complete_cycle = p.second;
       dtlb_miss_buffer[node_id][core_id].push_back(miss_temp);
       load_store_buffer[node_id][core_id].pop();
       return;
    }
    page_found_in_page_table = page_table_walk(_pgd[node_id], vaddr, paddr);
    if (page_found_in_page_table)
    {
        miss_temp.complete_cycle = miss_temp.request_cycle + TLB_MISS;
    }
    else
    {
        bool new_remote_chunk_allocated = false;
        handle_page_fault(_pgd[node_id], node_id, vaddr, paddr, L[node_id], new_remote_chunk_allocated); // handle by allocating a new page in Local or Remote
        if (new_remote_chunk_allocated == false)
            miss_temp.complete_cycle = miss_temp.request_cycle + page_fault_latency; // page_fault complete after 9ms if free page (local/remote) is there
        else
        {
            // if we are allocating remote page and remote memory is not available,
            miss_temp.complete_cycle = miss_temp.request_cycle + chunk_allocation_plus_page_fault_latency; // add 25ms, first 16ns to allocate a remote memory chunk by global memory,
                                                                                                           // then page allocation
        }
    }
    miss_temp.paddr = paddr;
    dtlb_miss_buffer[node_id][core_id].push_back(miss_temp);
    load_store_buffer[node_id][core_id].pop();
}

void dtlb_mshr_update(int node_id, int core_id)
{
    for (auto i = dtlb_miss_buffer[node_id][core_id].begin(); i != dtlb_miss_buffer[node_id][core_id].end(); i++)
    {
        miss_queue miss_buffer_entry = (*i);
        if (miss_buffer_entry.complete_cycle <= common_clock)
        {
            uint64_t vaddr = miss_buffer_entry.ins_vaddr;
            int proc_id = miss_buffer_entry.inst_fetch.ins.proc_id;
            uint64_t paddr = miss_buffer_entry.paddr;
            int dtlb_id = (node_id * core_count) + core_id;
            uint64_t victim_paddr = NULL;
            uint64_t victim_vaddr = NULL;
            dtlbs[dtlb_id].ReplaceTLB(vaddr, TLB_BASE::ACCESS_TYPE_LOAD, proc_id, paddr, victim_paddr, victim_vaddr);
            
            miss_buffer_entry.inst_fetch.tlb_complete_cycle = common_clock + TLB_HIT;
            miss_buffer_entry.inst_fetch.tlb_fetched = true;
            uint64_t vaddr_offset = vaddr & 0xfff;
            paddr = paddr << 12;
            uint64_t cache_access_paddr = paddr + vaddr_offset;
            bool dl1hit = dcache_hit(node_id, core_id, cache_access_paddr, vaddr, miss_buffer_entry, 0);
                
            if (!dl1hit)
            {
                if (dl1_miss_buffer[node_id][core_id].size() < mshr)
                {
                    dcache_miss(node_id, core_id, cache_access_paddr, vaddr, miss_buffer_entry);
                    dtlb_miss_buffer[node_id][core_id].erase(i);
                }
            }
            else
            {
                dtlb_miss_buffer[node_id][core_id].erase(i);
            }
            dtlb_to_dcache[node_id][core_id] = 1;
            break;
        }
    }
}

bool dcache_hit(int node_id, int core_id, uint64_t cache_access_paddr, uint64_t vaddr, miss_queue &load_store_entry, int source)
{
    bool dl1hit = false;
    load_store_entry.source_cache = 'D';
    CACHE_BASE::ACCESS_TYPE acctype = CACHE_BASE::ACCESS_TYPE_LOAD;
    if (load_store_entry.write)
    {
        acctype = CACHE_BASE::ACCESS_TYPE_STORE;
    }

    if (load_store_entry.cache_access_size > 4)
    {
        int line_read = 0;
        bool line_hit[2];
        dl1hit = dl1s[core_count * node_id + core_id].AccessMultiCacheLine(cache_access_paddr,
                                                                           load_store_entry.cache_access_size, acctype, load_store_entry.inst_fetch.ins.proc_id,
                                                                           line_read, line_hit);
    }
    else
    {
        dl1hit = dl1s[core_count * node_id + core_id].AccessSingleCacheLine(cache_access_paddr, acctype, load_store_entry.inst_fetch.ins.proc_id);
    }
    if (dl1hit)
    {
        num_dcache_hits[node_id][core_id]++;
        load_store_entry.complete_cycle = common_clock + L1_HIT;
        response_queue[node_id][core_id].push(load_store_entry);
        if (source == 1)
            load_store_buffer[node_id][core_id].pop();
    }

    return dl1hit;
}

void dcache_miss(int node_id, int core_id, uint64_t cache_access_paddr, uint64_t vaddr, miss_queue load_store_entry)
{
    num_dcache_misses[node_id][core_id]++;
    load_store_entry.source_cache = 'D';

    for (auto it = dl1_miss_buffer[node_id][core_id].begin(); it != dl1_miss_buffer[node_id][core_id].end(); it++)
    {
        if((vaddr>>6) == (*it).ins_vaddr>>6)
        {
            (*it).ld_str_dest.push_back(load_store_entry.ins_id);
            return;
        }
    }

    // to check
    load_store_entry.request_cycle = common_clock;
    load_store_entry.complete_cycle = -1;
    load_store_entry.L1_cache_miss_complete_cycle = common_clock + L1_HIT;
    load_store_entry.paddr = cache_access_paddr;
    dl1_miss_buffer[node_id][core_id].push_back(load_store_entry);
}

void update_dcache_mshr(int node_id, int core_id)
{
    // removing completed mshr enteries and replacing the victim cache blocks with new blocks and performing write-back
    // if modified block is replaced
    for (auto it = dl1_miss_buffer[node_id][core_id].begin(); it != dl1_miss_buffer[node_id][core_id].end(); it++)
    {
        if ((*it).complete_cycle <= common_clock && (*it).complete_cycle > 0)
        {
            int line_read = 0;
            miss_queue load_store_entry = (*it);
            bool line_hit[2];
            CACHE_BASE::ACCESS_TYPE acctype = CACHE_BASE::ACCESS_TYPE_LOAD;
            if (load_store_entry.write)
                acctype = CACHE_BASE::ACCESS_TYPE_STORE;
            uint64_t paddr = (*it).paddr;
            dl1s[core_count * node_id + core_id].AccessMultiCacheLine(paddr, load_store_entry.cache_access_size, acctype, load_store_entry.inst_fetch.ins.proc_id, line_read, line_hit);
            // no block present simply replace and get the corresponding victim

            for (int i = 0; i < line_read; i++)
            {
                if (line_hit[i] == 0)
                {
                    // address of the victim block to write back to memory
                    uint64_t victim_addr = NULL;
                    bool victim_modified = dl1s[core_count * node_id + core_id].ReplaceCACHE(paddr, acctype, (*it).inst_fetch.ins.proc_id, victim_addr);
                    if (victim_modified)
                    {
                        int cache_level = 1; // cache level from which write-back should happen
                        // need address of replaced block
                        perform_write_back(node_id, core_id, cache_level, (*it).inst_fetch.ins.proc_id, victim_addr, acctype);
                    }
                    GetNextBlockAddress(paddr, (*it).inst_fetch.ins.proc_id);
                }
            }

            for(auto k: (*it).ld_str_dest)
            {
               broadcast_completed_LSQ(node_id, core_id,k);
               // num_dcache_misses[node_id][core_id]++;
            }
            
            broadcast_completed_LSQ(node_id, core_id, (*it).ins_id);
            num_unique_dcache_misses[node_id][core_id]++;

            dl1_miss_buffer[node_id][core_id].erase(it);
            if (dl1_miss_buffer[node_id][core_id].empty() || it == dl1_miss_buffer[node_id][core_id].end())
            {
                break;
            }
        }
    }
}

// selecting the source mshr (I/D) while adding a pending entry to L2-mshr on L2 miss
int cache_source[num_nodes][core_count] = {1}; // 0 for I-cache, 1 for D-cache

void update_l2_mshr(int node_id, int core_id)
{
    // checking the completion of miss requests in the icache/dcache and start L2 cache search
    // if hit in L2, update completion time in the dcache, otherwise put in L2-mshr
    CACHE_BASE::ACCESS_TYPE acctype = CACHE_BASE::ACCESS_TYPE_LOAD;

    for (int i = 0; i < 2; i++)
    {
        cache_source[node_id][core_id] = (cache_source[node_id][core_id] + 1) % 2;
        switch (cache_source[node_id][core_id])
        {
        case 0:
            if (il1_miss_buffer[node_id][core_id].size() > 0)
            {
                for (auto it = il1_miss_buffer[node_id][core_id].begin(); it != il1_miss_buffer[node_id][core_id].end(); it++)
                {
                    miss_queue load_store_entry = (*it);
                    if ((*it).L1_cache_miss_complete_cycle <= common_clock && (*it).L1_cache_miss_complete_cycle > 0)
                    {
                        (*it).L1_cache_miss_complete_cycle = 0;
                        bool l2hit = l2s[core_count * node_id + core_id].AccessSingleCacheLine((*it).paddr,
                                                                                               acctype, (*it).inst_fetch.ins.proc_id);

                        num_l2_access[node_id][core_id]++;
                        if (l2hit)
                        {
                            num_l2_hits[node_id][core_id]++;
                            (*it).complete_cycle = common_clock + L2_HIT;
                            break;
                        }   
                        else
                        {
                            (*it).L2_cache_miss_complete_cycle = common_clock + L2_HIT;
                            l2_miss_buffer[node_id][core_id].push_back((*it));
                            break;
                        }
                    }
                }
            }
            break;

        case 1:
            if (dl1_miss_buffer[node_id][core_id].size() > 0)
            {
                for (auto it = dl1_miss_buffer[node_id][core_id].begin(); it != dl1_miss_buffer[node_id][core_id].end(); it++)
                {
                    miss_queue load_store_entry = (*it);
                    if ((*it).L1_cache_miss_complete_cycle <= common_clock && (*it).L1_cache_miss_complete_cycle > 0)
                    {
                        (*it).L1_cache_miss_complete_cycle = 0;
                        int line_read = 0;
                        bool line_hit[2];
                        if (load_store_entry.write)
                            acctype = CACHE_BASE::ACCESS_TYPE_STORE;

                        bool l2hit = l2s[core_count * node_id + core_id].AccessMultiCacheLine(load_store_entry.paddr,
                                                                                              load_store_entry.cache_access_size, acctype, load_store_entry.inst_fetch.ins.proc_id,
                                                                                              line_read, line_hit);

                        num_l2_access[node_id][core_id]++;
                        if (l2hit)
                        {
                            num_l2_hits[node_id][core_id]++;
                            (*it).complete_cycle = common_clock + L2_HIT;
                            break;
                        }
                        else
                        {
                            (*it).L2_cache_miss_complete_cycle = common_clock + L2_HIT;
                            l2_miss_buffer[node_id][core_id].push_back((*it));
                            break;
                        }
                    }
                }
            }
            break;
        }
    }
    cache_source[node_id][core_id] = (cache_source[node_id][core_id] + 1) % 2;

    // removing from l2-mshr whenever a pending miss completes
    for (auto it = l2_miss_buffer[node_id][core_id].begin(); it != l2_miss_buffer[node_id][core_id].end(); ++it)
    {
        auto miss_buffer_entry = (*it);
        // Replace the cache entery after a hit completes
        if (miss_buffer_entry.complete_cycle <= common_clock && miss_buffer_entry.complete_cycle > 0)
        {
            int line_read = 0;
            bool line_hit[2];
            CACHE_BASE::ACCESS_TYPE acctype = CACHE_BASE::ACCESS_TYPE_LOAD;
            if ((*it).write)
                acctype = CACHE_BASE::ACCESS_TYPE_STORE;
            uint64_t paddr = (*it).paddr;
            miss_queue load_store_entry = (*it);
            l2s[core_count * node_id + core_id].AccessMultiCacheLine(paddr, load_store_entry.cache_access_size,
                                                                     acctype, load_store_entry.inst_fetch.ins.proc_id, line_read, line_hit);
            // no block present, simply replace and get the corresponding victim

            for (int i = 0; i < line_read; i++)
            {
                if (line_hit[i] == 0)
                {
                    // address of the victim block to write back to memory
                    uint64_t victim_addr = NULL;
                    bool victim_modified = l2s[core_count * node_id + core_id].ReplaceCACHE(paddr, acctype, (*it).inst_fetch.ins.proc_id, victim_addr);
                    if (victim_modified)
                    {
                        int cache_level = 2; // cache level from which write-back should happen
                        // need address of replaced block
                        perform_write_back(node_id, core_id, cache_level, (*it).inst_fetch.ins.proc_id, victim_addr, acctype);
                    }
                    GetNextBlockAddress(paddr, (*it).inst_fetch.ins.proc_id);
                }
            }
            l2_miss_buffer[node_id][core_id].erase(it);
            if (l2_miss_buffer[node_id][core_id].size() == 0 || it == l2_miss_buffer[node_id][core_id].end())
                break;
        }
    }
}
int core[num_nodes] = {0};
void update_l3_mshr(int node_id)
{

    //remove l3-mshr entries if memory request is complete
    for (auto it = l3_miss_buffer[node_id].begin(); it != l3_miss_buffer[node_id].end(); ++it)
    {
        miss_queue miss_buffer_entry = *it;
        if (miss_buffer_entry.complete_cycle <= common_clock && miss_buffer_entry.complete_cycle > 0)
        {
            int line_read = 0;
            bool line_hit[2];
            CACHE_BASE::ACCESS_TYPE acctype = CACHE_BASE::ACCESS_TYPE_LOAD;
            if ((*it).write)
                acctype = CACHE_BASE::ACCESS_TYPE_STORE;
            uint64_t paddr = (*it).paddr;
            miss_queue load_store_entry = (*it);
            ul3[node_id].AccessMultiCacheLine(paddr, load_store_entry.cache_access_size, acctype, load_store_entry.inst_fetch.ins.proc_id, line_read, line_hit);
            // no block present simply replace and get the corresponding victim
            for (int i = 0; i < line_read; i++)
            {
                if (line_hit[i] == 0)
                {
                    // address of the victim block to write back to memory
                    uint64_t victim_addr = NULL;
                    bool victim_modified = ul3[node_id].ReplaceCACHE(paddr, acctype, (*it).inst_fetch.ins.proc_id, victim_addr);
                    if (victim_modified)
                    {
                        int cache_level = 3; // cache level from which write-back should happen
                        // need address of replaced block
                        perform_write_back(node_id, (*it).core_id, cache_level, (*it).inst_fetch.ins.proc_id, victim_addr, acctype);
                    }
                    GetNextBlockAddress(paddr, (*it).inst_fetch.ins.proc_id);
                }
            }

            l3_miss_buffer[node_id].erase(it);
        }

        if (l3_miss_buffer[node_id].size() == 0 || it == l3_miss_buffer[node_id].end())
            break;
    }

    // bring pending MSHR enteries from L2 to L3, give each core an equal chance
    bool ok = 0;
    int temp_coreid = core[node_id];
    while (!ok)
    {
        core[node_id] = (core[node_id] + 1) % core_count;

        if (temp_coreid == core[node_id]) // to stop while loop, if same core is given chance again
            ok = 1;

        int coreid=core[node_id];

        for (auto it = l2_miss_buffer[node_id][coreid].begin(); it != l2_miss_buffer[node_id][coreid].end(); it++)
        {
            
            if ((*it).L2_cache_miss_complete_cycle <= common_clock && (*it).L2_cache_miss_complete_cycle > 0)
            {
                ok = 1;     //only one miss to be forwarded from L2 to L3 per cycle
                (*it).L2_cache_miss_complete_cycle = 0;
                CACHE_BASE::ACCESS_TYPE acctype = CACHE_BASE::ACCESS_TYPE_LOAD;

                int line_read = 0;
                bool linehit[2];
                miss_queue load_store_entry = (*it);
                if ((*it).write)
                    acctype = CACHE_BASE::ACCESS_TYPE_STORE;
                // // cout<<"paddr: "load_store_entry.paddr<<"size: "<<load_store_entry.cache_access_size<<"proc id : "<<load_store_entry.inst_fetch.ins.proc_id<<"line_read: "<<line_read<<"linehit: "<<linehit[0]<<linehit[1]<<endl;
                bool l3hit = ul3[node_id].AccessMultiCacheLine(load_store_entry.paddr, load_store_entry.cache_access_size, acctype, load_store_entry.inst_fetch.ins.proc_id, line_read, linehit);

                num_l3_access[node_id]++;
                if (l3hit)
                {
                    num_l3_hits[node_id]++;
                    (*it).complete_cycle = common_clock + L3_HIT;
                    if ((*it).source_cache == 'D')
                    {
                        for (int i = 0; i < dl1_miss_buffer[node_id][coreid].size(); i++)
                        {
                            if (dl1_miss_buffer[node_id][coreid][i].ins_id == (*it).ins_id)
                            {
                                dl1_miss_buffer[node_id][coreid][i].complete_cycle = common_clock + L3_HIT;
                                break;
                            }
                        }
                    }
                    else
                    {
                        for (int i = 0; i < il1_miss_buffer[node_id][coreid].size(); i++)
                        {
                            if (il1_miss_buffer[node_id][coreid][i].ins_id == (*it).ins_id)
                            {
                                il1_miss_buffer[node_id][coreid][i].complete_cycle = common_clock + L3_HIT;
                                break;
                            }
                        }
                    }
                }
                else
                {
                    // add it in l3 mshr;
                    bool l3_present = false;
                    for(auto it1=l3_miss_buffer[node_id].begin();it1!=l3_miss_buffer[node_id].end();it1++)
                    {
                        if(((*it).ins_vaddr>>6) ==  ((*it1).ins_vaddr>>6))
                        {
                            l3_present = true;
                            merged_LLC_mshr temp;
                            temp.core_id = coreid;
                            temp.ins_id = (*it).ins_id;
                            temp.source_cache = (*it).source_cache;
                            (*it1).merged_mshr_in_shared_LLC.push_back(temp);
                            break;
                        }
                    }
                    if(!l3_present)
                    {
                        num_l3_misses[node_id]++;
                        (*it).L3_cache_miss_complete_cycle = common_clock + L3_HIT;
                        l3_miss_buffer[node_id].push_back((*it));
                    }
                    break;
                }
            }
        }
    }
}

// forward last level cache miss request to main memory
void LLC_to_memory(int node_id)
{
    for (auto it = l3_miss_buffer[node_id].begin(); it != l3_miss_buffer[node_id].end(); it++)
    {
        if ((*it).L3_cache_miss_complete_cycle <= common_clock && (*it).L3_cache_miss_complete_cycle > 0)
        {
            (*it).L3_cache_miss_complete_cycle = 0;
            (*it).mem_trans_id = mem_trans_id[node_id];
            core_map[node_id][mem_trans_id[node_id]]=(*it).core_id;
            // pthread_mutex_lock(&lock_mem);
            mem_trans_id[node_id]++;
            // pthread_mutex_unlock(&lock_mem);
            miss_queue miss_entry = (*it);
            miss_entry.request_cycle = common_clock;
            bool write_back = 0;
            send_to_memory(miss_entry, node_id,write_back);
        }
    }
}

void update_caches(int node_id)
{
    //will store all the pending mshrs that are completed in L3, used to update in L2 and L1
    vector <miss_queue> temp;

    for (auto it = l3_miss_buffer[node_id].begin(); it != l3_miss_buffer[node_id].end(); it++)
    {
        if ((*it).complete_cycle <= common_clock && (*it).complete_cycle > 0)
        {
            temp.push_back(*it);
            //break;
        }
    }

    for(int z=0;z<temp.size();z++)
    {
        if (temp[z].core_id != -1)
        {
            int size = l2_miss_buffer[node_id][temp[z].core_id].size();
            for (auto i = l2_miss_buffer[node_id][temp[z].core_id].begin(); i < l2_miss_buffer[node_id][temp[z].core_id].end(); i++)
            {
                if (temp[z].ins_id == (*i).ins_id)
                {
                    (*i).complete_cycle = temp[z].complete_cycle;
                    // update in all other cores
                    for(int k=0;k<temp[z].merged_mshr_in_shared_LLC.size();k++){
                        merged_LLC_mshr tem = temp[z].merged_mshr_in_shared_LLC[k];
                        for (auto i1 = l2_miss_buffer[node_id][tem.core_id].begin(); i1 < l2_miss_buffer[node_id][tem.core_id].end(); i1++)
                            {
                                if(tem.ins_id == (*i1).ins_id){
                                    (*i1).complete_cycle = temp[z].complete_cycle;
                                    break;
                                }
                            }
                    }
                    break;
                }
            }

            char source_cache = temp[z].source_cache;
            if (source_cache == 'I')
            {
                for(auto i=il1_miss_buffer[node_id][temp[z].core_id].begin();i<il1_miss_buffer[node_id][temp[z].core_id].end();i++)
                {
                    if(temp[z].ins_id == (*i).ins_id)
                    {
                        (*i).complete_cycle = temp[z].complete_cycle;
                        //update in respective l1s
                        for(int k=0;k<temp[z].merged_mshr_in_shared_LLC.size();k++){
                            merged_LLC_mshr tem = temp[z].merged_mshr_in_shared_LLC[k];
                            if(tem.source_cache == 'I'){
                                 for(auto i1=il1_miss_buffer[node_id][tem.core_id].begin();i1<il1_miss_buffer[node_id][tem.core_id].end();i1++)
                                    {
                                        if(tem.ins_id == (*i1).ins_id)
                                        {
                                            (*i1).complete_cycle = temp[z].complete_cycle;
                                            break;
                                        }
                                    }
                            }
                            else if(tem.source_cache=='D'){
                                     for(auto i1=dl1_miss_buffer[node_id][tem.core_id].begin();i1<dl1_miss_buffer[node_id][tem.core_id].end();i1++)
                                    {
                                        if(tem.ins_id == (*i1).ins_id)
                                        {
                                            (*i1).complete_cycle = temp[z].complete_cycle;
                                            break;
                                        }
                                    }
                            }
                        }
                        break;
                    }
                }

            }
            else if(source_cache == 'D')
            {
                for(auto i=dl1_miss_buffer[node_id][temp[z].core_id].begin();i<dl1_miss_buffer[node_id][temp[z].core_id].end();i++)
                {
                    if(temp[z].ins_id == (*i).ins_id)
                    {
                        (*i).complete_cycle = temp[z].complete_cycle;
                        for(int k=0;k<temp[z].merged_mshr_in_shared_LLC.size();k++){
                            merged_LLC_mshr tem = temp[z].merged_mshr_in_shared_LLC[k];
                            if(tem.source_cache == 'I'){
                                 for(auto i1=il1_miss_buffer[node_id][tem.core_id].begin();i1<il1_miss_buffer[node_id][tem.core_id].end();i1++)
                                    {
                                        if(tem.ins_id == (*i1).ins_id)
                                        {
                                            (*i1).complete_cycle = temp[z].complete_cycle;
                                            break;
                                        }
                                    }
                            }
                            else if(tem.source_cache=='D'){
                                     for(auto i1=dl1_miss_buffer[node_id][tem.core_id].begin();i1<dl1_miss_buffer[node_id][tem.core_id].end();i1++)
                                    {
                                        if(tem.ins_id == (*i1).ins_id)
                                        {
                                            (*i1).complete_cycle = temp[z].complete_cycle;
                                            break;
                                        }
                                    }
                            }
                        }
                        break;
                    }
                }
            }
        }
    }
    temp.clear();
}

// write back the victim block to the next level of memory after completing the replacement
void perform_write_back(int node_id, int core_id, int cache_level, int16_t proc_id, uint64_t victim_addr, CACHE_BASE::ACCESS_TYPE acctype)
{
    bool l2hit = true, l3hit = true;
    if (cache_level == 1)
    {
        num_dl1_writebacks[node_id][core_id]++;
        l2hit = l2s[core_count * node_id + core_id].AccessSingleCacheLine(victim_addr, acctype, proc_id);
        if (l2hit)
        {
            uint64_t new_victim=NULL;
            bool victim_modified = l2s[core_count * node_id + core_id].ReplaceCACHE(victim_addr, acctype, proc_id,new_victim);
            if(victim_modified)
            {
                int _cache_level=2;
                perform_write_back(node_id,core_id,_cache_level,proc_id,new_victim,CACHE_BASE::ACCESS_TYPE_STORE);
            }
        }
    }
    if (cache_level == 2 || l2hit == false)
    {
        num_l2_writebacks[node_id][core_id]++;
        l3hit = ul3[node_id].AccessSingleCacheLine(victim_addr, acctype, proc_id);
        if (l3hit)
        {
            uint64_t new_victim=NULL;
            bool victim_modified = ul3[node_id].ReplaceCACHE(victim_addr, acctype, proc_id, new_victim);
            if(victim_modified)
            {
                int _cache_level=3;
                perform_write_back(node_id,core_id,_cache_level,proc_id,new_victim,CACHE_BASE::ACCESS_TYPE_STORE);
            }
        }
    }
    if (cache_level == 3 || l3hit == false)
    {
        miss_queue miss_entry;
        miss_entry.paddr = victim_addr;
        miss_entry.mem_trans_id = 0;
        miss_entry.request_cycle = common_clock;
        miss_entry.write = true;
        bool write_back = 1;
        send_to_memory(miss_entry, node_id,write_back);
    }
}

// access to memory is performed, either in local or remote
void send_to_memory(miss_queue miss_new_entry, int node_id,bool is_write_back)
{
    bool isWrite=false;
    if(is_write_back)
    {
        isWrite=true;
        num_l3_writebacks[node_id]++;
    }

    uint64_t mem_access_addr = miss_new_entry.paddr;
    mem_access_addr = (mem_access_addr & 0xffffffffffc0);
    uint64_t start_cycle = miss_new_entry.request_cycle;
    uint64_t miss_cycle = miss_new_entry.request_cycle;
    uint64_t trans_id = miss_new_entry.mem_trans_id;

    unsigned long page_addr = get_page_addr(mem_access_addr);
    unsigned long page_offset_addr = mem_access_addr & (0x000000000fff);
    bool local = L[node_id].is_local(page_addr);
    
    if(local)
    {
        num_local[node_id]++;
        // pthread_mutex_lock(&lock_mem);
        obj.add_one_and_run(local_mem[node_id], mem_access_addr, isWrite, trans_id, start_cycle, miss_cycle, node_id);
        // pthread_mutex_unlock(&lock_mem);
    }
    else
    {
        num_remote[node_id]++;
        
        int remote_pool = L[node_id].find_remote_pool(page_addr);
        uint64_t remote_page_addr = L[node_id].remote_page_addr(page_addr, remote_pool);
        remote_page_addr = remote_page_addr << 12;
        uint64_t remote_mem_access_addr = remote_page_addr + page_offset_addr;
        
        remote_memory_access mem_request;
        mem_request.source = node_id;
        mem_request.dest = remote_pool;
        mem_request.mem_access_addr = remote_mem_access_addr;
        mem_request.cycle = start_cycle;
        mem_request.miss_cycle_num = miss_cycle;
        mem_request.trans_id = trans_id;
        mem_request.isWrite = isWrite;
        mem_request.isRDMA = false;
        to_trans_layer(mem_request, packet_queue_node);
    }
}
// Used to send a mem request to request a page and trans id is assigned separately and from reverse to distinguish it with cache line rem mem accesses
void send_RDMA_read(uint64_t page_addr,int node_id)
{
    RDMA_trans_id[node_id]--;
    uint64_t trans_id = RDMA_trans_id[node_id];
    num_remote_pages[node_id]++;
    int remote_pool = L[node_id].find_remote_pool(page_addr);
    uint64_t remote_page_addr = L[node_id].remote_page_addr(page_addr, remote_pool);
    remote_page_addr = remote_page_addr << 12;
    uint64_t remote_mem_access_addr = remote_page_addr;
    // record_access(remote_mem_access_addr,node_id);

    RDMA_request_queue[node_id][trans_id].start_cycle = common_clock;
    RDMA_request_queue[node_id][trans_id].pkt_count = 0;
    RDMA_request_queue[node_id][trans_id].page_addr = page_addr;

    remote_memory_access mem_request;
    mem_request.source = node_id;
    mem_request.dest = remote_pool;
    mem_request.mem_access_addr = (remote_mem_access_addr&0xfffffffff000);
    mem_request.cycle = common_clock;
    mem_request.miss_cycle_num = common_clock;
    mem_request.trans_id = trans_id;
    mem_request.isWrite = false;
    mem_request.isRDMA = true;
    to_trans_layer(mem_request, packet_queue_node);
}

void RDMA_request_complete(uint64_t trans_id,int node_id)
{
    num_page_swaps[node_id]++;
    uint64_t time_taken_to_swap_page=RDMA_request_queue[node_id][trans_id].complete_cycle - RDMA_request_queue[node_id][trans_id].start_cycle;
    avg_page_access_time[node_id]=avg_page_access_time[node_id]+time_taken_to_swap_page;

    RDMA_request_queue[node_id].erase(trans_id);
}

//to calculate avg. memory latency at LLC (the other way around is to do it through tracking all memory accesses (also done)) 
uint64_t total_mem_cost[num_nodes]={0};
uint64_t total_mem_access[num_nodes]={0};

void memory_request_completed(int node_id)
{
    // memory responses served to core this cycle, max of one memory response per core per cycle

    bool core_mem_complete[core_count] = {0};
    int i = node_id;
    int size = memory_completion_queue[i].size();

    for (auto j = memory_completion_queue[i].begin(); j != memory_completion_queue[i].end(); j++)
    {

        if ((*j).cycle <= common_clock)
        {
            uint64_t trans_id = (*j).trans_id;
            uint64_t addr = (*j).mem_access_addr;
            
            if (trans_id == 0)
            {
                memory_completion_queue[i].erase(j);
                if (memory_completion_queue[i].size() == 0)
                    break;
                continue;
            }
            if (trans_id > 1e10)
            {
                RDMA_request_queue[node_id][trans_id].pkt_count+=1;
                if(RDMA_request_queue[node_id][trans_id].pkt_count==RDMA_packet_segments)
                {
                    RDMA_request_queue[node_id][trans_id].complete_cycle=common_clock;
                    RDMA_request_complete(trans_id,node_id);
                }
                memory_completion_queue[i].erase(j);
                if (memory_completion_queue[i].size() == 0)
                    break;
                continue;
            }
            for (auto it = l3_miss_buffer[i].begin(); it != l3_miss_buffer[i].end(); it++)
            {

                if ((*it).mem_trans_id == trans_id)
                {
                    int core_id = (*it).core_id;
                    if (core_mem_complete[core_id])
                    {
                        break;
                    }
                    else
                    {
                        total_mem_cost[node_id] = total_mem_cost[node_id] + common_clock - (*it).memory_start_cycle;
                        total_mem_access[node_id]++;
                        (*it).complete_cycle = common_clock;
                        memory_completion_queue[i].erase(j);
                        core_mem_complete[core_id] = 1;
                        break;
                    }
                }
            }
        }

        if (memory_completion_queue[i].size() == 0 || j == memory_completion_queue[i].end())
        {
            break;
        }
    }
}

bool page_table_walk(pgd &_pgd, uint64_t vaddr, uint64_t &paddr)
{
    pud *_pud;
    pmd *_pmd;
    pte *_pte;
    page *_page;

    unsigned long pgd_vaddr = 0L, pud_vaddr = 0L, pmd_vaddr = 0L, pte_vaddr = 0L, page_offset_addr = 0L;
    split_vaddr(pgd_vaddr, pud_vaddr, pmd_vaddr, pte_vaddr, page_offset_addr, vaddr);

    _pud = _pgd.access_in_pgd(pgd_vaddr);
    if (_pud == nullptr)
    {
        return false;
    }
    else if (_pud != nullptr)
    {
        _pmd = _pud->access_in_pud(pud_vaddr);
        if (_pmd == nullptr)
        {
            return false;
        }
        else if (_pmd != nullptr)
        {
            _pte = _pmd->access_in_pmd(pmd_vaddr);
            if (_pte == nullptr)
            {
                return false;
            }
            else if (_pte != nullptr)
            {
                _page = _pte->access_in_pte(pte_vaddr);
                if (_page == nullptr)
                {
                    return false;
                }
                else if (_page != nullptr)
                {
                    paddr = _page->get_page_base_addr(pte_vaddr);
                    // paddr=paddr<<12;
                    // paddr=paddr + page_offset_addr;
                    return true;
                }
            }
        }
    }
}

uint64_t page_counter[num_nodes] = {0};

void handle_page_fault(pgd &_pgd, int node_no, uint64_t vaddr, uint64_t &paddr, local_addr_space &L, bool &new_remote_chunk_allocated)
{
    pud *_pud;
    pmd *_pmd;
    pte *_pte;
    page *_page;

    unsigned long pgd_vaddr = 0L, pud_vaddr = 0L, pmd_vaddr = 0L, pte_vaddr = 0L, page_offset_addr = 0L;
    split_vaddr(pgd_vaddr, pud_vaddr, pmd_vaddr, pte_vaddr, page_offset_addr, vaddr);

    long int virt_page_addr = get_page_addr(vaddr);

    char page_local_remote = NULL; // tells that new page should be in local memory or remote

    // bool b=L.free_local();
    // cout<<"status"<<b;
    // cin.get();

    #ifdef local_remote
    if (page_counter[node_no] % 2 != 0 && L.free_local()) // follows alternate local-remote page allocation
    #endif local_remote
    {
        page_local_remote = 'L';
        page_counter[node_no]++;
    }

    #ifdef local_remote
    else if (page_counter[node_no] % 2 == 0 || !L.free_local())
    {
        page_local_remote = 'R';
        page_counter[node_no]++;
    }
    #endif local_remote

    if (page_local_remote == 'R' && !L.free_remote())
    {

        new_remote_chunk_allocated = true;
        int remote_pool = round_robin_pool_select();
        double chunk_size = 4.0;//MB
        pthread_mutex_lock(&lock_mem);
        request_remote_memory(L, R[remote_pool], chunk_size);
        pthread_mutex_unlock(&lock_mem);
    }

    _pud = _pgd.access_in_pgd(pgd_vaddr);
    if (_pud == nullptr)
    {
        _pgd.add_in_pgd(pgd_vaddr);
        _pud = _pgd.access_in_pgd(pgd_vaddr);
    }

    _pmd = _pud->access_in_pud(pud_vaddr);
    if (_pmd == nullptr)
    {
        _pud->add_in_pud(pud_vaddr);
        _pmd = _pud->access_in_pud(pud_vaddr);
    }

    _pte = _pmd->access_in_pmd(pmd_vaddr);
    if (_pte == nullptr)
    {
        _pmd->add_in_pmd(pmd_vaddr);
        _pte = _pmd->access_in_pmd(pmd_vaddr);
    }

    _page = _pte->access_in_pte(pte_vaddr);
    if (_page == nullptr)
    {
        long int pte_paddr = NULL;
        if (page_local_remote == 'R')
        {
            pte_paddr = L.allocate_remote_page();
        }
        else
        {
            pte_paddr = L.allocate_local_page(); // will request a new page from local memory
        }
        _pte->add_in_pte(pte_vaddr, pte_paddr); // new page entry at vaddr to paddr map
        _page = _pte->access_in_pte(pte_vaddr);
    }

    paddr = _page->get_page_base_addr(pte_vaddr);
    uint64_t temp_vaddr = vaddr>>12;
    if(page_local_remote == 'L')
    {
        page_fault_entry[node_no][temp_vaddr] = {paddr,common_clock+page_fault_latency};
    }
    else
    {
         page_fault_entry[node_no][temp_vaddr] = {paddr,common_clock+ chunk_allocation_plus_page_fault_latency};
    }
    // paddr=paddr<<12;
    // paddr=paddr + page_offset_addr;
}
