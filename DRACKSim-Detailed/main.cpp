#include <iostream>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string>
#include <queue>
#include <vector>
#include <unordered_map>
#include <map>
#include <sstream>	
#include <fstream>
using namespace std;
#define local_remote 1
uint64_t common_clock = 0;
ofstream out,mem_stats,invalid,netstats;
#include "mem_defs.cpp"


//DDR4 used with frequency 1200-MHz
//CPU frequency kept at 3.6-GHz, which is 3 times the memory latenct
float CPU_Freq = 3.6;
float Mem_Freq = 1.2;//1200x2 MHz, select the configuration file in mem_defs.h 

//It is important to keep CPU frequency as exact divisor of Mem frequency, Same way it is mapped with interconenct latency (3-times)
float CPU_Freq_Times_Mem_Freq = (float)CPU_Freq/(float)Mem_Freq; 
int freq_ratio=ceil(CPU_Freq_Times_Mem_Freq);
uint64_t core_simulated_cycle[num_nodes][core_count]={0};
//results_directory
string dir;

#include "allocator.cpp"
#include "inter_connect.cpp"
#include "branch_predictor.cpp"
#include "TLB_Cache.cpp"
ofstream inst_queu_out;
#include "OOO_core.cpp"

#define simulation_time 100000000

#define Result_cycle 50000000

#define max_insts_to_simulate 50000000


void print_stats(int);

// number of instructions to be read from a instruction stream sent by pintool in single tracefile
// once these instructions are executed, pintool will create new file and simulator will read the stream again
#define max_ins 1000000

pthread_barrier_t b;


// node-level single instruction stream from PINTOOL
deque<INST> inst_stream[num_nodes];

uint64_t INS_id[num_nodes][core_count];

// convert per node instruction stream coming from Pintool to per core at each node
void Schedule_Instruction_on_cpu_cores(int node)
{
	int core = (inst_stream[node].front().threadid) % core_count;
	int combined_count = 0;
	for (int i = 0; i < core_count; i++)
	{
		combined_count += core_inst_stream[node][i].size();
	}
	while (combined_count < 10 * core_count && !inst_stream[node].empty())
	{
		combined_count++;
		inst_stream[node].front().ins_id = INS_id[node][core];
		INS_id[node][core]++;
		core_inst_stream[node][core].push(inst_stream[node].front());
		inst_stream[node].pop_front();
		core = (inst_stream[node].front().threadid) % core_count;
	}
}


int nid=0;

std::ofstream ResultsFile[num_nodes];
// keep track of core-wise total instruction fetched on each node
uint64_t num_ins_fetched[num_nodes][core_count] = {0};

// one handler for each node, it will fetch the instruction stream of a workload and will simulate an OOO CPU, including caches and memory subsytem
void *node_stream_handler(void *node)
{
	uint64_t total_num_inst_commited=0;
	//inst_queu_out.open("testing.txt");
	// Data structure for accessing instruction stream (in shared memory) of a benchmark that is produced by the Pintool
	key_t ShmKEY1, ShmKEY3;
	int ShmID1, ShmID3;

	uint64_t *num_ins;
	int *main_start;
//	int nid=1;
	long id = (long)(node);
	int nodeid = (int)id;
	std::ifstream TraceFile;
	string in;
	std::ostringstream ss, tf;
	ss << nodeid;
	int fileid = 0;
	tf << fileid;
	string f1 = "./Output/Node" + ss.str() + "/TraceFile" + tf.str() + ".trc";
	const char *file1 = f1.c_str();

	//cout<<f1<<endl;
	
	ShmKEY1 = 10 * nodeid + 1;
	ShmID1 = shmget(ShmKEY1, sizeof(int), IPC_CREAT | 0666);
	main_start = (int *)shmat(ShmID1, NULL, 0);

	ShmKEY3 = 10 * nodeid + 3;
	ShmID3 = shmget(ShmKEY3, sizeof(uint64_t), IPC_CREAT | 0666);
	num_ins = (uint64_t *)shmat(ShmID3, NULL, 0);

	int c = 0;
	nodeid = nodeid - nid;
	// fill_load_buffer(nodeid);//
	initialize_branch_predictor(nodeid);
	// simulation starts here, clock started
	// while(common_clock<=100000000)
	//with multiple nodes,use simulation time to control end of the simulation, as simultaneous execution of nodes
	//is meaningful for scalable performance evaluation
	while(total_num_inst_commited<=max_insts_to_simulate)
	//for single node, number of instructions can be also used to mark the end of simulation
	{
		pthread_barrier_wait(&b); 

		// pintool writes a max_ins number of instructions in a file and creates a new file everytime, delete old once it is read
		//(this hack is used to save disk space to avoid large instruction traces)
		// here we close previous file and open new file everytime we go for reading more instructions from the stream
		if (inst_stream[nodeid].size() < 50)
		{
			INST temp;
			while (*num_ins <= max_ins && (*main_start) != 10)
			{
				sleep(1);
			}
			if ((*num_ins) > 0)
			{
				TraceFile.clear();
				TraceFile.open(file1);
				TraceFile.seekg(0, ios::end);
				TraceFile.seekg(0, ios::beg);

				while (TraceFile.read((char *)&temp, sizeof(temp)))
				{
					inst_stream[nodeid].push_back(temp);
				}
				TraceFile.close();
				remove(file1);
				*num_ins = 0;
				fileid++;
				tf.str("");
				tf << fileid;
				f1 = "./Output/Node" + ss.str() + "/TraceFile" + tf.str() + ".trc";
				file1 = f1.c_str();
			}
		}

		// Seperating and schedule the core-wise instruction stream
		int combined_count = 0;
		bool read_ins_stream=true;
		for (int i = 0; i < core_count; i++)
		{
			if (core_inst_stream[nodeid][i].size() > 100)
				read_ins_stream=false;
		}
		if(read_ins_stream)	
			Schedule_Instruction_on_cpu_cores(nodeid); // core_inst_stream, nodeid);
		
		if(common_clock%freq_ratio==2)
			local_mem[nodeid]->update();

		if (nodeid == 0 && common_clock%freq_ratio==2)
		{
			for (int i = 0; i < num_mem_pools; i++)
			{
				remote_mem[i]->update();
			}
		}

		update_mshr(nodeid);
		
		// cout<<"abc3 "<<nodeid<<endl;
		for (int i = 0; i < core_count; i++)
		{
			itlb_search(nodeid, i); // in TLB.cpp
			dtlb_search(nodeid, i);
			update_core(nodeid, i);
		}
		
		long unsigned node_num_inst_issued=0;
		for(int j=0;j<core_count;j++)
		{
			node_num_inst_issued+=num_inst_issued[nodeid][j];
		}
				
			
		pthread_mutex_lock(&lock);

	if(common_clock%100000==0)
	{
		cout<<"\nnode "<<nodeid;
		cout<<"\nlocal: ";
		for(int j=0;j<core_count;j++)
		{
			cout<<total_core_local_count[nodeid][j]<<" ";
		}
		cout<<"\nremote: ";
		for(int j=0;j<core_count;j++)
		{
			cout<<total_core_remote_count[nodeid][j]<<" ";
		}

		static uint64_t old_rem=0,old_loc=0;

		cout<<"\nnum_locL: "<<num_local[nodeid]<<" num_rem: "<<num_remote[nodeid]<<endl;
		cout<<"\n num locaL completed: "<<completed_trans_local[nodeid]<<" num_remote_completed: "<<completed_trans_node_to_remote[nodeid]<<endl;

		cout << "\n Total issued instructions: " << node_num_inst_issued;
		cout<<"\n ";//committ ins "<<num_inst_commited[nodeid][0]<<"  "<<num_inst_commited[nodeid][1]<<"  "<<num_inst_commited[nodeid][2]<<"  "<<num_inst_commited[nodeid][3];
		for(int k=0;k<core_count;k++)
		{
			cout<<num_inst_commited[nodeid][k]<<"  ";	
		}
			cout<<"\ncycle "<<common_clock;
		
	}
		pthread_mutex_unlock(&lock);

		pthread_barrier_wait(&b);

		if(nodeid==0)
		{
			simulate_network();
			simulate_network_reverse();
		}

		pthread_barrier_wait(&b);

		if (nodeid == 0)
		{
			if (common_clock % Result_cycle == 0 && common_clock>0)
			{
				for(int i=0;i<num_nodes;i++)
				{
					print_stats(i);
				}	
			}

			common_clock++;
		}

		static int i=0;
		combined_count = 0;
		for (int i = 0; i < core_count; i++)
		{
			combined_count += core_inst_stream[nodeid][i].size();
			if(inst_queue[nodeid][i].size())// || !reorder_buffer[nodeid][i].is_empty())
				core_simulated_cycle[nodeid][i]++;
		}
		if(combined_count==0)
		{
			i++;
			if(i==2000)
			{
				cout<<"ooo";
				break;
			}
		}
		else if(combined_count>0)
		{
			i=0;
		}

		total_num_inst_commited=0;
		for(int j=0;j<core_count;j++)
		{	
			total_num_inst_commited+=num_inst_commited[nodeid][j];
		}
	}
	
	shmctl(ShmID1, IPC_RMID, NULL);
	shmctl(ShmID3, IPC_RMID, NULL);
	shmdt(main_start);
	shmdt(num_ins);

	pthread_exit(NULL);
}

void print_mem_stats(int);

void print_stats(int node_id)
{
	std::ostringstream rf;
	rf << node_id;

	double IPCC[core_count]={0};
	string result = dir + "/node_"+rf.str()+"result.txt";
	const char *result1=result.c_str();
	ResultsFile[node_id].open(result1, ios::app);
	ResultsFile[node_id]<<fixed;
	long unsigned node_local_access_time = 0,node_local_access_count=0;
	long unsigned node_remote_access_time=0,node_remote_access_count=0;
	uint64_t total_ld_str = 0,total_num_ld_str_total=0,total_num_ld_str_completed=0;
	ResultsFile[node_id] << "\n=======================================Epoch_Number-" << (common_clock/Result_cycle) <<"=======================================";
	ResultsFile[node_id] << "\n\t\t\t\t\t\t\t-------------Node-ID- " << node_id<<"-------------";
	unsigned long total_num_inst_issued=0,total_num_inst_exec=0,total_num_inst_commited=0;
	ResultsFile[node_id] << "\n\n\t\t\t\t\t\t\t-------------Core Wise Stats-------------\n";
	ResultsFile[node_id] << "\n--------core-ID--------\t\t\t" ;
	for(int j=0;j<core_count;j++)
	{   	
		ResultsFile[node_id] << j<<"\t|\t";
	}
	ResultsFile[node_id] << "\nTotal Issued instructions:\t    ";
	for(int j=0;j<core_count;j++)
		ResultsFile[node_id] << num_inst_issued[node_id][j] <<"\t|    ";
	ResultsFile[node_id] << "\nTotal executed instructions:\t    ";
	for(int j=0;j<core_count;j++)
		ResultsFile[node_id]  << num_inst_exec[node_id][j]<<"\t|    ";
	ResultsFile[node_id] << "\nTotal committed instructions:\t    " ;
	for(int j=0;j<core_count;j++)
		ResultsFile[node_id] << num_inst_commited[node_id][j]<<"\t|    ";
		ResultsFile[node_id] << "\nTotal Branch instructions:\t    " ;
	for(int j=0;j<core_count;j++)
		ResultsFile[node_id] << num_branches[node_id][j]<<"\t|    ";
		ResultsFile[node_id] << "\nTotal branch-miss instructions:\t    " ;
	for(int j=0;j<core_count;j++)
		ResultsFile[node_id] << num_branch_misprediction[node_id][j]<<"\t|    ";
	ResultsFile[node_id] << "\nSimulated Cycles on core:\t    " ;
	for(int j=0;j<core_count;j++)
	{
		// if(j==0)
		// 	ResultsFile[node_id] << common_clock<<"\t|    ";
		// else
			ResultsFile[node_id] << core_simulated_cycle[node_id][j]<<"\t|    ";
	}

	for(int j=0;j<core_count;j++)
	{	
		total_num_inst_issued+=num_inst_issued[node_id][j];
		total_num_inst_exec+=num_inst_exec[node_id][j];
		total_num_inst_commited+=num_inst_commited[node_id][j];
		total_ld_str = total_ld_str + ld_str_id[node_id][j];
	}
	// ResultsFile[node_id] << "\n Total Load-store sent to memory:(include redundant loads to store address) \n\t\t\t\t\t";
	// for(int j=0;j<core_count;j++)
	// 	ResultsFile[node_id] <<num_ld_str_total[node_id][j]<<"\t|\t";
	
	ResultsFile[node_id] << "\nTotal Load-stores by CPU:   \t    "; 
	for(int j=0;j<core_count;j++)
		ResultsFile[node_id] << num_ld_str_total[node_id][j]<<"\t|    ";

	ResultsFile[node_id] << "\nNon-redundant Load-stores : \t    "; 
	for(int j=0;j<core_count;j++)
		ResultsFile[node_id] << num_ld_str_completed[node_id][j]<<"\t|    ";

	uint64_t num_icache_access=0; 
	for(int j=0;j<core_count;j++)
		num_icache_access = num_icache_misses[node_id][j]+num_icache_hits[node_id][j];
	ResultsFile[node_id] << "\nDemand accesses icache: \t    "; 
	for(int j=0;j<core_count;j++)
		ResultsFile[node_id] << (num_icache_misses[node_id][j]+num_icache_hits[node_id][j])<< "\t|    ";

	ResultsFile[node_id] << "\nDemand hits icache: \t\t    "; 
	for(int j=0;j<core_count;j++)
		ResultsFile[node_id] << num_icache_hits[node_id][j]<< "\t|    ";

		ResultsFile[node_id] << "\nicache hit-rate: \t\t    "; 
	for(int j=0;j<core_count;j++)
		ResultsFile[node_id] << (double)(num_icache_hits[node_id][j]*100)/(double)(num_icache_misses[node_id][j]+num_icache_hits[node_id][j])<< "\t|    ";

	uint64_t dcache_accesses=0;
	for(int j=0;j<core_count;j++)
		dcache_accesses=(num_dcache_misses[node_id][j] + num_dcache_hits[node_id][j]);

	ResultsFile[node_id] << "\nDemand accesses-dcache: \t    "; 
	for(int j=0;j<core_count;j++)
		ResultsFile[node_id] << (num_dcache_hits[node_id][j]+num_dcache_misses[node_id][j])<< "\t|    ";

	ResultsFile[node_id] << "\nDemand hits-dcache: \t\t    "; 
	for(int j=0;j<core_count;j++)
		ResultsFile[node_id] << num_dcache_hits[node_id][j]<< "\t|    ";

	ResultsFile[node_id] << "\nUnique Demand misses-dcache: \t    "; 
	for(int j=0;j<core_count;j++)
		ResultsFile[node_id] << num_unique_dcache_misses[node_id][j]<< "\t|    ";

	ResultsFile[node_id] << "\ndcache hit-rate: \t\t    "; 
	for(int j=0;j<core_count;j++)
		ResultsFile[node_id] << (double)(num_dcache_hits[node_id][j]*100)/(double)(num_dcache_hits[node_id][j]+num_dcache_misses[node_id][j])<< "\t|    ";

	ResultsFile[node_id] << "\nWrite-backs dcache: \t\t    "; 
	for(int j=0;j<core_count;j++)
		ResultsFile[node_id] << num_dl1_writebacks[node_id][j]+0<< "\t|\t";

	ResultsFile[node_id] << "\nDemand accesses-L2cache: \t    "; 
	for(int j=0;j<core_count;j++)
		ResultsFile[node_id] << num_l2_access[node_id][j]<< "\t|    ";

	ResultsFile[node_id] << "\nDemand hits-L2cache: \t\t    "; 
	for(int j=0;j<core_count;j++)
		ResultsFile[node_id] <<setprecision(2)<< num_l2_hits[node_id][j]<< "\t|\t";

	ResultsFile[node_id] << "\nL2-cache hit-rate: \t\t    "; 
	for(int j=0;j<core_count;j++)
		ResultsFile[node_id] << (double)(num_l2_hits[node_id][j]*100)/(double)num_l2_access[node_id][j]<< "\t|    ";
	
	ResultsFile[node_id] << "\nWrite-backs L2cache: \t\t    "; 
	for(int j=0;j<core_count;j++)
		ResultsFile[node_id] << num_l2_writebacks[node_id][j]+0<<"\t|    ";
	
	for(int j=0;j<core_count;j++)
	{
		total_num_ld_str_total+=num_ld_str_total[node_id][j];
		total_num_ld_str_completed+=num_ld_str_completed[node_id][j];
	}

	ResultsFile[node_id] << "\nIPC is:\t\t\t\t    " ;
	for(int j=0;j<core_count;j++)
	{
		double IPC = ((long double)num_inst_commited[node_id][j] / (long double)(core_simulated_cycle[node_id][j]));
		// if(j==0)
		// 	IPC=((long double)num_inst_commited[node_id][j] / (long double)(common_clock));
		IPCC[j]=IPC;
		ResultsFile[node_id]<<setprecision(2) << IPC <<"\t|\t";
	}

	ResultsFile[node_id] << "\nCPI is:\t\t\t\t    ";
	for(int j=0;j<core_count;j++)
	{
		double IPC = ((long double)num_inst_commited[node_id][j] / (long double)(core_simulated_cycle[node_id][j]));
		// if(j==0)
		// 	IPC=((long double)num_inst_commited[node_id][j] / (long double)(common_clock));
		ResultsFile[node_id]<<setprecision(2) << 1/IPC <<"\t|\t";
	}

	for(int j=0;j<core_count;j++)
	{
		node_local_access_time+=total_core_local_time[node_id][j];
		node_local_access_count+=total_core_local_count[node_id][j];
		node_remote_access_time+=total_core_remote_time[node_id][j];
		node_remote_access_count+=total_core_remote_count[node_id][j];
	}
	ResultsFile[node_id]<<"\ncore local memory access:\t    ";
	for(int j=0;j<core_count;j++)
	{
		ResultsFile[node_id]<<total_core_local_count[node_id][j]<<"\t|\t";
	}

	ResultsFile[node_id]<<"\ncore remote memory access:\t    ";
	for(int j=0;j<core_count;j++)
	{
		ResultsFile[node_id]<<total_core_remote_count[node_id][j]<<"\t|\t";
	}

	ResultsFile[node_id]<<"\navg local access time:(ns)\t    ";
	for(int j=0;j<core_count;j++)
	{
		double temp1 = (long double)total_core_local_time[node_id][j]/(long double)(total_core_local_count[node_id][j]*3.6);
		ResultsFile[node_id]<<setprecision(2)<<temp1<<"\t|\t";
		
	}

	ResultsFile[node_id]<<"\navg remote access time:(ns)\t    ";
	for(int j=0;j<core_count;j++)
	{
		double temp2 = (long double)total_core_remote_time[node_id][j]/(long double)(total_core_remote_count[node_id][j]*3.6);
		ResultsFile[node_id]<<setprecision(2)<<temp2<<"\t|\t";
	}

	ResultsFile[node_id]<<" \nTotal memory accesses:\t\t    ";
	for(int j=0;j<core_count;j++)
	{
		ResultsFile[node_id]<<(total_core_local_count[node_id][j]+total_core_remote_count[node_id][j])<<"\t|\t";
	}

	ResultsFile[node_id]<<"\navg memory access time:(ns)\t    " ;
	for(int j=0;j<core_count;j++)
	{
		double temp3 = (long double)(total_core_local_time[node_id][j]+total_core_remote_time[node_id][j])/ (long double)(3.6*(total_core_local_count[node_id][j]+total_core_remote_count[node_id][j]));
		ResultsFile[node_id]<<setprecision(2)<<temp3<<"\t|\t";
	}

	ResultsFile[node_id] << "\n\n\n\t\t\t\t\t----------------Node "<<node_id<<" Overall Statistics---------------";
	ResultsFile[node_id] << "\nTotal Issued instructions: \t\t\t\t\t" << total_num_inst_issued;
	ResultsFile[node_id] << "\nTotal executed instructions: \t\t\t\t\t" << total_num_inst_exec;
	ResultsFile[node_id] << "\nTotal committed instructions: \t\t\t\t\t" <<total_num_inst_commited;
	float IPC=0;
	for(int i=0;i<core_count;i++)
		IPC=IPC+IPCC[i];
	ResultsFile[node_id] << "\nIPC is: \t\t\t\t\t\t\t" << IPC;
	ResultsFile[node_id] << "\nCPI is: \t\t\t\t\t\t\t" << 1/IPC;
	ResultsFile[node_id] << "\nL3 cache demand accesses: \t\t\t\t\t" << num_l3_access[node_id];
	ResultsFile[node_id] << "\nL3 cache demand hits:     \t\t\t\t\t" << num_l3_hits[node_id];
	ResultsFile[node_id] << "\nL3 cache demand misses:   \t\t\t\t\t" << num_l3_misses[node_id];
	ResultsFile[node_id] << "\nL3 cache hit rate: \t\t\t\t\t\t" << (double)(num_l3_hits[node_id]*100)/(double)num_l3_access[node_id];
	ResultsFile[node_id] << "\nWrite-backs LLC : \t\t\t\t\t\t"<<num_l3_writebacks[node_id];
	ResultsFile[node_id] << "\nTotal Load-stores by CPU: \t\t\t\t\t" <<total_num_ld_str_total;
	ResultsFile[node_id] << "\nNon-redundant Load-stores\t\t\t\t\t"<<total_num_ld_str_completed;
	uint64_t LLC_misses = node_local_access_count+node_remote_access_count;
	ResultsFile[node_id] << "\nLLC to memory requests: \t\t\t\t\t" <<LLC_misses;
	ResultsFile[node_id] << "\nOverall Miss Rate : \t\t\t\t\t\t"<<(double)(LLC_misses*100)/(double)total_num_ld_str_completed;
	ResultsFile[node_id]<<"\nTotal local memory accesses sent: \t\t\t\t"<<num_local[node_id];
	ResultsFile[node_id]<<"\nTotal local memory reads completed:    \t\t\t\t"<<node_local_access_count-local_writebacks[node_id];
	ResultsFile[node_id]<<"\nTotal local memory writebacks completed:    \t\t\t"<<local_writebacks[node_id];
	ResultsFile[node_id]<<"\nTotal local memory accesses completed: \t\t\t\t"<<node_local_access_count;
	ResultsFile[node_id]<<"\nTotal remote memory accesses sent: \t\t\t\t"<<num_remote[node_id];
	ResultsFile[node_id]<<"\nTotal remote memory reads completed:    \t\t\t"<<node_remote_access_count;
	ResultsFile[node_id]<<"\nTotal remote memory writebacks completed:    \t\t\t"<<completed_trans_node_to_remote[node_id]-node_remote_access_count;
	ResultsFile[node_id]<<"\nTotal remote memory accesses completed:   \t\t\t"<<completed_trans_node_to_remote[node_id];
	double temp1 = (long double)node_local_access_time /(long double)(3.6*node_local_access_count);
	double temp2 = (long double)node_remote_access_time/(long double)(3.6*node_remote_access_count);
	double temp3 = (long double)(node_local_access_time+node_remote_access_time)/(long double)(node_local_access_count+node_remote_access_count);
	double temp4 = (long double)(total_node_remote[node_id])/(long double)(3.6*completed_trans_node_to_remote[node_id]);
	temp3 = (double)temp3 /(double)3.6;
	// double temp4 = (long double)(node_local_access_time+node_remote_access_time+total_prefetch_access_time[node_id] - total_remote_block_prefetch_time[node_id])/(long double)(node_local_access_count+node_remote_access_count+prefetch_remote_memory_completion[node_id]-num_prefetched_blocks[node_id]);
	// temp4 = temp4 /freq_ratio;
	ResultsFile[node_id]<<"\nTotal memory accesses: \t\t\t\t\t\t"<<(node_local_access_count+node_remote_access_count);
	ResultsFile[node_id]<<setprecision(2)<<"\nNode avg local access time:\t\t\t\t\t"<<temp1<<"(ns)";
	ResultsFile[node_id]<<setprecision(2)<<"\nNode avg remote access time:\t\t\t\t\t"<<temp2<<"(ns)";
	ResultsFile[node_id]<<setprecision(2)<<"\nNode avg remote access time (excluding network):\t\t"<<temp4<<"(ns)";
    	//Calculated by tracking all the memory accesses
	ResultsFile[node_id]<<setprecision(2)<<"\nAverage memory access time  :\t\t\t\t"<<temp3<<"(ns)";
	//calculated at LLC after every LLC miss serviced
	ResultsFile[node_id]<<setprecision(2)<<"\nAverage memory access time (LLC) :\t\t\t\t"<<(long double)total_mem_cost[node_id]/(long double)(total_mem_access[node_id]*3.6);
	ResultsFile[node_id].close();
	print_mem_stats(node_id);
}

void print_mem_stats(int node_id)
{	
		string mem;
	mem = dir + "/mem_stats";
	// cout << mem << endl;
	const char *mem1 = mem.c_str();
	mem_stats.open(mem1, ios::app);
	L[node_id].get_stats();
	mem_stats<<"\n\n------ Pool Wise Page Count --------------";
	L[node_id].pool_wise_page_count(num_mem_pools);
	mem_stats<<"\n\n-------Local-Remote Mapping Entries--------------";
	L[node_id].display_mapping(mem_stats);
	mem_stats.close();
}

void fill_victim_list(int node_id)
{
	#ifdef migration
	int cnt = 0;
	for (int i = 0; i < max_victim_pages; i++)
	{
		victim_pages[node_id].push_back(L[node_id].local_page_count() -cnt-1);
		cnt++;
	}
	#endif
}
int main(int argc, char *argv[])
{
	// for(int i=0;i<num_nodes;i++){
	// 	for(int j=0;j<core_count;j++){
	// 		for(int k=0;k<num_exec_units;k++){
	// 			execution_unit[i][j][k].status=0;
	// 			cout<<"exe_stat:"execution_unit[i][j][k].status<<endl;
	// 		}
	// 	}
	// }
	for(int i=0;i<num_nodes;i++){
		RDMA_trans_id[i] = 1e12;
	}
	if(argc<2)
	{
		cout<<"\nEnter output directory name for results\n";
		exit(0);
	}
	else if(argc==2)
		dir=argv[1];

	cout<<"\nEnter starting node number:";
	cin>>nid;
	declare_memory_variables(dir);
	pthread_barrier_init(&b, NULL, num_nodes);
	pthread_mutex_init(&lock, NULL);
	pthread_mutex_init(&lock_update, NULL);
	pthread_mutex_init(&lock_queue, NULL);
	pthread_mutex_init(&lock_mem, NULL);
	pthread_t local_mem_threads[num_nodes];

	for (int i = 0; i < num_nodes; i++)
	{
		mem_trans_id[i] = 1;
		fill_victim_list(i); // Initilally filling out the victim list with random physical addresses
		for (int j = 0; j < core_count; j++)
		{
			INS_id[i][j] = 1;
			ld_str_id[i][j] = 1;
		}
		#ifdef migration
		num_pages_to_migrate[i] = 0;
		background_pg_swap_clock[i] = -1;
		#endif
	}
	// for(int i=0;i<max_victim_pages;i++){
	// 	cout<<victim_pages[0][i]<<" ";
	// }
	// cout<<endl;
	for (int i = nid; i < nid+num_nodes; i++)
	{
		// each thread for one node simulation
		pthread_create(&local_mem_threads[i - nid], NULL, node_stream_handler, (void *)i);
	}

	for (int i = 1; i <= (num_nodes); i++)
	{
		pthread_join(local_mem_threads[i - 1], NULL);
	}


for(int i=0;i<num_nodes;i++)
	{
		print_stats(i);
		print_mem_stats(i);
	}
	
	return 0;
}
