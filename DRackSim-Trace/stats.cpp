//Till memory access reach the memory pool
void calculate_and_print_average_network_delays(int window_no)
{
	if(window_no==-1)
		netstats<<"\n\n"<<"=========================Final_epoch=========================\n";
	else
		netstats<<"\n\n"<<"=========================Epoch"<<(common_clock/result_cycle)<<"=========================\n";	

	netstats<<"\n---------Node-Wise Average Packet Delay----------\n";
	netstats<<"\n\t\tSending\t\tReceiving\t\tTotal\n";
	for(int i=0;i<num_nodes;i++)
	{
		long double avg_network_delay = total_network_delay[i]/(total_packets_per_node[i]*CPU_Freq);
		long double rx_avg_network_delay = rx_total_network_delay[i]/(rx_total_packets_per_node[i]*CPU_Freq);
		mem_access_avg_network_delay[i] = (avg_network_delay + rx_avg_network_delay);
		netstats<<fixed<<setprecision(4)<<"\nNode_no-"<<(i+1)<<"  : "<<avg_network_delay<<"\t\t: "<<rx_avg_network_delay	\
		<<"\t\t: "<<mem_access_avg_network_delay[i]<<"\t\tpackets sent: "<<total_packets_per_node[i]<<"\t\tPackets received: "<<rx_total_packets_per_node[i];
	}

	netstats<<"\n\n\n---------Overall Network Delays for all the Packets from source to destination to source----------\n";
	{
		netstats<<fixed<<setprecision(4)<<"\n\tTotal Packets sent: "<<overall_total_packets<<"\t\tTotal Packets received: "<<rx_overall_total_packets;
		overall_avg_network_delay=overall_total_network_delay/(overall_total_packets*CPU_Freq);
		rx_overall_avg_network_delay=rx_overall_total_network_delay/(rx_overall_total_packets*CPU_Freq);
		long double total_average=overall_avg_network_delay+rx_overall_avg_network_delay;
		netstats<<"\n\n";
		netstats<<fixed<<setprecision(4)<<"\nAverage Network Delay while transmitting packets from node to pool= "<<overall_avg_network_delay;
		netstats<<fixed<<setprecision(4)<<"\nAverage Network Delay while Receiving packets from pool to node= "<<rx_overall_avg_network_delay;
		netstats<<fixed<<setprecision(4)<<"\nTotal Average Network Delay Per Packet= "<<total_average;
	}

	netstats<<"\n\n-----------------------------------------------------------\n\n";

}


void print_pool_wise_diff_memory_accesses_time()
{
	track<<"\n--------No. of memory access with access time less than 100ns:--------\n";
	for(int i=0;i<num_mem_pools;i++)
	{
		track<<"\nPool_no-"<<(i+1)<<" : "<<U_100ns[i];
	}
	track<<"\n\n--------No. of memory access with access time between 100ns and 300ns:--------\n";
	for(int i=0;i<num_mem_pools;i++)
	{
		track<<"\nPool_no-"<<(i+1)<<" : "<<B_100_300ns[i];
	}
	track<<"\n\n--------No. of memory access with access time between 300ns and 500ns:--------\n";
	for(int i=0;i<num_mem_pools;i++)
	{
		track<<"\nPool_no-"<<(i+1)<<" : "<<B_300_500ns[i];
	}
	track<<"\n\n--------No. of memory access with access time between 500ns and 700ns:--------\n";
	for(int i=0;i<num_mem_pools;i++)
	{
		track<<"\nPool_no-"<<(i+1)<<" : "<<B_500_700ns[i];
	}	
	track<<"\n\n--------No. of memory access with access time between 700ns and 1000ns:--------\n";
	for(int i=0;i<num_mem_pools;i++)
	{
		track<<"\nPool_no-"<<(i+1)<<" : "<<B_700_1000ns[i];
	}
	track<<"\n\n--------No. of memory access with access time gretaer than 1000ns:--------\n";
	for(int i=0;i<num_mem_pools;i++)
	{
		track<<"\nPool_no-"<<(i+1)<<" : "<<G_1000ns[i];
	}
}


void calculate_avg_mem_access_cycle()
{
	pthread_mutex_lock(&lock_update);

	mem_stats<<"\n\t\t\t\tAverage no. of cycles for memory access\n========================================================================================";
	
	for(int i=0;i<num_nodes;i++)
	{	
		avg_node_local[i]=0;
		avg_node_local[i] = total_node_local[i] / (long double)(completed_trans_local[i]*CPU_Freq);
		avg_node_local_read[i] = total_node_local_read[i] / (long double)(completed_trans_local_read[i]*CPU_Freq);
		avg_node_local_write[i] = total_node_local_write[i] / (long double)(completed_trans_local_write[i]*CPU_Freq);

		avg_node_remote[i]=0;
		avg_node_remote[i] = total_node_remote[i] / (long double)(completed_trans_node_to_remote[i]*CPU_Freq);

		avg_node_remote_read[i]=0;
		avg_node_remote_read[i] = total_node_remote_read[i] / (long double)(completed_trans_node_to_remote_read[i]*CPU_Freq);

		avg_node_remote_write[i]=0;
		avg_node_remote_write[i] = total_node_remote_write[i] / (long double)(completed_trans_node_to_remote_write[i]*CPU_Freq);


		avg_node_overall[i]=0;
		avg_node_overall[i]= (total_node_local[i] + total_node_remote[i]) / (long double) ((completed_trans_local[i] + completed_trans_node_to_remote[i])*CPU_Freq);

		avg_node_overall_read[i]=0;
		avg_node_overall_read[i]= (total_node_local_read[i] + total_node_remote_read[i]) / (long double) ((completed_trans_local_read[i] + completed_trans_node_to_remote_read[i])*CPU_Freq);

		avg_node_overall_write[i]=0;
		avg_node_overall_write[i]= (total_node_local_write[i] + total_node_remote_write[i]) / (long double) ((completed_trans_local_write[i] + completed_trans_node_to_remote_write[i])*CPU_Freq);

		mem_stats<<"\n\n\t\t\t\t===============Node-"<<(i+1)<<"===============";
		mem_stats<<"\nTotal Local memory access and Avg. memory Latency :\t\t"<<completed_trans_local[i]<<"("<<avg_node_local[i]<<")ns";
		mem_stats<<"\tR:"<<completed_trans_local_read[i]<<"("<<avg_node_local_read[i]<<"ns)"<<"\tW:"<<completed_trans_local_write[i]\
		<<"("<<avg_node_local_write[i]<<"ns)";


		mem_stats<<"\nTotal Remote memory access and Avg. memory latency :\t\t "<<completed_trans_node_to_remote[i]<<"("<<avg_node_remote[i]<<")ns";
		mem_stats<<"\tR:"<<completed_trans_node_to_remote_read[i]<<"("<<avg_node_remote_read[i]<<"ns)"<<"\tW:"<<completed_trans_node_to_remote_write[i]\
		<<"("<<avg_node_remote_write[i]<<"ns)";

		mem_stats<<"\nTotal (Local+Remote) memory access and Avg. memory latency :\t"<<completed_trans_local[i]+completed_trans_node_to_remote[i]<<"("<<avg_node_overall[i]<<")ns";
		mem_stats<<"\tR:"<<completed_trans_local_read[i]+completed_trans_node_to_remote_read[i]<<"("<<avg_node_overall_read[i]<<"ns)"<<"\tW:"<<completed_trans_local_write[i]+completed_trans_node_to_remote_write[i]\
		<<"("<<avg_node_overall_write[i]<<"ns)";
	}

	mem_stats<<"\n\n\t\t\t\t=============Memory Access at Pools=============";

	for(int i=0;i<num_mem_pools;i++)
	{
		avg_remote_pool[i]=0;
		avg_remote_pool[i] = total_remote_pool[i] / (long double)(completed_trans_remote[i]*CPU_Freq);

		avg_remote_pool_read[i]=0;
		avg_remote_pool_read[i] = total_remote_pool_read[i] / (long double)(completed_trans_remote_read[i]*CPU_Freq);

		avg_remote_pool_write[i]=0;
		avg_remote_pool_write[i] = total_remote_pool_write[i] / (long double)(completed_trans_remote_write[i]*CPU_Freq);

		mem_stats<<"\nTotal Memory Accesses and Avg. Memory Latency at remote pool-"<<i+1<<" :\t"<<completed_trans_remote[i]<<"("<<avg_remote_pool[i]<<"ns)";
		mem_stats<<"\tR"<<completed_trans_remote_read[i]<<"("<<avg_remote_pool_read[i]<<"ns)"<<"\tW:"<<completed_trans_remote_write[i]<<"("<<avg_remote_pool_write[i]<<"ns)";
	}

	mem_stats<<endl<<endl;

	pthread_mutex_unlock(&lock_update);
}


//for collecting samples on number of memory access to each pool during different epochs 
long int sample_access_count_buffer[num_mem_pools][max_samples]={0};

void print_per_sample_access_count()
{
	track<<"\n\n--------Pool wise no. of memory accesses per sample(One window consists of 10 such samples and one windows includes all remote memory access within range of 10000000 cycles)--------\n";
	track<<"\nTotal samples:"<<max_samples<<"\n\n";
	for(int i=0;i<num_mem_pools;i++)
	{
		track<<"\n-------------Pool-no"<<i<<"-------------\n";
		for(int j=0;j<max_samples;j++)
		{
			track<<"\n"<<sample_access_count_buffer[i][j];
		}
	}
}


//print the collected stats till this window_no
void print_mem_stats(int window_no)
{
	calculate_and_print_average_network_delays(window_no);
	unsigned long total_accesses=0;

	for(int i=0;i<num_nodes;i++)
		total_accesses=total_accesses+count_access[i];

	if(window_no==-1)
		mem_stats<<"\n\n"<<"=========================Final_epoch=========================\n";
	else
		mem_stats<<"\n\n"<<"=========================Epoch"<<(common_clock/result_cycle)<<"=========================\n";	

	mem_stats<<"\nTotal Memory Accesses till this epoch of simulation= "<<total_accesses;
	mem_stats<<endl;


	mem_stats<<"\n\n\t\t\tNode wise total memory accesses";
	mem_stats<<"\n--------------------------------------------------------------\n";
	for(int j=0;j<num_nodes;j++)
		mem_stats<<"Memory accesses in NODE-"<<j+1<<" :"<<count_access[j]<<endl;

	mem_stats<<endl;


//Print total accesses in each memory
	mem_stats<<"\n=========================================";
	for(int i=0;i<num_nodes;i++)
	{
		mem_stats<<"\n\t\t\t\tNode-"<<i+1;
		mem_stats<<"\n=========================================";
		mem_stats<<"\n\nLocal_Accesses-"<<(i+1)<<"\t:"<<local_access[i];
		mem_stats<<"\nRemote_Accesses-"<<(i+1)<<"\t:"<<remote_access[i];//  (count_access[i] - local_access[i]);

		mem_stats<<endl;
		for(int j=0;j<num_mem_pools;j++)
			mem_stats<<"\nTotal memory access in remote pool-"<<j+1<<" :"<<per_pool_access_count[i][j];

		mem_stats<<"\n\n";
		L[i].get_stats();

		mem_stats<<"\nPool wise page count";
		L[i].pool_wise_page_count(num_mem_pools);
		mem_stats<<"\n\n--------------------------------------------------------------";
		
	}
mem_stats<<"\n========================================================================================";
	calculate_avg_mem_access_cycle();
	
	for(int i=0;i<num_nodes;i++)
	{
		if(window_no==-1)
		{
			mem_stats<<"\nLocal-Remote Mapping Enteries\n\n";
			L[i].display_mapping(mem_stats);
		}
	}

	if(window_no==-1)
	{
		print_pool_wise_diff_memory_accesses_time();
		print_per_sample_access_count();
	}
//	int epoch=window_no/res_window;
}