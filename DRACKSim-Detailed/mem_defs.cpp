#include <bitset>
#include "MMU.cpp"
#include "mmap.cpp"
#include "DRAMSim2/DRAMSim.h"
#include <algorithm>
#include <string>
#include <queue>
#include <sys/stat.h>
#include <unistd.h>

ofstream track;

static int epoch_num = 0;

#define num_nodes 1
#define num_mem_pools 1
#define core_count 4

#define local_mem_size 1
#define remote_mem_size 32

// hardware DRAM units of same size in MBs (used by DRAMSim2)
#define local_DRAM_size (local_mem_size * pow(2.0, 10.0))
#define remote_DRAM_size (remote_mem_size * pow(2.0, 10.0))

pthread_mutex_t lock_update;
pthread_mutex_t lock_queue;
pthread_mutex_t lock_mem;
pthread_mutex_t lock;

// One page-table for each process
pgd _pgd[num_nodes]; // a max of 1-process assumed per node, should be increased
					 // when more processes are simulated

// instruction object structure
struct INST
{
	int proc_id;
	uint64_t ins_id;
	int64_t addr; // ns_addr
	int16_t threadid;
	int64_t read_opr;
	int16_t read_size;
	int64_t read_opr2;
	int16_t read_size2;
	int64_t write_opr;
	int16_t write_size;
	int32_t RR[4];
	int32_t WR[4];
	bool is_Branch;
	bool branch_taken;
	bool branch_miss_predicted;
	int16_t ins_type;
};

struct remote_memory_access
{
	uint64_t mem_access_addr;
	int source;
	int dest;
	uint64_t cycle;
	uint64_t miss_cycle_num;
	uint64_t memory_access_completion_cycle;
	uint64_t trans_id;
	bool isWrite;
	bool isRDMA;
	int RDMA_segment_id=0;
};

// memory request/response to node/pool from pool/node as a network packet
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

using namespace DRAMSim;

// used to store average local memory access cycle time for different nodes
long unsigned total_core_local_time[num_nodes][core_count]={0};
long unsigned total_core_local_count[num_nodes][core_count]={0};
// used to store average remote memory access cycle time for different nodes
long unsigned total_core_remote_time[num_nodes][core_count]={0};
long unsigned total_core_remote_count[num_nodes][core_count]={0};
// used to store average remote memory access for different pools
long unsigned total_remote_pool_time[num_mem_pools]={0};
long unsigned total_remote_pool_count[num_mem_pools]={0};


// total number of memory accesses at each memory unit
unsigned long _total_access_count[num_mem_pools];

// address spaces for different type of memory units
local_addr_space L[num_nodes];
remote_addr_space R[num_mem_pools];

// total number of completed memory accesses at each memory unit
uint64_t completed_trans_local[num_nodes];
uint64_t completed_trans_node_to_remote[num_nodes];
uint64_t completed_trans_remote[num_mem_pools];
uint64_t local_writebacks[num_nodes]={0};

// total cycle count for all memory accesses at each memory unit
uint64_t total_node_local[num_nodes];
uint64_t total_remote_pool[num_mem_pools];
uint64_t total_node_remote[num_nodes];

// used to store core id of a memory request 
unordered_map<int,int> core_map[num_nodes];

uint64_t total = 0;
uint64_t avg = 0;
uint64_t completed = 0;

class some_object
{
public:
	void read_complete(int, uint64_t, uint64_t, uint64_t, int, unsigned, uint64_t, uint64_t);
	void write_complete(int, uint64_t, uint64_t, uint64_t, int, unsigned, uint64_t, uint64_t);
	int add_one_and_run(DRAMSim::MultiChannelMemorySystem *mem, uint64_t addr, bool isWrite, uint64_t tid, uint64_t start_cycle, uint64_t miss_cycle, int nid);
};

// used to store the total cycle number used at each memory unit
uint64_t total_cycle[num_nodes + num_mem_pools];

// used for just printing "simulation going on" message rather than memory access complete message
uint64_t x = 0;

// to maintain count of memory accesses with different access time on different memory pools
uint64_t U_100ns[num_mem_pools];
uint64_t B_100_300ns[num_mem_pools];
uint64_t B_300_500ns[num_mem_pools];
uint64_t B_500_700ns[num_mem_pools];
uint64_t B_700_1000ns[num_mem_pools];
uint64_t G_1000ns[num_mem_pools];

void to_trans_layer(remote_memory_access, deque<packet> *);
void add_local_access_time(remote_memory_access);
void add_remote_access_time(remote_memory_access);
deque<packet> rx_packet_queue_pool[num_mem_pools]; // receiving queue

vector<remote_memory_access> memory_completion_queue[num_nodes];
map<uint64_t,int>RDMA_response_queue[num_nodes];
int RDMA_PKT_SIZE=64;	//(bytes)
uint64_t RDMA_packet_segments =0;
//pagesize/RDMA_pkt_size

// function to update end-cycle of each memory request once a transaction is completed
void update(int node_id, int mem_id, uint64_t cycle, uint64_t tid, uint64_t start_cycle, uint64_t miss_cycle, uint64_t address, bool write)
{
	//	uint64_t size=0;
	int i = mem_id - 1;
	total_cycle[i] = cycle;
	// It is an RDMA request
	if(tid>1e8)
	{
		address = address&(0xfffffffff000);
		RDMA_response_queue[node_id][address]++;
		if(RDMA_response_queue[node_id][address]%((int)ceil((double)64/(double)RDMA_packet_segments))==0 || RDMA_response_queue[node_id][address]==64)
		{
			int remote_pool = i - num_nodes;
			remote_memory_access mem_response;
			mem_response.source = remote_pool;
			mem_response.dest = node_id;
			mem_response.cycle = common_clock;
			mem_response.memory_access_completion_cycle = common_clock;
			mem_response.miss_cycle_num = miss_cycle;
			mem_response.trans_id = tid;
			mem_response.mem_access_addr=address;
			mem_response.RDMA_segment_id = RDMA_response_queue[node_id][address];
			if(RDMA_response_queue[node_id][address]==64)
			{
				RDMA_response_queue[node_id].erase(address);
			}
			if(tid!=0)
				to_trans_layer(mem_response, rx_packet_queue_pool);
		}
	}
	//cache-line memory request
	else if (tid<1e8 && i < num_nodes)
	{
		// calcuates memory transaction time and total cycles till now at memory unit with-in local node
		remote_memory_access mem_response;
		mem_response.source = node_id;
		mem_response.dest = node_id;
		mem_response.cycle = common_clock;
		mem_response.memory_access_completion_cycle = common_clock;
		mem_response.miss_cycle_num = miss_cycle;
		mem_response.trans_id = tid;
		mem_response.mem_access_addr=address;

		completed_trans_local[i]++;
		total_node_local[i] = total_node_local[i] + (common_clock - start_cycle);
		add_local_access_time(mem_response);
		if(tid!=0)
		{
			memory_completion_queue[i].push_back(mem_response);
		}
		else
		{
			local_writebacks[i]++;
		}
	}
	else if (tid<1e8 && i >= num_nodes && i < (num_nodes + num_mem_pools))
	{
		int remote_pool = i - num_nodes;
		remote_memory_access mem_response;
		mem_response.source = remote_pool;
		mem_response.dest = node_id;
		mem_response.cycle = common_clock;
		mem_response.memory_access_completion_cycle = common_clock;
		mem_response.miss_cycle_num = miss_cycle;
		mem_response.trans_id = tid;
		mem_response.mem_access_addr=address;
		if(tid!=0)
		{
			to_trans_layer(mem_response, rx_packet_queue_pool);
		}

		// calcuates memory transaction time and total cycles till now at memory unit at remote pool
		completed_trans_node_to_remote[node_id]++;
		total_node_remote[node_id]=total_node_remote[node_id]+(common_clock - start_cycle);

		completed_trans_remote[remote_pool]++;
		total_remote_pool[remote_pool] = total_remote_pool[remote_pool] + (common_clock - start_cycle);

		// increamenting the count of memory access with particular cycle number per memory pool
		int diff = cycle - start_cycle;
		if (diff < 100)
		{
			U_100ns[i - num_nodes]++;
		}
		else if (diff > 100 && diff < 300)
		{
			B_100_300ns[i - num_nodes]++;
		}
		else if (diff > 300 && diff < 500)
		{
			B_300_500ns[i - num_nodes]++;
		}
		else if (diff > 500 && diff < 700)
		{
			B_500_700ns[i - num_nodes]++;
		}
		else if (diff > 700 && diff < 1000)
		{
			B_700_1000ns[i - num_nodes]++;
		}
		else if (diff > 1000)
		{
			G_1000ns[i - num_nodes]++;
		}

		// track<<"\nmem-id:"<<(i-num_nodes)<<"  tid:"<<tid<<"  sc:"<<start_cycle<<"  end-c"<<cycle<<"  diff:"<<(cycle - start_cycle)<<"  pending_count"<<_pending_count[i-num_nodes];

		// updates end-cycle for memory units at remote pool, with the knowldege of node-no whose
		// transaction this pool is serving for. (this stat is managed in array node_to_remote_memory_cycle_time)

		// remote-local-map is used to know the node-id and trans-id of a node for which
		// this remote pool had completed this memory request

		// total_node_remote[node_id]=total_node_remote[node_id]+(cycle - miss_cycle) + rx_dis_latency; //reverse dis_latency
		// completed_trans_node_to_remote[node_id]++;
	}
}

/* callback functors */
void some_object::read_complete(int nid, uint64_t tid, uint64_t start_cycle, uint64_t miss_cycle, int mem_id, unsigned id, uint64_t address, uint64_t clock_cycle)
{
	x++;
	if (x % 10000 == 0)
		cout << ".";
	if (x % 100000 == 0)
		cout << "\nSimulation Going On";
	pthread_mutex_lock(&lock_update);
	bool write=0;
	update(nid, mem_id, clock_cycle, tid, start_cycle, miss_cycle,address,write);
	pthread_mutex_unlock(&lock_update);
	// printf("Mem_ID: %d [Callback] read complete: %d 0x%lx cycle=%lu\n", mem_id, id, address, clock_cycle);
}

void some_object::write_complete(int nid, uint64_t tid, uint64_t start_cycle, uint64_t miss_cycle, int mem_id, unsigned id, uint64_t address, uint64_t clock_cycle)
{
	x++;
	if (x % 10000 == 0)
		cout << ".";
	if (x % 100000 == 0)
		cout << "\nSimulation Going On";
	pthread_mutex_lock(&lock_update);
	bool write=1;
	update(nid, mem_id, clock_cycle, tid, start_cycle, miss_cycle, address,write);
	pthread_mutex_unlock(&lock_update);
	// printf("Mem_ID: %d [Callback] write complete: %d 0x%lx cycle=%lu\n", mem_id, id, address, clock_cycle);
}

/* FIXME: this may be broken, currently */
void power_callback(double a, double b, double c, double d)
{
	// printf("power callback: %0.3f, %0.3f, %0.3f, %0.3f\n",a,b,c,d);
}

int some_object::add_one_and_run(MultiChannelMemorySystem *mem, uint64_t addr, bool isWrite, uint64_t tid, uint64_t start_cycle, uint64_t miss_cycle, int nid)
{
	/* create a transaction and add it */
	mem->addTransaction(isWrite, addr, nid, tid, start_cycle, miss_cycle);
	return 0;
}

// used to store total number of accesses in local nodes and remote pools
unsigned long long local_access[num_nodes];
unsigned long long remote_access[num_nodes];
unsigned long long count_access[num_nodes];

// used to store each nodes total memory access count at each remote pool
unsigned long per_pool_access_count[num_nodes][num_mem_pools];

some_object obj;

TransactionCompleteCB *read_cb = new Callback<some_object, void, int, uint64_t, uint64_t, uint64_t, int, unsigned, uint64_t, uint64_t>(&obj, &some_object::read_complete);
TransactionCompleteCB *write_cb = new Callback<some_object, void, int, uint64_t, uint64_t, uint64_t, int, unsigned, uint64_t, uint64_t>(&obj, &some_object::write_complete);

/* Declare DRAMs memory to simulate */
MultiChannelMemorySystem *local_mem[num_nodes];
MultiChannelMemorySystem *remote_mem[num_mem_pools];


//std::ofstream ResultsFile;

void declare_memory_variables(string dir)
{
	// initializing stat variables

	// node wise number of local, remote and total memory access
	for (int i = 0; i < num_nodes; i++)
	{
		local_access[i] = 0;
		count_access[i] = 0;
		remote_access[i] = 0;

		total_node_local[i] = 0;
		
		completed_trans_local[i] = 0;

		total_node_remote[i] = 0;
		
		completed_trans_node_to_remote[i] = 0;
	}

	for (int i = 0; i < num_mem_pools; i++)
	{
		total_remote_pool[i] = 0;
	
		completed_trans_remote[i] = 0;
		_total_access_count[i] = 0;

		U_100ns[i] = 0;
		B_100_300ns[i] = 0;
		B_300_500ns[i] = 0;
		B_500_700ns[i] = 0;
		B_700_1000ns[i] = 0;
		G_1000ns[i] = 0;
	}

	for (int i = 0; i < num_nodes; i++)
	{
		for (int j = 0; j < num_mem_pools; j++)
		{
			per_pool_access_count[i][j] = 0;
		}
	}

	for (int i = 0; i < (num_nodes + num_mem_pools); i++)
		total_cycle[i] = 0;

	for (int i = 0; i < num_nodes; i++) // add local memory at each ndoe
		L[i].add_local_memory(local_mem_size, i);

	for (int i = 0; i < num_mem_pools; i++) // add remote memory pools
		R[i].add_remote_memory_pool(remote_mem_size, i);

	for (int i = 0; i < num_nodes; i++)
	{
		local_mem[i] = getMemorySystemInstance(i + 1, "ini/DDR4_x16_2400.ini", "system.ini", "./DRAMSim2", "abc", local_DRAM_size);
		local_mem[i]->RegisterCallbacks(read_cb, write_cb, power_callback); // DRAM simulator function

		//		local_mem[i]->setCPUClockSpeed(3601440555);
	}

	for (int i = 0; i < num_mem_pools; i++)
	{
		remote_mem[i] = getMemorySystemInstance(i + num_nodes + 1, "ini/DDR4_x16_2400.ini", "system.ini", "./DRAMSim2", "abc", remote_DRAM_size);
		remote_mem[i]->RegisterCallbacks(read_cb, write_cb, power_callback); // DRAM simulator function

		//		remote_mem[i]->setCPUClockSpeed(3601440555);
	}

	string inv, tra, ou, mem;
	inv = dir + "/memory_access_completion_log.trc";
	tra = dir + "/Extra_stats.log";
	ou = dir + "/pool_select_trace.trc";
	mem = dir + "/mem_stats";



	cout << inv << endl;
	cout << tra << endl;
	cout << ou << endl;
	cout << mem << endl;

	const char *dirr = dir.c_str();

	mkdir(dirr, 0777);

	const char *inv1 = inv.c_str();
	invalid.open(inv1);
	invalid << "\nLog Turned Off, Un-comment log statements in mem_defs.h read and write complete functions to turn it on";
	const char *tra1 = tra.c_str();
	track.open(tra1);
	const char *ou1 = ou.c_str();
	out.open(ou1);
	const char *mem1 = mem.c_str();
	mem_stats.open(mem1);
}

// stats for local access
void add_local_access_time(remote_memory_access mem_response)
{
   int node_id = mem_response.source;
   long unsigned trans_id = mem_response.trans_id;

   int core_id = core_map[node_id][trans_id];
   core_map[node_id].erase(trans_id);

   int time_taken = mem_response.memory_access_completion_cycle - mem_response.miss_cycle_num;
   total_core_local_time[node_id][core_id]+=time_taken;
   total_core_local_count[node_id][core_id]++;
}

// stats for remote access
void add_remote_access_time(remote_memory_access mem_response)
{
   int node_id = mem_response.dest;
   unsigned long trans_id = mem_response.trans_id;
   int core_id = core_map[node_id][trans_id];
   core_map[node_id].erase(trans_id);

   int time_taken = mem_response.cycle - mem_response.miss_cycle_num;
   total_core_remote_time[node_id][core_id]+=time_taken;
   total_core_remote_count[node_id][core_id]++;
   

}
