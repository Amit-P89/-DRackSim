static int round_robin_last=1;

//pool allocation policies


//round-robin memory pool allocation
int round_robin_pool_select()
{
	if(num_mem_pools==1)
		return 0;
	
	out<<"\n\n=======Window-"<<epoch_num<<"========\n";
	for(int i=0;i<num_mem_pools;i++)
	{
		out<<"\npool-"<<i<<"  current_access_count :"<<R[i].get_count()<<"  total_access_count :"<<_total_access_count[i];
	}
	if(round_robin_last>num_mem_pools)
	{
		round_robin_last=round_robin_last%num_mem_pools;
	}
	round_robin_last++;
	out<<"\nSelected pool is- "<<(round_robin_last-2);
	return (round_robin_last-2);
}



// static int node_round_robin_last[num_nodes];
// //node-wise round-robin memory pool allocation
// int per_node_round_robin_pool_select(int node_no)
// {
// 	out<<"\n\n=======Window-"<<epoch_num<<"========\n";
// 	for(int i=0;i<num_mem_pools;i++)
// 	{
// 		out<<"\npool-"<<i<<"  current_access_count :"<<R[i].get_count()<<"  total_access_count :"<<_total_access_count[i];
// 	}
// 	if(node_round_robin_last[node_no]>num_mem_pools)
// 	{
// 		node_round_robin_last[node_no]=node_round_robin_last[node_no]%num_mem_pools;
// 	}
// 	node_round_robin_last[node_no]++;
// 	out<<"\nSelected pool for Node:"<<node_no<<" is- "<<(node_round_robin_last[node_no]-2);
// 	return (node_round_robin_last[node_no]-2);
// }


// long double last[num_mem_pools]={0};
// long double sec_last[num_mem_pools]={0};
// long double third_last[num_mem_pools]={0};
// long double current[num_mem_pools]={0};
// long double access_factor[num_mem_pools+1]={0,0,0,0,0,0,100000000};//,0,0,100000000};
// long int alloc_count[num_mem_pools]={1,1,1,1,1,1};
// unsigned int reserve_count=0;
// //to find the remote pool according to the idle first memory algorithm used
// int last_alloc=-1;
// int min_access_count()
// {
// 	long double total_access_count=1;
// 	for(int i=0;i<num_mem_pools;i++)
// 	{
// 		//total_access_count=total_access_count+_total_access_count[i]+R[i].get_count();
// 	}
// 	reserve_count++;
// 	out<<"\n\n=======Window-"<<epoch_num<<"========\n";
// 	for(int i=0;i<num_mem_pools;i++)
// 	{
// 		total_access_count=0;
// 		third_last[i]=sec_last[i];
// 		sec_last[i]=last[i];
// 		last[i]=current[i];
// 		current[i]=R[i].get_count();
// 		total_access_count=last[i]+sec_last[i]+third_last[i]+current[i]+1;
// 		long double past_af=((last[i] + sec_last[i]+ third_last[i])/3);
// 		//if(alloc_count[i]==0)
// 			access_factor[i]= (R[i].get_count()) + (past_af);
// 		//else
// 			//access_factor[i]=(long double)((_total_access_count[i] * 0.3 )/ alloc_count[i]) + (long double)(R[i].get_count() * 0.7);
		
// 		out<<"\npool-"<<i<<"  currnt :"<<current[i]<<" last:"<<last[i]<<" sec_lst:"<<sec_last[i]<<" 3rd_lst:"<<third_last[i]<<" 3-alloc_tot:"<<total_access_count<<"  tot_acc_cnt :"<<_total_access_count[i]<<"  acc_fac:"<<access_factor[i];

// 		_total_access_count[i]=_total_access_count[i]+R[i].get_count();
// 		R[i].reset_count();
// 	}

// 	if(reserve_count<=4)	//round-robin memory pool allocation for ist time
// 	{
// 		if(round_robin_last>num_mem_pools)
// 		{
// 			round_robin_last=round_robin_last%num_mem_pools;
// 		}
// 		round_robin_last++;
// 		out<<"\nSelected pool is- "<<(round_robin_last-2);
// 		alloc_count[round_robin_last-2]++;
// 		return (round_robin_last-2);
// 	}
// 	else
// 	{
// 		int min=num_mem_pools;
// 		for(int i=0;i<num_mem_pools;i++)
// 		{
// 			if(access_factor[i]<access_factor[min] && i!=last_alloc)
// 				min=i;
// 		}
// 		last_alloc=min;
// 		out<<"\nIdle Selected pool is- "<<min;
// 		alloc_count[min]++;
// 		return min;
// 	}
// }

// int smart_idle()
// {
// 	//size of min_set with minimum access factors
// 	int size=ceil(log2(num_mem_pools));
// 	uint64_t *min_set=new uint64_t[size];
	
// 	long double total_access_count=1;
	
// 	reserve_count++;
	
// 	out<<"\n\n=======Window-"<<epoch_num<<"========\n";
// 	for(int i=0;i<num_mem_pools;i++)
// 	{
// 		total_access_count=0;
// 		third_last[i]=sec_last[i];
// 		sec_last[i]=last[i];
// 		last[i]=current[i];
// 		current[i]=R[i].get_count();
// 		total_access_count=last[i]+sec_last[i]+third_last[i]+current[i]+1;
// 		long double past_af=((last[i] + sec_last[i]+ third_last[i])/3);
// 		access_factor[i]= (R[i].get_count()) + (past_af);
		
// 		out<<"\npool-"<<i<<"  currnt :"<<current[i]<<" last:"<<last[i]<<" sec_lst:"<<sec_last[i]<<" 3rd_lst:"<<third_last[i]<<" 3-alloc_tot:"<<total_access_count<<"  tot_acc_cnt :"<<_total_access_count[i]<<"  acc_fac:"<<access_factor[i];

// 		_total_access_count[i]=_total_access_count[i]+R[i].get_count();
// 		R[i].reset_count();
// 	}

// 	if(reserve_count<=num_mem_pools)	//round-robin memory pool allocation for ist time
// 	{
// 		if(round_robin_last>num_mem_pools)
// 		{
// 			round_robin_last=round_robin_last%num_mem_pools;
// 		}
// 		round_robin_last++;
// 		out<<"\nSelected pool is- "<<(round_robin_last-2);
// 		alloc_count[round_robin_last-2]++;
// 		last_alloc=round_robin_last-2;
// 		return (round_robin_last-2);
// 	}
// 	else
// 	{
// 		for(int z=0;z<size;z++)
// 		{
// 			min_set[z]=num_mem_pools;
// 			for(int i=0;i<num_mem_pools;i++)
// 			{
// 				if(access_factor[i]<access_factor[min_set[z]])
// 				{
// 					bool exist_in_set=false;
// 					if(z>0)
// 					{
// 						for(int j=0;j<z;j++)
// 						{
// 							if(min_set[j]==(uint64_t)i)
// 								exist_in_set=true;
// 						}
// 					}
// 					if(exist_in_set==true)
// 						continue;
// 					else
// 						min_set[z]=i;
// 				}
// 			}
// 		}

// 		out<<"\nMem_pools in set are-\n";
// 		for(int i=0;i<size;i++)
// 			out<<min_set[i]<<"\tmem_left: "<<R[min_set[i]].mem_left()<<"\t";

// 		int min_small=min_set[0];		
// 		for(int i=0;i<size;i++)
// 		{
// 			if(R[min_small].mem_left()<=R[min_set[i]].mem_left() && min_set[i]!=(uint64_t)last_alloc)
// 				min_small=min_set[i];
// 		}

// 		last_alloc=min_small;
// 		out<<"\nIdle Selected pool is- "<<min_small;
// 		alloc_count[min_small]++;
// 		return min_small;
// 	}
// }

// int Random()
// {
// 	int random=rand()%num_mem_pools;
// 	out<<"\n\n=======Window-"<<epoch_num<<"========";
// 	out<<"\nSelected pool is- "<<random;
// 	return random;
// }

// long Random1(long limit)
// {
// 	int divisor=RAND_MAX/(limit+1);
// 	int random;
// 	do {
// 	    random = rand() / divisor;
// 	} while (random > limit);

// 	    out<<"\n\n=======Window-"<<epoch_num<<"========\n";
// 	    out<<"\nSelected pool is- "<<random;

// 	return random;
// }
// //end allocation policies

// //capacity allocator


// //used for capacity allocation algorithm, request counter
// long double node_epoch_chunk_count[num_nodes];
// //request size per node
// int node_chunk_request_size[num_nodes];


// //access count per node in each epoch
// long double node_epoch_access_count[num_nodes];

// //capacity allocator

// void modify_chunk_sizes()
// {
// 	invalid<<"\n------------------New-win----------------------\n";

// 	//max and min chunk request count for this window
// 	int max=0;
// 	int min=100000;

// 	//number of nodes requesting chunks in this window
// 	int num_nodes_rqstd_chunks=0;
// 	//total number of requested chunks
// 	int total_chunks_rqstd=0;

// 	for(int i=0;i<num_nodes;i++)
// 	{
// 		if(node_epoch_chunk_count[i]>max && node_epoch_chunk_count[i]!=0)
// 			max=node_epoch_chunk_count[i];

// 		if(node_epoch_chunk_count[i]<min && node_epoch_chunk_count[i]!=0)
// 			min=node_epoch_chunk_count[i];

// 		total_chunks_rqstd=total_chunks_rqstd+node_epoch_chunk_count[i];
// 		if(node_epoch_chunk_count[i]>0)
// 			num_nodes_rqstd_chunks++;
// 	}

// 	invalid<<"\nMax Count-"<<max<<"  Min Count-"<<min;

// 	if(num_nodes_rqstd_chunks==0)
// 		return;

// 	int avg_chunks_per_node=total_chunks_rqstd/num_nodes_rqstd_chunks;
// 	invalid<<"\nAverage count per node-"<<avg_chunks_per_node;

// 	//chunk request count for high and low category
// 	int high_limit=(avg_chunks_per_node+max)/2;
// 	int low_limit=(avg_chunks_per_node+min)/2;

// 	invalid<<"\nHigh-Limit-"<<high_limit;
// 	invalid<<"\nLow-Limit-"<<low_limit;


// 	if(high_limit==low_limit)
// 	{
// 		invalid<<"\nno-change";
// 		return;
// 	}

// 	invalid<<"\n";
// 	for(int i=0;i<num_nodes;i++)
// 	{
// 		invalid<<"Old_N"<<i+1<<"-"<<node_chunk_request_size[i];
// 		if(node_epoch_chunk_count[i]>high_limit && node_epoch_chunk_count[i]!=0)
// 		{
// 			if(node_chunk_request_size[i]!=16)
// 				node_chunk_request_size[i]=node_chunk_request_size[i]+2;
// 			else
// 				node_chunk_request_size[i]=16;
// 		}

// 		if(node_epoch_chunk_count[i]<low_limit && node_epoch_chunk_count[i]!=0)
// 		{
// 			if(node_chunk_request_size[i]!=4)
// 				node_chunk_request_size[i]=node_chunk_request_size[i]-2;
// 			else
// 				node_chunk_request_size[i]=4;
// 		}
// 		invalid<<" New_N"<<i+1<<"-"<<node_chunk_request_size[i]<<"\t";
// 	}
// }

// void reset_chunk_counts()
// {
// 	for(int i=0;i<num_nodes;i++)
// 		node_epoch_chunk_count[i]=0;
// }


// //capacity allocator 2

// void modify_chunk_sizes1()
// {

// 	invalid<<"\n------------------New-win----------------------\n";
// 	long double max_mem_rqsts=0;
// 	long double min_mem_rqsts=99999999999;

// 	int max_chunk_count=0;
// 	int min_chunk_count=999999;



// 	int total_chunks_rqstd=0;
// 	int num_nodes_rqstd_chunks=0;

// 	int total_nodes_rqstd_mem=0;
// 	long double total_mem_request=0;

// 	for(int i=0;i<num_nodes;i++)
// 	{
// 		if(node_epoch_chunk_count[i]>0)
// 		{
// 			if(node_epoch_chunk_count[i]>max_chunk_count)
// 				max_chunk_count=node_epoch_chunk_count[i];

// 			if(node_epoch_chunk_count[i]<min_chunk_count)
// 				min_chunk_count=node_epoch_chunk_count[i];

// 			total_chunks_rqstd=total_chunks_rqstd+node_epoch_chunk_count[i];
// 			num_nodes_rqstd_chunks++;
// 		}
// 		if(node_epoch_access_count[i]>0)
// 		{
// 			if(node_epoch_access_count[i]>max_mem_rqsts)
// 				max_mem_rqsts=node_epoch_access_count[i];

// 			if(node_epoch_access_count[i]<min_mem_rqsts)
// 				min_mem_rqsts=node_epoch_access_count[i];

// 			total_mem_request=total_mem_request+node_epoch_access_count[i];
// 			total_nodes_rqstd_mem++;
// 		}
// 	}
// 	invalid<<"\nMax chunk Count-"<<max_chunk_count<<"  Min chunk Count-"<<min_chunk_count;
// 	invalid<<"\nMax mem Requests-"<<max_mem_rqsts<<"  Min mem Requests-"<<min_mem_rqsts;
	
// 	int avg_chunk_per_node=total_chunks_rqstd/num_nodes_rqstd_chunks;
// 	long double avg_mem_request_per_node=total_mem_request/total_nodes_rqstd_mem;

// 	invalid<<"\nAvg chunk Count-"<<avg_chunk_per_node<<"  Avg mem Requests-"<<avg_mem_request_per_node;


// 	int high_limit_chunk_count= (avg_chunk_per_node + max_chunk_count)/2;
// 	int low_limit_chunk_count= (avg_chunk_per_node + min_chunk_count)/2;

// 	invalid<<"\nHigh chunk Count limit-"<<high_limit_chunk_count<<"  Low chunk Count limit-"<<low_limit_chunk_count;

// 	long double high_limit_mem_rqsts= (avg_mem_request_per_node+max_mem_rqsts)/2;
// 	long double low_limit_mem_rqsts= (avg_mem_request_per_node+min_mem_rqsts)/2;

// 	invalid<<"\nHigh mem Requests limit-"<<high_limit_mem_rqsts<<"  Low mem Requests limit-"<<low_limit_mem_rqsts;

// 	for(int i=0;i<num_nodes;i++)
// 	{
// 		invalid<<"Old_N"<<i+1<<"-"<<node_chunk_request_size[i];

// 		int access_rate=0;//-1,0,1  -1 for low, 0 for medium, 1 for high
// 	//	int chunk_size=0;//-1,0,1 -1 for small, 0 for medium, 1 for large
// 		int chunk_count=0;//-1,0,1

// //find access_rate range of a node
// 		if(node_epoch_access_count[i]<=low_limit_mem_rqsts)
// 			access_rate=-1;
// 		else if(node_epoch_access_count[i]>low_limit_mem_rqsts && node_epoch_access_count[i]<=high_limit_mem_rqsts)
// 			access_rate=0;
// 		else if(node_epoch_access_count[i]>high_limit_mem_rqsts)
// 			access_rate=1;

// //find chunk request frequency range of a node
// 		if(node_epoch_chunk_count[i]>high_limit_chunk_count)
// 			chunk_count=1;
// 		else if(node_epoch_chunk_count[i]>=low_limit_chunk_count && node_epoch_chunk_count[i]<=high_limit_chunk_count)
// 			chunk_count=0;
// 		else if(node_epoch_chunk_count[i]<low_limit_chunk_count)
// 			chunk_count=-1;


// //find chunk_size range of a node
// 		/*if(node_chunk_request_size[i]<=8)
// 			chunk_size=-1;
// 		else if(node_chunk_request_size[i]>8 && node_chunk_request_size[i]<=12)
// 			chunk_size=0;
// 		else if(node_chunk_request_size[i]>12)
// 			chunk_size=1;
// */
// //modify size


// 		if(node_epoch_access_count[i]>0)
// 		{
// 			if(access_rate==-1 && chunk_count==-1)
// 				node_chunk_request_size[i]=node_chunk_request_size[i]-4;
// 			else if(access_rate==-1 && chunk_count== 0)
// 				node_chunk_request_size[i]=node_chunk_request_size[i]-2;
// 			else if(access_rate==-1 && chunk_count== 1)
// 				node_chunk_request_size[i]=node_chunk_request_size[i];
// 			else if(access_rate== 0 && chunk_count==-1)
// 				node_chunk_request_size[i]=node_chunk_request_size[i]+2;
// 			else if(access_rate== 0 && chunk_count== 0)
// 				node_chunk_request_size[i]=node_chunk_request_size[i]; //no-change
// 			else if(access_rate== 0 && chunk_count== 1)
// 				node_chunk_request_size[i]=node_chunk_request_size[i]-2;
// 			else if(access_rate== 1 && chunk_count==-1)
// 				node_chunk_request_size[i]=node_chunk_request_size[i];
// 			else if(access_rate== 1 && chunk_count== 0)
// 				node_chunk_request_size[i]=node_chunk_request_size[i]+2;
// 			else if(access_rate== 1 && chunk_count== 1)
// 				node_chunk_request_size[i]=node_chunk_request_size[i]+4;

// 			if(node_chunk_request_size[i]>16)
// 				node_chunk_request_size[i]=16;
// 			else if(node_chunk_request_size[i]<4)
// 				node_chunk_request_size[i]=4;
// 		}

// 	invalid<<" New_N"<<i+1<<"-"<<node_chunk_request_size[i]<<"\n";
// 	}

// }

// void reset_chunk_count_and_access()
// {
// 	reset_chunk_counts();

// 	for(int i=0;i<num_nodes;i++)
// 		node_epoch_access_count[i]=0;
// }