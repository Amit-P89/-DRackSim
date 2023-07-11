#include<iostream>
#include<fstream>
#include<vector>
#include<string>
#include<algorithm>
#include<pthread.h>

using namespace std;

struct Trace
{
	int procid;
    int threadid;
    unsigned long long addr;
    char r;
    unsigned long penalty; 
    unsigned long cycle;
    int block_to_fetch;
};
struct mem_stream
{
	int procid;
    int threadid;
    unsigned long long addr;
    char r;
    unsigned long long cycle;
};

struct args 
{
    int node_id;
    char Text_Trace;
};

bool min_cycle(const Trace &a, const Trace &b)
{
    return a.cycle < b.cycle ;
}

void *parse(void *t_data)
{
	struct args *data=(struct args *)t_data;
	cout<<data->node_id;
	vector<Trace> tra;
	fstream trace;
	ofstream trace1,trace_out;
	string in,out;
	in="Output/Node"+to_string(data->node_id)+"/TraceFile.trc";
	out="Trace_Node"+to_string(data->node_id)+".trc";
	trace.open(in);
	if(data->Text_Trace=='y' || data->Text_Trace=='Y')
		trace_out.open(out);

	while(1)
	{
		Trace temp;
		trace.read((char*)&temp,sizeof(temp));
		if(!trace.eof())
			{
				tra.push_back(temp);
			}
		else
			break;
	}
	
	remove(in.c_str());
	trace1.open(in);
	sort(tra.begin(),tra.end(),min_cycle);
	
	for(int j=0;j<tra.size();j++)
	{
//starting address of block for block-size 64 B, required to update for different LLC block-size
		unsigned long long block_start_addr=tra[j].addr & 0xffffffffffc0;
		
		//trace1.write((char*)&tra[j],sizeof(tra[j]));
		mem_stream temp;
		temp.procid=tra[j].procid;
		temp.threadid=tra[j].threadid;
		temp.r=tra[j].r;
		temp.cycle=tra[j].cycle;

		int block_fetch=tra[j].block_to_fetch;
		
		if(block_fetch==0 || block_fetch==1)
		{
			if(block_fetch==1)
			{
				uint64_t lineSize=64;
				const uint64_t notLineMask = ~(lineSize - 1);
				block_start_addr = (block_start_addr & notLineMask) + lineSize;
			}

			temp.addr=block_start_addr;
			trace1.write((char*)&temp,sizeof(temp));

			if(data->Text_Trace=='y' || data->Text_Trace=='Y')
				trace_out<<tra[j].procid<<"  "<<tra[j].threadid<<"  "<<hex<<"0x"	\
				<<block_start_addr<<"  "<<tra[j].r<<"  "<<dec<<tra[j].cycle<<"  "<<"\n";
			
		}

		else if(tra[j].block_to_fetch==2)
		{
			for(int i=0;i<2;i++)
			{
				temp.addr=block_start_addr;
				trace1.write((char*)&temp,sizeof(temp));
				
				if(data->Text_Trace=='y' || data->Text_Trace=='Y')
					trace_out<<tra[j].procid<<"  "<<tra[j].threadid<<"  "<<hex<<"0x"	\
					<<block_start_addr<<"  "<<tra[j].r<<"  "<<dec<<tra[j].cycle<<"  "<<tra[j].block_to_fetch<<"\n";
				
				uint64_t lineSize=64;
				const uint64_t notLineMask = ~(lineSize - 1);
				block_start_addr = (block_start_addr & notLineMask) + lineSize;
			}
		}
	}
	trace1.close();
	trace.close();
	trace_out.close();
	pthread_exit(NULL);	
}

int main()
{
	int node_id=0;int end_id=0;
	cout<<"\nInput the node number for which the Traces need to be Parsed\t:";
	cin>>node_id;
	struct args t_data;
	char Text_Trace;
	cout<<"Press 'y' if you want to generate Text Trace\t:";
	cin>>Text_Trace;

	pthread_t threads;

	t_data.Text_Trace=Text_Trace;
	t_data.node_id=node_id;
	pthread_create(&threads, NULL, parse, &t_data);

	pthread_exit(NULL);	

//	parse(1);


	return 0;
}
