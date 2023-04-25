#include"MMU.cpp"
#include"mmap.cpp"
#include"DRAMSim2/DRAMSim.h"
#include<algorithm>
#include<string>
#include<queue>
#include<sys/stat.h>
#include<unistd.h>

ofstream track;

#define num_mem_pools	1
#define num_nodes		4

//in GBs, to declare memory address space 
#define local_mem_size	4
#define remote_mem_size 4

//in MBs, to declare DRAM instances of same size as address space
#define local_DRAM_size	(local_mem_size * pow(2.0,10.0))
#define remote_DRAM_size (remote_mem_size * pow(2.0,10.0))

pthread_mutex_t lock_queue;
pthread_mutex_t lock_update;

using namespace DRAMSim;

#define max_num_process_per_node 5
#define RDMA_PKT_SIZE 1024
//network-stats

//packets sent from node to memory pool
long double total_packets_per_node[num_nodes]={0};
long double overall_total_packets=0;
long double rx_overall_total_packets=0;

//average network time for memory requests of each node from node to memory pool
long double total_network_delay[num_nodes]={0};


//received back at node from memory pool
long double rx_total_packets_per_node[num_nodes]={0};

//average network time for memory requests of each node when from memory pool to node
long double rx_total_network_delay[num_nodes]={0};

//average network time for memory requests of each node to memory pool to back to node
long double mem_access_avg_network_delay[num_nodes]={0};

//overall-average network delay of all the packets node-to-pool
long double overall_total_network_delay=0;
long double overall_avg_network_delay=0;

//overall-average network delay of all the packets pool-to-node
long double rx_overall_total_network_delay=0;
long double rx_overall_avg_network_delay=0;

//overall-average network delay of all the packets node-pool-to-node
long double request_overall_total_network_delay=0;
long double request_overall_avg_network_delay=0;

//memory-stats

//used to store average local memory access cycle time for different nodes
long double avg_node_local[num_nodes]={0};
long double avg_node_local_read[num_nodes]={0};
long double avg_node_local_write[num_nodes]={0};
//used to store average remote memory access cycle time for different nodes
long double avg_node_remote[num_nodes]={0};
long double avg_node_remote_read[num_nodes]={0};
long double avg_node_remote_write[num_nodes]={0};
//used to store total average memory access (local/remote) cycle time for different nodes
long double avg_node_overall[num_nodes]={0};
long double avg_node_overall_read[num_nodes]={0};
long double avg_node_overall_write[num_nodes]={0};
//used to store average memory access cycle time for different remote memory pools
long double avg_remote_pool[num_mem_pools]={0};
long double avg_remote_pool_read[num_mem_pools]={0};
long double avg_remote_pool_write[num_mem_pools]={0};

//total number of memory accesses at each memory unit 
unsigned long _total_access_count[num_mem_pools]={0};

//address spaces for different type of memory units
local_addr_space L[num_nodes];
remote_addr_space R[num_mem_pools];

//total number of completed memory accesses at each memory unit 
uint64_t completed_trans_local[num_nodes]={0};
uint64_t completed_trans_local_read[num_nodes]={0};
uint64_t completed_trans_local_write[num_nodes]={0};
uint64_t completed_trans_node_to_remote[num_nodes]={0};
uint64_t completed_trans_node_to_remote_read[num_nodes]={0};
uint64_t completed_trans_node_to_remote_write[num_nodes]={0};
uint64_t completed_trans_remote[num_mem_pools]={0};
uint64_t completed_trans_remote_read[num_mem_pools]={0};
uint64_t completed_trans_remote_write[num_mem_pools]={0};

//total cycle count for all memory accesses at each memory unit
uint64_t total_node_local[num_nodes]={0};
uint64_t total_node_local_read[num_nodes]={0};
uint64_t total_node_local_write[num_nodes]={0};
uint64_t total_remote_pool[num_mem_pools]={0};
uint64_t total_remote_pool_read[num_mem_pools]={0};
uint64_t total_remote_pool_write[num_mem_pools]={0};
uint64_t total_node_remote[num_nodes]={0};
uint64_t total_node_remote_read[num_nodes]={0};
uint64_t total_node_remote_write[num_nodes]={0};

uint64_t memory_cycle_per_pool[num_mem_pools]={0};


class some_object
{
	public: 
		void read_complete(int, uint64_t, uint64_t, uint64_t, int, unsigned, uint64_t, uint64_t);
		void write_complete(int, uint64_t, uint64_t, uint64_t, int, unsigned, uint64_t, uint64_t);
		int add_one_and_run(DRAMSim::MultiChannelMemorySystem *mem, uint64_t addr, bool isWrite, uint64_t tid, uint64_t start_cycle, uint64_t miss_cycle, int nid);
};

//used to store the total cycle number used at each memory unit
uint64_t total_cycle[num_nodes+num_mem_pools]={0};

//to maintain count of memory accesses with different access time on different memory pools
uint64_t U_100ns[num_mem_pools]={0};
uint64_t B_100_300ns[num_mem_pools]={0};
uint64_t B_300_500ns[num_mem_pools]={0};
uint64_t B_500_700ns[num_mem_pools]={0};
uint64_t B_700_1000ns[num_mem_pools]={0};
uint64_t G_1000ns[num_mem_pools]={0};


//trace read format
struct Trace
{
	int procid;
    int threadid;
    unsigned long long addr;
    char r;
    unsigned long long cycle;
};

//memory request/response
struct remote_memory_access
{
	int source;
    int dest;
    uint64_t mem_access_addr;
    int64_t cycle;
    int64_t miss_cycle_num;
    int64_t memory_access_completion_cycle;
    uint64_t trans_id;
	bool isWrite;
	bool isRDMA;
	int RDMA_segment_id=0;
};


//memory request/response to node/pool from pool/node as a network packet
struct packet
{
	remote_memory_access mem;
	int is_transmitting;
	int is_processing;
	int64_t in_nic_source;
	int64_t out_nic_source;
	int64_t in_switch_input_port;
	int64_t out_switch_input_port;
	int64_t in_switch_output_port;
	int64_t out_switch_output_port;
	int64_t in_nic_dest;
	int64_t out_nic_dest;
};

deque <packet> rx_packet_queue_pool[num_mem_pools];	//receiving queue


long int packets_out=0;
void to_trans_layer(remote_memory_access, deque <packet>*);
//function to update end-cycle of each memory request once a transaction is completed
//also calculate memory stats
void update(int node_id, int mem_id, uint64_t cycle, uint64_t tid, uint64_t start_cycle, uint64_t miss_cycle, char S)
{
//	uint64_t size=0;
	int i=mem_id-1;

	total_cycle[i]=cycle;
	if(i<num_nodes)
	{
		completed_trans_local[i]++;
		total_node_local[i]=total_node_local[i]+(common_clock - miss_cycle);
		
		if(S=='r')
		{
			completed_trans_local_read[i]++;
			total_node_local_read[i]=total_node_local_read[i]+(common_clock - miss_cycle);
		}
		else if(S=='w')
		{
			completed_trans_local_write[i]++;
			total_node_local_write[i]=total_node_local_write[i]+(common_clock - miss_cycle);
		}

	}
	if(i>=num_nodes && i<(num_nodes+num_mem_pools))
	{
		int remote_pool=i-num_nodes;
		remote_memory_access mem_response;
		mem_response.miss_cycle_num=miss_cycle;
		mem_response.source=remote_pool;
		mem_response.dest=node_id;
		mem_response.trans_id=tid;
		mem_response.cycle=common_clock;
		mem_response.memory_access_completion_cycle=common_clock;

		if(S=='w')
			mem_response.isWrite=true;

		if(S=='r')
			mem_response.isWrite=false;
		packets_out++;
		to_trans_layer(mem_response,rx_packet_queue_pool);

		//calcuates memory transaction time and total cycles till now at memory unit at remote pool
		completed_trans_remote[i-num_nodes]++;
		total_remote_pool[i-num_nodes]=total_remote_pool[i-num_nodes]+(common_clock - start_cycle);

		if(S=='r')
		{
			completed_trans_remote_read[i-num_nodes]++;
			total_remote_pool_read[i-num_nodes]=total_remote_pool_read[i-num_nodes]+(common_clock - start_cycle);
		}
		else if(S=='w')
		{
			completed_trans_remote_write[i-num_nodes]++;
			total_remote_pool_write[i-num_nodes]=total_remote_pool_write[i-num_nodes]+(common_clock - start_cycle);
		}
		

	//increamenting the count of memory access with particular cycle number per memory pool 
		int diff=common_clock - start_cycle;
		if(diff<100)
		{
			U_100ns[i-num_nodes]++;
		}
		else if(diff>100 && diff<300)
		{
			B_100_300ns[i-num_nodes]++;
		}
		else if(diff>300 && diff<500)
		{
			B_300_500ns[i-num_nodes]++;
		}
		else if(diff>500 && diff<700)
		{
			B_500_700ns[i-num_nodes]++;
		}
		else if(diff>700 && diff<1000)
		{
			B_700_1000ns[i-num_nodes]++;
		}
		else if(diff>1000)
		{
			G_1000ns[i-num_nodes]++;
		}
	}
}

/* callback functors */
void some_object::read_complete(int nid, uint64_t tid, uint64_t start_cycle, uint64_t miss_cycle, int mem_id, unsigned id, uint64_t address, uint64_t clock_cycle)
{
	if(tid%10000==0 && tid>10)
		cout<<".";
	if(tid%100000==0 && tid>10)
		cout<<"\nMemory Simulation Going On";
	pthread_mutex_lock(&lock_update);
	update(nid,mem_id,clock_cycle,tid,start_cycle,miss_cycle,'r');
	pthread_mutex_unlock(&lock_update);
	//printf("Mem_ID: %d [Callback] read complete: %d 0x%lx cycle=%lu\n", mem_id, id, address, clock_cycle);
}

void some_object::write_complete(int nid, uint64_t tid, uint64_t start_cycle, uint64_t miss_cycle, int mem_id, unsigned id, uint64_t address, uint64_t clock_cycle)
{
	if(tid%10000==0 && tid>10)
		cout<<".";
	if(tid%100000==0 && tid>10)
		cout<<"\nMemory Simulation Going On";
	pthread_mutex_lock(&lock_update);
	update(nid,mem_id,clock_cycle,tid,start_cycle,miss_cycle,'w');
	pthread_mutex_unlock(&lock_update);
	//printf("Mem_ID: %d [Callback] write complete: %d 0x%lx cycle=%lu\n", mem_id, id, address, clock_cycle);
}

/* FIXME: this may be broken, currently */
void power_callback(double a, double b, double c, double d)
{
	//printf("power callback: %0.3f, %0.3f, %0.3f, %0.3f\n",a,b,c,d);
}

int some_object::add_one_and_run(MultiChannelMemorySystem *mem, uint64_t addr, bool isWrite, uint64_t tid, uint64_t start_cycle, uint64_t miss_cycle, int nid)
{
	/* create a transaction and add it */
	mem->addTransaction(isWrite, addr, nid, tid, start_cycle,miss_cycle);
	return 0;
}
						//One page-table for each process
pgd _pgd[num_nodes*max_num_process_per_node];	//a max of 3-processes assumed per node, should be increased 
						//when more processes are simulated

//used to store total number of accesses in local nodes and remote pools
unsigned long long local_access[num_nodes]={0};
unsigned long long remote_access[num_nodes]={0};
unsigned long long count_access[num_nodes]={0};

//used to store each nodes total memory access count at each remote pool
unsigned long per_pool_access_count[num_nodes][num_mem_pools]={0};

some_object obj;

TransactionCompleteCB *read_cb= new Callback<some_object, void, int, uint64_t, uint64_t, uint64_t, int, unsigned, uint64_t, uint64_t>(&obj, &some_object::read_complete);
TransactionCompleteCB *write_cb= new Callback<some_object, void, int, uint64_t, uint64_t, uint64_t, int, unsigned, uint64_t, uint64_t>(&obj, &some_object::write_complete);

/* Declare DRAMs memory to simulate */
MultiChannelMemorySystem *local_mem[num_nodes];
MultiChannelMemorySystem *remote_mem[num_mem_pools];

void declare_memory_variables(string dir)
{
 	for(int i=0;i<num_nodes;i++)	//add local memory at each ndoe
 		L[i].add_local_memory(local_mem_size,i);//1,i);//0.0000038147,i);//(0.03125,i);

 	for(int i=0;i<num_mem_pools;i++)	//add remote memory pools
		R[i].add_remote_memory_pool(remote_mem_size,i);

	for(int i=0;i<num_nodes;i++)
	{
		local_mem[i]= getMemorySystemInstance(i+1, "ini/DDR4_x16_2400_1.ini", "system.ini", "./DRAMSim2", "abc", local_DRAM_size);
		local_mem[i]->RegisterCallbacks(read_cb,write_cb,power_callback);	//DRAM simulator function

//		local_mem[i]->setCPUClockSpeed(3601440555);
	}	

	for(int i=0;i<num_mem_pools;i++)
	{
		remote_mem[i]= getMemorySystemInstance(i+num_nodes+1, "ini/DDR4_x16_2400.ini", "system.ini", "./DRAMSim2", "abc", remote_DRAM_size);
		remote_mem[i]->RegisterCallbacks(read_cb,write_cb,power_callback);	//DRAM simulator function
		
//		remote_mem[i]->setCPUClockSpeed(3601440555);
	}

	string inv,tra,ou,mem,net;
	inv=dir+"/memory_access_completion_log.trc";
	tra=dir+"/Extra_stats.log";
	ou=dir+"/pool_select_trace.trc";
	mem=dir+"/mem_stats";
	net=dir+"/net_stats";

	cout<<inv<<endl;
	cout<<tra<<endl;
	cout<<ou<<endl;
	cout<<mem<<endl;
	cout<<net<<endl;

	const char * dirr=dir.c_str();

	mkdir(dirr,0777);
	
	const char * inv1=inv.c_str();
	invalid.open(inv1);
	invalid<<"\nLog Turned Off, Un-comment log statements in mem_defs.h read and write complete functions to turn it on";
	const char * tra1=tra.c_str();
	track.open(tra1);
	const char * ou1=ou.c_str();
	out.open(ou1);
	const char * mem1=mem.c_str();
	mem_stats.open(mem1);
	const char * net1=net.c_str();
	netstats.open(net1);
}

void add_remote_access_time(remote_memory_access mem_response)
{
	int node_id = mem_response.dest;

	rx_total_packets_per_node[node_id]++;
	rx_overall_total_packets++;

	int time_taken = mem_response.cycle - mem_response.miss_cycle_num;

	long double net_resp_start_cycle = mem_response.memory_access_completion_cycle;
	long double net_resp_end_cycle = mem_response.cycle;
	long double miss_cycle = mem_response.miss_cycle_num;

	rx_total_network_delay[node_id] = rx_total_network_delay[node_id] + net_resp_end_cycle - net_resp_start_cycle;
	rx_overall_total_network_delay = rx_overall_total_network_delay + net_resp_end_cycle - net_resp_start_cycle;

	// total_network_delay[node_id] = total_network_delay[node_id] + (net_resp_end_cycle - net_resp_start_cycle);
	// overall_total_network_delay = overall_total_network_delay + (net_resp_end_cycle - net_resp_start_cycle);

	total_node_remote[node_id]=total_node_remote[node_id]+(net_resp_end_cycle - miss_cycle);
	completed_trans_node_to_remote[node_id]++;

	char isWrite = mem_response.isWrite;

	if(!isWrite)
	{
		total_node_remote_read[node_id]=total_node_remote_read[node_id]+(net_resp_end_cycle - miss_cycle);
		completed_trans_node_to_remote_read[node_id]++;	
	}
	else
	{
		total_node_remote_write[node_id]=total_node_remote_write[node_id]+(net_resp_end_cycle - miss_cycle);
		completed_trans_node_to_remote_write[node_id]++;		
	}

}

//page-table-walker
bool page_table_walk(pgd &_pgd, uint64_t vaddr, long int &paddr)
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
                    paddr=paddr<<12;
                    paddr=paddr + page_offset_addr;
                    return true;
                }
            }
        }
    }
}

uint64_t page_counter[num_nodes]={0};
//page-table handler
long int handle_page_fault(pgd &_pgd, long int vaddr, long int &mem_access_addr, local_addr_space &L, int node_no)
{
//used to check weather this page should be allocated on local or remote memory
//pages are allocated alternately on local/emote memory until local is available 

	pud *_pud;
	pmd *_pmd;
	pte *_pte;
	page *_page;

	//return -1;

	unsigned long pgd_vaddr=0L, pud_vaddr=0L, pmd_vaddr=0L, pte_vaddr=0L, page_offset_addr=0L;
	split_vaddr(pgd_vaddr, pud_vaddr, pmd_vaddr, pte_vaddr, page_offset_addr, vaddr);


//this logic is used to allocate pages between local and remote, rather than completely using local memory before,
//it can be allocated in a round-robin manner, one in local, one in remote
	
	char page_local_remote = NULL; // tells that new page should be in local memory or remote

    if (page_counter[node_no] % 2 == 0  && L.free_local()) // follows alternate local-remote page allocation
    {
        page_local_remote = 'L';
        page_counter[node_no]++;
    }
    else if (page_counter[node_no] % 2 == 1 || !L.free_local())
    {
        page_local_remote = 'R';
        page_counter[node_no]++;
    }

    if (page_local_remote == 'R' && !L.free_remote())
    {
		pthread_mutex_lock(&lock_mem);
        int remote_pool = round_robin_pool_select();
        double chunk_size = 4.0;
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

	long int paddr=_page->get_page_base_addr(pte_vaddr);

	paddr=paddr<<12;
	paddr=paddr + page_offset_addr;
	mem_access_addr = paddr;
}