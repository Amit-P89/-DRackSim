int round_robin_pool_select();
int per_node_round_robin_pool_select(int);
int min_access_count();
int smart_idle();
int Random();
int uni_distribution(int );

#include"mem_defs.cpp"


//pool allocation policies


static int round_robin_last=1;
//round-robin memory pool allocation
int round_robin_pool_select()
{
	if(num_mem_pools==1)
		return 0;
	out<<"\n\n=======Cycle-"<<common_clock<<"========\n";
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



static int node_round_robin_last[num_nodes];
//node-wise round-robin memory pool allocation
int per_node_round_robin_pool_select(int node_no)
{
	out<<"\n\n=======Cycle-"<<common_clock<<"========\n";
	for(int i=0;i<num_mem_pools;i++)
	{
		out<<"\npool-"<<i<<"  current_access_count :"<<R[i].get_count()<<"  total_access_count :"<<_total_access_count[i];
	}
	if(node_round_robin_last[node_no]>num_mem_pools)
	{
		node_round_robin_last[node_no]=node_round_robin_last[node_no]%num_mem_pools;
	}
	node_round_robin_last[node_no]++;
	out<<"\nSelected pool for Node:"<<node_no<<" is- "<<(node_round_robin_last[node_no]-2);
	return (node_round_robin_last[node_no]-2);
}


long double last[num_mem_pools]={0};
long double sec_last[num_mem_pools]={0};
long double third_last[num_mem_pools]={0};
long double current[num_mem_pools]={0};
long double access_factor[num_mem_pools+1];
long int alloc_count[num_mem_pools];
unsigned int reserve_count=0;
//to find the remote pool according to the idle first memory algorithm used
int last_alloc=-1;

int smart_idle()
{
	//size of min_set with minimum access factors
	int size=ceil(log2(num_mem_pools));
	uint64_t *min_set=new uint64_t[size];
	
	long double total_access_count=1;
	
	reserve_count++;
	
	out<<"\n\n=======Cycle-"<<common_clock<<"========\n";
	for(int i=0;i<num_mem_pools;i++)
	{
		total_access_count=0;
		third_last[i]=sec_last[i];
		sec_last[i]=last[i];
		last[i]=current[i];
		current[i]=R[i].get_count();
		total_access_count=last[i]+sec_last[i]+third_last[i]+current[i]+1;
		long double past_af=((last[i] + sec_last[i]+ third_last[i])/3);
		access_factor[i]= (R[i].get_count()) + (past_af);
		
		out<<"\npool-"<<i<<"  currnt :"<<current[i]<<" last:"<<last[i]<<" sec_lst:"<<sec_last[i]<<" 3rd_lst:"<<third_last[i]<<" 3-alloc_tot:"<<total_access_count<<"  tot_acc_cnt :"<<_total_access_count[i]<<"  acc_fac:"<<access_factor[i];

		_total_access_count[i]=_total_access_count[i]+R[i].get_count();
		R[i].reset_count();
	}

	if(reserve_count<=num_mem_pools)	//round-robin memory pool allocation for ist time
	{
		if(round_robin_last>num_mem_pools)
		{
			round_robin_last=round_robin_last%num_mem_pools;
		}
		round_robin_last++;
		out<<"\nSelected pool is- "<<(round_robin_last-2);
		alloc_count[round_robin_last-2]++;
		last_alloc=round_robin_last-2;
		return (round_robin_last-2);
	}
	else
	{
		for(int z=0;z<size;z++)
		{
			min_set[z]=num_mem_pools;
			for(int i=0;i<num_mem_pools;i++)
			{
				if(access_factor[i]<access_factor[min_set[z]])
				{
					bool exist_in_set=false;
					if(z>0)
					{
						for(int j=0;j<z;j++)
						{
							if(min_set[j]==(uint64_t)i)
								exist_in_set=true;
						}
					}
					if(exist_in_set==true)
						continue;
					else
						min_set[z]=i;
				}
			}
		}

		out<<"\nMem_pools in set are-\n";
		for(int i=0;i<size;i++)
			out<<min_set[i]<<"\tmem_left: "<<R[min_set[i]].mem_left()<<"\t";

		int min_small=min_set[0];		
		for(int i=0;i<size;i++)
		{
			if(R[min_small].mem_left()<=R[min_set[i]].mem_left() && min_set[i]!=(uint64_t)last_alloc)
				min_small=min_set[i];
		}

		last_alloc=min_small;
		out<<"\nIdle Selected pool is- "<<min_small;
		alloc_count[min_small]++;
		return min_small;
	}
}

int Random()
{
	int random=rand()%num_mem_pools;
	out<<"\n\n=======Cycle-"<<common_clock<<"========\n";
	out<<"\nSelected pool is- "<<random;
	return random;
}

long Random1(long limit)
{
	int divisor=RAND_MAX/(limit+1);
	int random;
	do {
	    random = rand() / divisor;
	} while (random > limit);

	out<<"\n\n=======Cycle-"<<common_clock<<"========\n";
	out<<"\nSelected pool is- "<<random;

	return random;
}
//end allocation policies

//capacity allocator

struct uniform_distribution
{
	int node_no;
	long int request_rate;
	int mapped_pool_no;
};

vector<uniform_distribution>udp;
vector<uniform_distribution>udp_pool_set[num_mem_pools];


bool compare_by_request_rate(const uniform_distribution &a, const uniform_distribution &b)
{
	return a.request_rate > b.request_rate;
}


long int subset_sum(int pool)
{
	long int subset_size=0;
	for(auto j=0;j<udp_pool_set[pool].size();j++)
	{
			subset_size+=udp_pool_set[pool][j].request_rate;
	}
	return subset_size;
}

int find_min_set()
{
	int min=num_mem_pools-1;
	long int subset_size_min=0;

	subset_size_min=subset_sum(min);

	for(int i=0;i<num_mem_pools;i++)
	{
		long int subset_size=subset_sum(i);
		
		if(subset_size<subset_size_min)
			min=i;
	}

	return min;
}

int uni_distribution(int node_no)
{
	int remote_pool= udp[node_no].mapped_pool_no;
	if(remote_pool==-2)
	{
		remote_pool=round_robin_pool_select();
	}
	out<<"\n\n=======Cycle-"<<common_clock<<"========\n";
	for(int i=0;i<num_mem_pools;i++)
	{
		out<<"\npool-"<<i<<"  current_access_count :"<<R[i].get_count()<<"  total_access_count :"<<_total_access_count[i];
	}
	out<<"\nSelected pool is- "<<remote_pool;
	return remote_pool;
}

void uniform_load_distribution()
{
	long int aggr_request_rate=0;
	long int max_request_rate_per_pool=0;

	vector <uniform_distribution>udp_set;
	// sort(udp.begin(),udp.end(),compare_by_request_rate);

	for(int i=0;i<num_nodes;i++)
	{
		if(udp[i].request_rate>0)
		{
			aggr_request_rate+=udp[i].request_rate;
			udp_set.push_back(udp[i]);
			// track<<"\nnode: "<<i<<"  rq_rate: "<<udp[i].request_rate;
		}
	}

	sort(udp_set.begin(),udp_set.end(),compare_by_request_rate);

	for(int i=0;i<udp_set.size();i++)
	{
		// track<<"\nudp_set:"<<i;
		out<<"\nnode: "<<udp_set[i].node_no<<" rq_rate: "<<udp_set[i].request_rate<<" pool: "<<udp_set[i].mapped_pool_no;
	}

	max_request_rate_per_pool=aggr_request_rate/num_mem_pools;

	int set_size=udp_set.size();

	if(set_size<0)
		return;

	
	for(auto it=udp_set.begin();it!=udp_set.end();it++)
	{
		int min_set = find_min_set();

		udp_pool_set[min_set].push_back(*it);

	}

	for(int i=0;i<num_mem_pools;i++)
	{
		out<<"\npool: "<<i<<" enteries ";
		for(int j=0;j<udp_pool_set[i].size();j++)
		{
			out<<"\t"<<udp_pool_set[i][j].node_no;
		}
	}

	udp_set.clear();

	for(int i=0;i<num_mem_pools;i++)
	{
		for(int j=0;j<udp_pool_set[i].size();j++)
		{
			int node_no=udp_pool_set[i][j].node_no;
			udp[node_no].mapped_pool_no=i;
		}
		udp_pool_set[i].clear();
	}


	for(int i=0;i<num_nodes;i++)
	{
		out<<"\nnode: "<<i<<"\tpool: "<<udp[i].mapped_pool_no;
	}
	// cin.get();

}