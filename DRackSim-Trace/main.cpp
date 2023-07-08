#include<iostream>
#include<vector>
#include<stack>
#include<pthread.h>
#include<semaphore.h>
#include<sstream>
#include<ctime>
#include<iomanip>
#include<math.h>
#include<string.h>

using namespace std;

long int max_cycle_num = 100000000;
long int common_clock=0;
long int result_cycle=10000000;

//CPU frequency kept at 3.6-GHz, which is 3 times the memory latenct
float CPU_Freq = 3.6;
//DDR4 used with frequency 1200-MHz
float Mem_Freq = 1.2;
//It is important to keep CPU frequency as exact divisor of Mem frequency, Same way it is mapped with interconenct latency (3-times)
float CPU_Freq_Times_Mem_Freq = (float)CPU_Freq/(float)Mem_Freq; 
int freq_ratio=ceil(CPU_Freq_Times_Mem_Freq);
const int max_samples=10000;

pthread_mutex_t lock, lock_mem;
pthread_barrier_t b;
sem_t sem1,sem2;
#include"remote_mem_allocator.cpp"
#include"inter_connect.cpp"
#include"stats.cpp"


string mem;
void print_mem_stats(int);
//function to simulate memory access at each node for local memory accesses and 
//pass remote accesses to remote_memory_handler
void *node_handler(void *node)
{
	vector<Trace> tra;
	long node_id=(long)node;
	//cout<<"\n"<<node_id;
	ifstream trace_in;
	string in;
	std::ostringstream ss;
	ss<<node_id;
	in="./Output/Node"+ss.str()+"/TraceFile.trc";
	const char * in1=in.c_str();
	trace_in.open(in1);

	Trace temp;
	node_id--;
	//used as transaction ID of local-memory request for each memory transaction
	uint64_t trans_id=0;
	
	bool trace_complete = false;
	
	if(trace_in.eof())
		trace_complete = true;

	trace_in.read((char*)&temp,sizeof(temp));

	while((common_clock < max_cycle_num) && !trace_complete)
	{
		pthread_barrier_wait(&b);
		
		while(temp.cycle==common_clock && !trace_complete)
		{
			long int mem_access_addr=NULL;

			bool page_found_in_page_table = page_table_walk(_pgd[((node_id)*max_num_process_per_node)+temp.procid-1], temp.addr, mem_access_addr);
			
			if(!page_found_in_page_table)
				handle_page_fault(_pgd[((node_id)*max_num_process_per_node)+temp.procid-1], temp.addr, mem_access_addr, L[node_id], node_id);
			
			bool isWrite=(temp.r == 'W' ? true : false);
			unsigned long page_addr = get_page_addr(mem_access_addr);
		    unsigned long page_offset_addr = mem_access_addr & (0x000000000fff);
		    bool local = L[node_id].is_local(page_addr);

			count_access[node_id]++;
			if(local)
			{
				int current_request_cycle = common_clock;
				obj.add_one_and_run(local_mem[node_id], mem_access_addr, isWrite, trans_id, current_request_cycle, current_request_cycle, node_id);
				local_access[node_id-1]++;
			}
			else
			{
				int current_request_cycle = common_clock;
    			int remote_pool = L[node_id].find_remote_pool(page_addr);
			    if(remote_pool==-1)
			    {
			    	cout<<"\npool error ";
			    	cin.get();
			    }
			    uint64_t remote_page_addr = L[node_id].remote_page_addr(page_addr, remote_pool);
			    // cout<<remote_page_addr<<" "<<remote_pool<<" ";
			    remote_page_addr = remote_page_addr << 12;
			    uint64_t remote_mem_access_addr = remote_page_addr + page_offset_addr;
			    // cout<<hex<<remote_mem_access_addr<<dec<<"\n";
			    bool isWrite=(temp.r == 'W' ? true : false);

				remote_memory_access remote_mem_access;
				remote_mem_access.source=node_id;
				remote_mem_access.dest=remote_pool;
				remote_mem_access.mem_access_addr=remote_mem_access_addr;
				remote_mem_access.trans_id=trans_id;
				remote_mem_access.isWrite = isWrite;
				remote_mem_access.cycle=current_request_cycle;
				remote_mem_access.miss_cycle_num=current_request_cycle;
				remote_mem_access.isRDMA=false;
				to_trans_layer(remote_mem_access,packet_queue_node);

				remote_access[node_id]++;
				total_packets_per_node[node_id]++;
				per_pool_access_count[node_id][remote_pool]++;
				
				pthread_mutex_lock(&lock_update);
				overall_total_packets++;
				pthread_mutex_unlock(&lock_update);
				
				long int sample_num = common_clock/max_samples;
				sample_access_count_buffer[remote_pool][sample_num]++;
			}
			
			if(trace_in.eof())
			{
				trace_complete = true;
			}
			else
			{
				trace_in.read((char*)&temp,sizeof(temp));
				trans_id++;
			}
		}
		pthread_barrier_wait(&b);

		if(common_clock%freq_ratio==(freq_ratio-1))
		{
			local_mem[node_id]->update();		
		}

		if(node_id==0)
	 	{
			if(common_clock%freq_ratio==(freq_ratio-1))
				for (int i = 0; i < num_mem_pools; i++)
					remote_mem[i]->update();
		
			simulate_network();
			simulate_network_reverse();

			if(common_clock == result_cycle)
			{
				cout<<"\nResults checkpoint created";
				print_mem_stats(common_clock/result_cycle);
			}

			common_clock++;
			if(common_clock%100000==0)
				cout<<"\nCycle completed"<<common_clock;
		}

		pthread_barrier_wait(&b);
	}

	long int extra_cycles = 100000;
	long int end_cycle = common_clock + extra_cycles;

	while(common_clock < end_cycle)
	{
		pthread_barrier_wait(&b);
		
		if(common_clock%freq_ratio==(freq_ratio-1))
			local_mem[node_id]->update();
			
		if(node_id==0)
		{
			if(common_clock%freq_ratio==(freq_ratio-1))
				for (int i = 0; i < num_mem_pools; i++)
					remote_mem[i]->update();

			simulate_network();
			simulate_network_reverse();
			
			common_clock++;
			if(common_clock%100000==0)
				cout<<"\nCycle completed"<<common_clock;
		}
		pthread_barrier_wait(&b);
	}

	// cout<<"\nnode exit"<<node_id;
	pthread_exit(NULL);
}	

void status_stat()
{
	// for(int i=0;i<num_mem_pools;i++)
	// {
	// 	cout<<"\nrx_packet_queue_pool"<<rx_packet_queue_pool[i].size();
	// 	cout<<"\nrx_nic_queue_pool"<<rx_nic_queue_pool[i].size();

	// 	for(int j=0;j<num_nodes;j++)
	// 		cout<<"\nrx_nic_queue_pool"<<rx_input_port_queue[i][j].size();			
	// }

	cout<<"\npack_in: "<<overall_total_packets;
	cout<<"\npack_out: "<<packets_out;
	cout<<"\npackets end: "<<rx_overall_total_packets;
}			

int main(int argc, char *argv[])
{
	string dir;
	if(argc<2)
	{
		cout<<"\nEnter all the arguments: Ist arg: Output Dir Name 		2nd arg: Max cycles to simulate (default=1e8)";
		exit(0);
	}
	else if(argc==2 || argc==3)
	{
		dir=argv[1];
	}
	
	if(argc==3)
	{
		char ptr[30];
		char *eptr;
		strcpy(ptr,argv[2]);
		max_cycle_num = strtoll(ptr, &eptr, 10);
	}

	cout<<"\nMax cycles to simulate: "<<max_cycle_num;
	
	for(int i=0;i<num_nodes;i++)
	{
		node_round_robin_last[i]=1;
	}

	declare_memory_variables(dir);

	pthread_mutex_init(&lock,NULL);
	pthread_mutex_init(&lock_queue,NULL);
	pthread_mutex_init(&lock_update,NULL);
	pthread_barrier_init(&b,NULL,num_nodes);

	sem_init(&sem1,0,0);
	sem_init(&sem2,0,0);
	pthread_t local_mem_threads[num_nodes];

	//threads for simulating local memory access to each node
	for(long i=1;i<=num_nodes;i++)
	{
		//thread-1 is responsble for sending the per window remote memory accesses
		//to the remote pools
		pthread_create(&local_mem_threads[i-1], NULL, node_handler, (void *)i);
	}

	for(int i=0;i<num_mem_pools;i++)
	{
		alloc_count[i]=1;

		if((i+1)<num_mem_pools)
			access_factor[i]=0;
		else
			access_factor[i]=100000000;
	}

	for(long i=1;i<=(num_nodes);i++)
	{
		pthread_join(local_mem_threads[i-1],NULL);
	}

	//last batch of remote memory accesses has been sent and all accesses 
	//from all nodes have been parsed

	pthread_detach(pthread_self());
	pthread_mutex_destroy(&lock);
	pthread_mutex_destroy(&lock_update);
	pthread_mutex_destroy(&lock_queue);

	sem_destroy(&sem1);
	sem_destroy(&sem2);

//print memory stats in DRAM simulator output files
	for(int i=0;i<num_nodes;i++)
		local_mem[i]->printStats(true);

	for(int i=0;i<num_mem_pools;i++)
		remote_mem[i]->printStats(true);

	print_mem_stats(-1);
	status_stat();
	mem_stats.close();
	out.close();
	invalid.close();
	track.close();

cout<<"\n====================Simulation_Complete====================\n";
//	pthread_exit(NULL);	

/*	L[0].display_mapping();
	R[0].display_mapping();

	// get a nice summary of this epoch 
//	mem->printStats(true);

	cout<<"\nFree pages left "<<L.has_free();//<<endl;
	cout<<"\n\nPages Reffered: \t"<<page_count<<endl;
	cout<<endl;*/
	return 0;
}
