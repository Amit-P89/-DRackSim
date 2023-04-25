#include<math.h>
#include<iomanip>
#include<bitset>
#include<fstream>

ofstream out,mem_stats,invalid,netstats;

#define Page_Size		4096	//in bytes

class remote_addr_space;


//maintains local-to-remote mapping at each node 
class node_remote_map
{
public:
	unsigned long local_base;	//base address at local node address space
	unsigned long remote_base;	//remote base address at remote pool address space
	unsigned long offset_mask;	//unused
	int region_size;			//size of the region assigned from remote to local 
								//(multiple enteries can be there for one node to same remote pool)
	int mem_pool_no;			//remote pool number at which memory is reserved
};

class local_addr_space
{
	long double memory_size;		//in Giga-Bytes
	long double remote_mem_size;	//total remote memory reserved
	unsigned long total_pages;		//local+remote pages
	unsigned long allocated_pages;	//used pages
	unsigned long local_allocated_pages;	//used local pages
	unsigned long remote_allocated_pages;	//used remote pages
	unsigned long free_pages;				//free pages in all the memory
	unsigned long local_pages;				//
	unsigned long remote_pages;
	int node_no;
	node_remote_map *remote_map;	//node maintain local-remote mapping table
	int remote_map_index;

public:
	local_addr_space(){}

	//add mem_size(GB) amount of local memory into the address space
	void add_local_memory(long double mem_size, int node_num)
	{
		remote_map_index=-1;
		node_no=node_num;
		memory_size=mem_size;
		total_pages=(pow(2.0,30.0) * memory_size) / 4096 ;
		local_pages=total_pages;
		remote_pages=0;
		allocated_pages=0;
		local_allocated_pages=0;
		remote_allocated_pages=0;
		free_pages=total_pages;
		remote_mem_size=0;
		cout<<fixed;
		cout<<"\nLocal Mem_Size= "<<memory_size<<" GB"<<" in node number "<<node_no;
		cout<<"\nNumbee of pages= "<<total_pages<<endl;
	}

	~local_addr_space()
	{
		if(remote_map_index>=0)
			delete[] remote_map;
	}

	//check if free(local or remote) memory is available
	unsigned long has_free()
	{
		if(free_pages>0)
		{
			return free_pages;
		}
		else
			return 0;
	}

	//check if free-local memory is available
	bool free_local()
	{
		if(local_pages > local_allocated_pages)
			return true;
		else
			return false;
	}

	//check if free-local memory is available
	bool free_remote()
	{
		if(remote_pages > remote_allocated_pages)
			return true;
		else
			return false;
	}

	//check if a page is local or not
	bool is_local(unsigned long page_no)
	{
		if(page_no<local_pages)
			return 1;
		else
			return 0;
	}

	//allocates a local page of memory
	long int allocate_local_page()
	{
		local_allocated_pages++;
		allocated_pages++;
		free_pages--;
		return (local_allocated_pages-1);
	}

	//allocates a remote page of memory
	long int allocate_remote_page()
	{
		remote_allocated_pages++;
		allocated_pages++;
		free_pages--;
		return ((remote_allocated_pages + local_pages)-1);
	}


	//find the remote pool at which address #paddr is allocated
	int find_remote_pool(unsigned long paddr)
	{
		for(int i=0;i<=remote_map_index;i++)
		{
			if(paddr>=remote_map[i].local_base && paddr<=(remote_map[i].local_base+remote_map[i].region_size-1))
			{
				return remote_map[i].mem_pool_no;
			}
		}

		return -1;
	}

	//find the remote page address of #paddr in #remote-pool
	long int remote_page_addr(unsigned long paddr, int remote_pool)
	{
		long int remote_pool_page_no=-1;
		for(int i=0;i<=remote_map_index;i++)
		{
			unsigned long memory_pool_limit=remote_map[i].local_base+remote_map[i].region_size-1;
			if(remote_pool==remote_map[i].mem_pool_no && memory_pool_limit>=paddr)
			{
				int regional_page_no = paddr - remote_map[i].local_base;
				if(regional_page_no <= remote_map[i].region_size)
				{
					remote_pool_page_no = remote_map[i].remote_base + regional_page_no;
				}
				else
					cout<<"\nWrong memory pool info.\n";
				break;
			}
		}
		return remote_pool_page_no;
	}

	void get_stats()
	{
		mem_stats<<"\n--------------------------------------------------------------------";
		mem_stats<<"\nMemory Size               			:"<<memory_size<<"GB";
		mem_stats<<"\n\tLocal Memory Size			:"<<(memory_size - remote_mem_size)<<"GB";
		mem_stats<<"\n\tRemote Memory Size			:"<<remote_mem_size<<"GB";
		mem_stats<<"\nLocal Memory Used        			:"<<(((local_allocated_pages)*4096)/pow(2.0,30.0))<<"GB";
		mem_stats<<"\nRemote Memory Used        			:"<<((remote_allocated_pages*4096)/pow(2.0,30.0))<<"GB";
		mem_stats<<"\nTotal Pages in memory     			:"<<total_pages;
		mem_stats<<"\n\tTotal local pages 			 :"<<local_pages;
		mem_stats<<"\n\tTotal remote pages 		 	:"<<remote_pages;
		mem_stats<<"\nTotal Allocated Pages 				:"<<allocated_pages;
		mem_stats<<"\n\tTotal local allocated pages 		:"<<local_allocated_pages;
		mem_stats<<"\n\tTotal remote allocated pages 		:"<<remote_allocated_pages;
		mem_stats<<"\nFree Pages left 				:"<<free_pages;
		mem_stats<<"\n\tTotal local free pages 			:"<<(local_pages - local_allocated_pages);
		mem_stats<<"\n\tTotal remote free pages 		:"<<(remote_pages - remote_allocated_pages);
		mem_stats<<"\n------------------------------------------------------------------\n";
	}


	//add entry for local-remote mapping whenever remote memory is requested 
	void add_remote_memory_entry(unsigned long local_base, unsigned long remote_base, unsigned long mask, unsigned long size, int mem_pool)
	{
		remote_map_index++;
		if(remote_map_index==0)
		{
			remote_map= new node_remote_map[1];
		}
		else
		{
			node_remote_map *new_table=new node_remote_map[remote_map_index+1];
			copy(remote_map, remote_map + min(remote_map_index, remote_map_index+1), new_table);
			delete[] remote_map;
			remote_map=new_table;
		}
		remote_map[remote_map_index].local_base=local_base;
		remote_map[remote_map_index].remote_base=remote_base;
		remote_map[remote_map_index].offset_mask=mask;
		remote_map[remote_map_index].region_size=size;
		remote_map[remote_map_index].mem_pool_no=mem_pool;
	}

	//display all the local-remote mappings at this node
	void display_mapping(ofstream & mem_stats)
	{
		if(remote_map_index>=0)
		{
			mem_stats<<"\n\t\t\t\t\t\tNode Remote-Memory Mapping Table Node-"<<node_no+1<<endl;
			mem_stats<<"\n\t\tS.No.\t local_base\t remote_base\t mask\t size\t num_pages\t mem_pool\n";
			mem_stats<<"\t\t\t-----------------------------------------------------------------------------";
			for(int i=0;i<=remote_map_index;i++)
			{
/*				mem_stats<<"\n\t\t\t     "<<remote_map[i].local_base<<"\t\t      "<<remote_map[i].remote_base \
				<<"\t\t  "<<remote_map[i].offset_mask<<"\t"<<remote_map[i].region_size \
				<<"\t    "<<remote_map[i].mem_pool_no;*/

				mem_stats<<fixed<<"\n"<<setw(15)<<i+1<<fixed<<setw(10)<<"0x"<<hex<<remote_map[i].local_base<<fixed<<setw(12)<<"0x"	\
				<<remote_map[i].remote_base <<fixed<<setw(11)<<dec<<remote_map[i].offset_mask<<fixed<<setw(8)<<	\
				(remote_map[i].region_size*4)/1024<<"MB"<<fixed<<setw(10)<<remote_map[i].region_size<<fixed<<	\
				setw(10)<<remote_map[i].mem_pool_no;

			}
			mem_stats<<"\n\t\t\t-----------------------------------------------------------------------------\n";
		}
		else
		{
			mem_stats<<"\nNo entry yet";
		}
	}

	void pool_wise_page_count(int num_pools)
	{
		unsigned long pools[num_pools]={0};
		for(int i=0;i<=remote_map_index;i++)
		{
			int pool_no=remote_map[i].mem_pool_no;
			pools[pool_no]=pools[pool_no]+1024;
		}
		unsigned long pages_left_in_last_shared_region= remote_pages - (remote_allocated_pages);// - local_pages);
		
		if(remote_map_index>=0)
		{	
			int last_pool=remote_map[remote_map_index].mem_pool_no;
			pools[last_pool]=pools[last_pool] - pages_left_in_last_shared_region;
		}
		
		for(int i=0;i<num_pools;i++)
		{
			if(pools[i]>0)
				mem_stats<<"\nPages in remote-pool-"<<i<<" :"<<pools[i];
		}

	}

	//request #new-mem (MBs) amount of memory from a remote pool
	friend void request_remote_memory(local_addr_space &,remote_addr_space &, long double new_mem); //add in Mega-Bytes

};


//used to maintain mappings of shared regions in a memory pools to different nodes
class remote_shared_map
{
public:
	unsigned long base_page_no;
	unsigned long limit;
	int node_no;
};

class remote_addr_space
{
	long double memory_size;	//in Giga-Bytes
	unsigned long total_pages;
	unsigned long free_pages;
	unsigned long allocated_pages;
	remote_shared_map *shared_mem_table;	//central memory manager make table enteries at each memory pool 
											//for keeping a record of shared memory with any node
	long int shared_mem_table_index;
	int mem_pool_no;
	long double access_count;

public:

	remote_addr_space(){}

	//add mem_size(GB) amount of memory at remote pool
	void add_remote_memory_pool(long double mem_size, int pool_no)
	{
		access_count=0;
		mem_pool_no=pool_no;
		memory_size=mem_size;
		total_pages=(pow(2.0,30.0) * memory_size) / (4096);
		free_pages=total_pages;
		allocated_pages=0;
		shared_mem_table_index=-1;
		//cout<<"\nRemote Memory size ="<<mem_size<<" GB in memory pool no "<<mem_pool_no;
		//cout<<"\nNumber of Pages ="<<total_pages;	
	}
	~remote_addr_space()
	{
		if(shared_mem_table_index>=0)
			delete[] shared_mem_table;
	}

	//maintain access count at each remote pool, in a particular window defined while reading memory accesses
	void inc_access_count()
	{
		access_count++;
	}

	//check if free remote memory is available to allocate a chunk from
	unsigned long has_free(long double mem_shared)
	{
		unsigned long new_pages=(pow(2.0,20.0) * mem_shared) / 4096;
		if(free_pages>=new_pages)
		{
			return 1;
		}
		else
			return 0;
	}

	unsigned long mem_left()
	{
		return free_pages;
	}

	//reset access count after the window
	void reset_count()
	{
		access_count=0;
	}

	unsigned long get_count()
	{
		return access_count;
	}

	//validate an memory access address when a node is trying to access it
	bool validate_addr(unsigned long paddr, int node_no)
	{
		bool valid=false;
		for(long int i=0;i<=shared_mem_table_index;i++)
		{
			unsigned long Last_page_no=shared_mem_table[i].base_page_no+shared_mem_table[i].limit;
			if(shared_mem_table[i].node_no==node_no && paddr>=shared_mem_table[i].base_page_no && paddr<Last_page_no)
			{
				valid=true;
				break;	
			}
		}
		return valid;
	}

	//add shared memory mapping entry at memory pool with its region size and base address
	void add_shared_memory_entry(unsigned long base, unsigned long size, int node)
	{
		shared_mem_table_index++;
		if(shared_mem_table_index==0)
		{
			shared_mem_table= new remote_shared_map[1];
		}
		else
		{
			remote_shared_map *new_table=new remote_shared_map[shared_mem_table_index+1];
			copy(shared_mem_table,shared_mem_table + min(shared_mem_table_index, shared_mem_table_index+1), new_table);
			delete[] shared_mem_table;
			shared_mem_table=new_table;
		}
		shared_mem_table[shared_mem_table_index].base_page_no=base;
		shared_mem_table[shared_mem_table_index].limit=size;
		shared_mem_table[shared_mem_table_index].node_no=node;
	}

	//displat all shared memory region mappings of a remote memory pool
	void display_mapping()
	{
		if(shared_mem_table_index>=0)
		{
			mem_stats<<"\n\t\t\t\t\t Remote-Memory Mapping Pool "<<mem_pool_no<<endl;
			mem_stats<<"\n\t\t\t\t\tbase_addr     limit    Node_no\n";
			mem_stats<<"\t\t\t\t\t==============================";
			for(long int i=0;i<=shared_mem_table_index;i++)
			{
				mem_stats<<"\n\t\t\t\t\t    "<<shared_mem_table[i].base_page_no<<"\t\t\b\b"<<shared_mem_table[i].limit<<"\t\t  "<<shared_mem_table[i].node_no;
			}
			mem_stats<<"\n\t\t\t\t\t==============================\n";
		}
		else
		{
			mem_stats<<"\nNo entry yet";
		}
	}

	int num_shared_regions()
	{
		if(shared_mem_table_index==-1)
			return 1;
		else
			return (shared_mem_table_index+1);
	}

	void get_stats()
	{
		invalid<<"\n-------------------------------------------";
		invalid<<"\nMemory Size "<<memory_size<<"GB";
		invalid<<"\nTotal Pages in memory "<<total_pages;
		invalid<<"\nTotal Allocated Pages "<<allocated_pages;
		invalid<<"\nFree Pages left "<<free_pages;
		invalid<<"\n-------------------------------------------\n";	
	}

	//request memory from a remote pool by a local node(in MBs)
	friend void request_remote_memory(local_addr_space &,remote_addr_space &, long double new_mem);

};

//add in Mega-Bytes	
void request_remote_memory(local_addr_space &L, remote_addr_space &R, long double mem_shared)
{
	//cout<<"\n-------------------------------------------";
	unsigned long new_pages=(pow(2.0,20.0) * mem_shared) / 4096;
	if(R.free_pages>=new_pages)
	{

		L.add_remote_memory_entry(L.total_pages,R.allocated_pages,12,new_pages,R.mem_pool_no);
		L.total_pages=L.total_pages+new_pages;
		L.free_pages=L.free_pages+new_pages;
		L.remote_pages=L.remote_pages+new_pages;
		//cout<<"\nNew total_pages "<<L.total_pages;
		L.memory_size=L.memory_size+(mem_shared/1024);
		L.remote_mem_size=L.remote_mem_size+(mem_shared/1024);

		R.add_shared_memory_entry(R.allocated_pages,new_pages,L.node_no);
		R.allocated_pages=R.allocated_pages+new_pages;
		R.free_pages=R.free_pages-new_pages;

		//cout<<"\n"<<new_pages<<" new pages added to local from remote";
	}

	else
	{
		invalid<<"\n\nRemote memory not allocated, no free page left in remote memory pool-"<<(R.mem_pool_no+1);
		R.get_stats();
	}

	//cout<<"\n-------------------------------------------\n";
};

/*int main()
{

local_addr_space L(0.00006103515625,1);
remote_addr_space R(4,1);

for(int i=0;i<100;i++)
{
	if(L.has_free())
	{
		L.allocate_page();
	}
	else
	{	
		request_remote_memory(L,R,4);
		if(L.has_free())
		{
			L.allocate_page();
		}
	}
}
	cout<<"\n";
	local_addr_space p(3,1);
	local_addr_space q(1,2);
	remote_addr_space m(4,1);
	cout<<"\nalloc  p-"<<p.allocate_page();
	cout<<"\nalloc  q-"<<q.allocate_page();
	cout<<"\nalloc  p-"<<p.allocate_page();
	request_remote_memory(p,m,4);
	request_remote_memory(q,m,4);
	p.display_mapping();
	q.display_mapping();
	m.display_mapping();
	cout<<"\nalloc p-"<<p.allocate_page();
	cout<<"\nalloc p-"<<p.allocate_page();
	cout<<"\nalloc q-"<<q.allocate_page();
	cout<<"\nalloc q-"<<q.allocate_page();
	request_remote_memory(p,m,4);
	p.display_mapping();
	m.display_mapping();
	request_remote_memory(q,m,4);
	q.display_mapping();
	m.display_mapping();
	cout<<"\nalloc q-"<<q.allocate_page();
	cout<<"\nalloc q-"<<q.allocate_page();
	cout<<"\nalloc q-"<<q.allocate_page();
	cout<<"\nalloc q-"<<q.allocate_page();
	cout<<"\nalloc p-"<<p.allocate_page();
	cout<<"\nalloc p-"<<p.allocate_page();
	cout<<"\nalloc p-"<<p.allocate_page();
	return 0;
}*/