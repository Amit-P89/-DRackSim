#include <fstream>

// both tx/rx, for all packet sizes, assuming enough memory
#define nic_queue_size 16384 * 64 // in bytes

// buffer sizes for Rx/tx, divided according to the total buffer size of the switch
//(DELL POWERSWITCH Z9432F-ON), allows 128 ports of 100GbE(used in this simulation), or 32-ports of 400GbE
// has buffer size of 132MB,

double per_port_mb = ceil((double)132 / (num_nodes + num_mem_pools));

long double tx_per_port_mb = (per_port_mb / 2);
long double rx_per_port_mb = (per_port_mb / 2);

long double tx_input_port_queue_size = ((tx_per_port_mb * 1024 * 1024)); // 64-byte packet while sending
long double rx_input_port_queue_size = ((rx_per_port_mb * 1024 * 1024)); // 128-byte packet while receiving
long double tx_output_port_queue_size = ((tx_per_port_mb * 1024 * 1024));
long double rx_output_port_queue_size = ((rx_per_port_mb * 1024 * 1024));

#define nic_bandwidth 100	 // Gbps
#define switch_bandwidth 400 // Gbps


//all delays in nano-seconds (with respect to CPU of 3.6 GHz)

//all latency parameters are used accordingly to map the interconnect latency of genZ
//on top of that our queue latency model will add more latency as per the traffic
int packetisation_time = ceil(12*CPU_Freq);

int nic_trans_delay = 0;
int switch_trans_delay = 0;

#define nic_proc_delay (4 * freq_ratio)
#define switch_proc_delay (4 * freq_ratio)
#define prop_delay (5*CPU_Freq) // max 1-meter inside rack
#define switching_delay (1 * freq_ratio)

//switch input port arbitrator (transmitting/receiving)
static int tx_arbitrator = 0;
static int rx_arbitrator = 0;

// arbitrator for virtual queues at input port(transmitting/receiving)
int static tx_vq_arbitrator_input_queue[num_nodes] = {0};
int static rx_vq_arbitrator_input_queue[num_mem_pools] = {0};

// a variable to identify flow of network request (0: node->pool) (1: pool->node)
bool network_flow = 0;

// infinite queue at source after link_layer to collect all packets in time-ordered manner from all (CPUs-Node side)/(Memory-Pool side)
deque<packet> packet_queue_node[num_nodes];	   // transmitting queue
deque<packet> rx_packet_queue_node[num_nodes]; // receiveing queue

// NIC queue at node with size nic_queue_size
deque<packet> tx_nic_queue_node[num_nodes];
deque<packet> rx_nic_queue_node[num_nodes];

// virtual queues at switch
deque<packet> tx_input_port_queue[num_nodes][num_mem_pools];
deque<packet> rx_input_port_queue[num_mem_pools][num_nodes];

// output queues at switch
deque<packet> tx_output_port_queue[num_mem_pools];
deque<packet> rx_output_port_queue[num_nodes];

////NIC queue at pool with size nic_queue_size
deque<packet> tx_nic_queue_pool[num_mem_pools];
deque<packet> rx_nic_queue_pool[num_mem_pools];

// unlimited queue at memory-pool after link_layer to collect all packets
deque<packet> packet_queue_pool[num_mem_pools]; // transmitting queue

// unlimited queue at pool after NIC
// deque <packet> packet_queue_pool[num_mem_pools];

void to_network_layer(packet, deque<packet> *);
void to_link_layer(packet, deque<packet> *);
void simulate_network();
void simulate_network_reverse();
void send_to_source_nic_queue(deque<packet> *, deque<packet> *, int);
void source_to_switch_input_port(deque<packet> *, deque<packet> *, int, int, int);
void switch_input_port_to_output_port(deque<packet> *, deque<packet> *, int, int, int &, int *, int, int);
void switch_to_dest_nic_queue(deque<packet> *, deque<packet> *, int);
void send_to_dest_collector_queue(deque<packet> *, deque<packet> *, int);

void to_trans_layer(remote_memory_access m, deque<packet> *packet_queue)
{
	packet temp;
	temp.is_transmitting = 0;
	temp.is_processing = 0;
	temp.in_nic_source = -1;
	temp.out_nic_source = -1;
	temp.in_switch_input_port = -1;
	temp.out_switch_input_port = -1;
	temp.in_switch_output_port = -1;
	temp.out_switch_output_port = -1;
	temp.in_nic_dest = -1;
	temp.out_nic_dest = -1;
	temp.mem = m;
	temp.mem.cycle = m.cycle + packetisation_time/2; // added packetization time
	to_network_layer(temp, packet_queue);
}

void to_network_layer(packet p, deque<packet> *packet_queue)
{
	p.mem.cycle = p.mem.cycle + packetisation_time/2; // added packetization time
	to_link_layer(p, packet_queue);
}

void to_link_layer(packet p, deque<packet> *packet_queue)
{
	// pthread_mutex_lock(&lock_queue);
	packet_queue[p.mem.source].push_back(p);
	// pthread_mutex_unlock(&lock_queue);
}

// checks it queue size is available for forwarding new packet
bool has_queue_size(deque<packet> *queue_source, int id, int current_packet_size)
{
	long size = 0;
	for (int i = 0; i < queue_source[id].size(); i++)
	{
		// int current_packet_size = 0;
		if (queue_source[id][i].mem.trans_id>1e8)
			size += RDMA_PKT_SIZE*(network_flow) + 64 * (!network_flow);
		else if (queue_source[id][i].mem.trans_id == 0)
			size += 128 * (!network_flow) + 64 * (network_flow);
		else if (queue_source[id][i].mem.trans_id != 0)
			size += 64 * (!network_flow) + 128 * (network_flow);
	}

	if ((size + current_packet_size) <= nic_queue_size)
		return true;
	else
		return false;
}

void send_to_source_nic_queue(deque<packet> *packet_queue_source, deque<packet> *nic_queue_source, int source_count)
{
	for (int i = 0; i < source_count; i++)
	{
		if (packet_queue_source[i].size() > 0)
		{
			int current_packet_size = 0;
			if (packet_queue_source[i].front().mem.trans_id == 0)
				current_packet_size = 128 * (!network_flow) + 64 * (network_flow);
			else if (packet_queue_source[i].front().mem.trans_id > 1e8)
				current_packet_size = RDMA_PKT_SIZE*(network_flow) + 64 * (!network_flow);
			else
				current_packet_size = 64 * (!network_flow) + 128 * (network_flow);

			bool has_size = has_queue_size(nic_queue_source, i, current_packet_size);
			if (packet_queue_source[i].front().mem.cycle == common_clock && has_size)
			{
				if (packet_queue_source[i].front().is_processing > 0)
				{
					if (packet_queue_source[i].front().is_processing == nic_proc_delay)
					{
						packet_queue_source[i].front().in_nic_source = packet_queue_source[i].front().mem.cycle;
						packet_queue_source[i].front().mem.cycle = packet_queue_source[i].front().mem.cycle ;
						packet_queue_source[i].front().is_processing = 0;
						nic_queue_source[i].push_back(packet_queue_source[i].front());
						// pthread_mutex_lock(&lock_queue);
						packet_queue_source[i].pop_front();
						// pthread_mutex_unlock(&lock_queue);
						for (long long int j = 0; j < packet_queue_source[i].size(); j++)
						{
							if (packet_queue_source[i].at(j).mem.cycle == common_clock)
								packet_queue_source[i].at(j).mem.cycle++;
							else if (packet_queue_source[i].at(j).mem.cycle > common_clock)
								break;
						}
					}
					else if (packet_queue_source[i].front().is_processing < nic_proc_delay)
					{
						packet_queue_source[i].front().is_processing++;
						for (long long int j = 0; j < packet_queue_source[i].size(); j++)
						{
							if (packet_queue_source[i].at(j).mem.cycle == common_clock)
								packet_queue_source[i].at(j).mem.cycle++;
							else if (packet_queue_source[i].at(j).mem.cycle > common_clock)
								break;
						}
					}
				}
				else if (packet_queue_source[i].front().is_processing == 0)
				{
					packet_queue_source[i].front().is_processing++;
					for (long long int j = 0; j < packet_queue_source[i].size(); j++)
					{
						if (packet_queue_source[i].at(j).mem.cycle == common_clock)
							packet_queue_source[i].at(j).mem.cycle++;
						else if (packet_queue_source[i].at(j).mem.cycle > common_clock)
							break;
					}
				}
			}
			else if (!has_size)
			{
				for (long long int j = 0; j < packet_queue_source[i].size(); j++)
				{
					if (packet_queue_source[i].at(j).mem.cycle == common_clock)
						packet_queue_source[i].at(j).mem.cycle++;
					else if (packet_queue_source[i].at(j).mem.cycle > common_clock)
						break;
				}
			}
		}
	}
}

// check if space at input port queue
template <size_t rows, size_t cols>
bool virtual_queue_has_space(int port_no, int dest_count, deque<packet> (&input_port_queue)[rows][cols], int input_port_queue_size, int current_packet_size)
{
	int size = 0;

	for (int i = 0; i < dest_count; i++)
	{
		for (int j = 0; j < input_port_queue[port_no][i].size(); j++)
		{
			if (input_port_queue[port_no][i][j].mem.trans_id == 0)
				size = size + 128 * (!network_flow) + 64 * (network_flow);
			else if (input_port_queue[port_no][i][j].mem.trans_id > 1e8)
				size = size + RDMA_PKT_SIZE * (network_flow) + 64 * (!network_flow);
			else
				size = size + 64 * (!network_flow) + 128 * (network_flow);
		}
	}
// if(current_packet_size>0)
// {
// cout<<" \n nic qs "<<input_port_queue_size<<" size "<<size<<" current_packet_size "<<current_packet_size;
// cin.get();
// }
	if ((size + current_packet_size) <= input_port_queue_size)
		return true;
	else
		return false;
}

template <size_t rows, size_t cols>
void source_to_switch_input_port(deque<packet> *nic_queue_source, deque<packet> (&input_port_queue)[rows][cols], int source_count, int dest_count, int input_port_queue_size)
{
	for (int i = 0; i < source_count; i++)
	{
		int current_packet_size = 0;
		// if space available at switch input port, select top packet from the node nic queue and transmit to switch.
		// next packet will wait until current packet is transmitted, appropriate cycles added to all waiting packets
		// propagation delay inside RACK is 5ns (max 1 meter assumed)
		if (nic_queue_source[i].size() > 0)
		{
			if (nic_queue_source[i].front().mem.trans_id == 0)
			{
				current_packet_size = 128 * (!network_flow) + 64 * (network_flow);
			}
			else if (nic_queue_source[i].front().mem.trans_id > 1e8)
			{
				current_packet_size = RDMA_PKT_SIZE * (network_flow) + 64 * (!network_flow);
			}
			else
			{
				current_packet_size = 64 * (!network_flow) + 128 * (network_flow);
			}

			if (nic_queue_source[i].front().mem.cycle == common_clock && virtual_queue_has_space(i, dest_count, input_port_queue, input_port_queue_size, current_packet_size))
			{
				nic_trans_delay = (current_packet_size * 8) / nic_bandwidth;
				if (nic_queue_source[i].front().is_transmitting > 0)
				{
					if ((nic_queue_source[i].front().is_transmitting == nic_trans_delay))
					{
						nic_queue_source[i].front().out_nic_source = nic_queue_source[i].front().mem.cycle ;
						nic_queue_source[i].front().mem.cycle = nic_queue_source[i].front().mem.cycle + prop_delay + switch_proc_delay;
						nic_queue_source[i].front().in_switch_input_port = nic_queue_source[i].front().mem.cycle - switch_proc_delay;
						int vq_no = nic_queue_source[i].front().mem.dest;
						nic_queue_source[i].front().is_transmitting = 0;
						input_port_queue[i][vq_no].push_back(nic_queue_source[i].front());
						nic_queue_source[i].pop_front();
						for (long long int j = 0; j < nic_queue_source[i].size(); j++)
						{
							if (nic_queue_source[i].at(j).mem.cycle == common_clock)
								nic_queue_source[i].at(j).mem.cycle++;
							else if (nic_queue_source[i].at(j).mem.cycle > common_clock)
								break;
						}
					}
					else if (nic_queue_source[i].front().is_transmitting < nic_trans_delay)
					{
						nic_queue_source[i].front().is_transmitting++;
						for (long long int j = 0; j < nic_queue_source[i].size(); j++)
						{
							if (nic_queue_source[i].at(j).mem.cycle == common_clock)
								nic_queue_source[i].at(j).mem.cycle++;
							else if (nic_queue_source[i].at(j).mem.cycle > common_clock)
								break;
						}
					}
				}
				else if (nic_queue_source[i].front().is_transmitting == 0)
				{
					nic_queue_source[i].front().is_transmitting = 1;
					for (long long int j = 0; j < nic_queue_source[i].size(); j++)
					{
						if (nic_queue_source[i].at(j).mem.cycle == common_clock)
							nic_queue_source[i].at(j).mem.cycle++;
						else if (nic_queue_source[i].at(j).mem.cycle > common_clock)
							break;
					}
				}
			}
		}

		if (!virtual_queue_has_space(i, dest_count, input_port_queue, input_port_queue_size, current_packet_size))
		{
			for (long long int j = 0; j < nic_queue_source[i].size(); j++)
			{
				if (nic_queue_source[i].at(j).mem.cycle == common_clock)
					nic_queue_source[i].at(j).mem.cycle++; // increament the cycle number of all packets waiting in the queue
				else if (nic_queue_source[i].at(j).mem.cycle > common_clock)
					break;
			}
		}
	}
}

// this will tell the maximum number of packets that can be taken from input to output buffer in one cycle
void find_free_buffers_at_output_ports(int &count, int *output_queue_status, int dest_count, deque<packet> *output_port_queue, int output_port_queue_size)
{
	for (int i = 0; i < dest_count; i++)
	{
		long size = 0;
		for (int j = 0; j < output_port_queue[i].size(); j++)
		{
			if (output_port_queue[i][j].mem.trans_id == 0)
				size += 128 * (!network_flow) + 64 * (network_flow);
			else if (output_port_queue[i][j].mem.trans_id > 1e8)
				size += RDMA_PKT_SIZE * (network_flow) + 64 * (!network_flow);
			else
				size += 64 * (!network_flow) + 128 * (network_flow);
		}
		if ((size + RDMA_PKT_SIZE) <= output_port_queue_size)
		{
			output_queue_status[i] = 3;
			count++;
		}
		else if ((size + 128) <= output_port_queue_size)
		{
			output_queue_status[i] = 2;
			count++;
		}
		else if ((size + 64) <= output_port_queue_size)
		{
			output_queue_status[i] = 1;
			count++;
		}
	}
}

// count all the ready packets in all the virtual queues of all the ports
template <size_t rows, size_t cols>
int total_ready_packets(int source_count, int dest_count, deque<packet> (&input_port_queue)[rows][cols])
{
	int ready_packets = 0;
	for (int i = 0; i < source_count; i++)
	{
		for (int j = 0; j < dest_count; j++)
		{
			if (input_port_queue[i][j].size() > 0)
			{
				if (input_port_queue[i][j].front().mem.cycle == common_clock)
					ready_packets++;
			}
		}
	}
	return ready_packets;
}

// use arbitrator logic for multiple input ports as well as within virtual queues inside each input port
template <size_t rows, size_t cols>
void switch_input_port_to_output_port(deque<packet> (&input_port_queue)[rows][cols], deque<packet> *output_port_queue,
									  int source_count, int dest_count, int &arbitrator, int *vq_arbitrator_input_queue, int input_port_queue_size, int output_port_queue_size)
{
	// only those packets which are ready can be sent towards output port (max. packets are equal to num of output ports with free queue space)
	int ready_packets = total_ready_packets(source_count, dest_count, input_port_queue);

	// packets only to be sent to those outputs port queues, which have empty space
	// maintain count for free output queues
	int free_output_buffers_count = 0;

	// maintain full/empty status of each output queue
	int output_queue_status[dest_count] = {0};

	find_free_buffers_at_output_ports(free_output_buffers_count, output_queue_status, dest_count, output_port_queue, output_port_queue_size);

	// used to stop searching loop, if all input ports are searched and not enough ready packets
	int arbitration_count = 1;

	// allow multiple packets from all input ports(equal to the number of output queues having queue space), in one cylce to go to output ports.
	// if not enough ready packets in this cycle, then try in next cycle
	while (free_output_buffers_count > 0 && ready_packets > 0 && arbitration_count <= source_count)
	{
		// check all virtual queues(equal to num_mem_pools) for ready packet, in one cycle allow only one virtual queue to send its packet
		// use the arbitrator to search next virtual queue next time
		for (int num_vqs = 0; num_vqs < dest_count; num_vqs++)
		{
			int out_port_num = vq_arbitrator_input_queue[arbitrator];

			// checking available packets in a virtual queue where the arbitrator is
			if (input_port_queue[arbitrator][out_port_num].size() > 0 && input_port_queue[arbitrator][out_port_num].front().mem.cycle == common_clock)
			{
				// if same output port queue(as virtual queue number) space is also available, transmit the packet to output queue with switching delay
				int status = output_queue_status[out_port_num];
				if (status)
				{
					int current_packet_size = (input_port_queue[arbitrator][out_port_num].front().mem.trans_id == 0) ? (128 * (!network_flow) + 64 * (network_flow)) : (64 * (!network_flow) + 128 * (network_flow));
					if(input_port_queue[arbitrator][out_port_num].front().mem.trans_id > 1e8){
						current_packet_size = RDMA_PKT_SIZE * (network_flow) + 64 * (!network_flow);
					}
					if ( (status == 1 && current_packet_size == 128) || (status!=3 && current_packet_size == RDMA_PKT_SIZE))
					{
						ready_packets--;
						vq_arbitrator_input_queue[arbitrator]++;
						if (vq_arbitrator_input_queue[arbitrator] == dest_count)
						{
							vq_arbitrator_input_queue[arbitrator] = 0;
						}
						continue;
					}
					input_port_queue[arbitrator][out_port_num].front().out_switch_input_port = input_port_queue[arbitrator][out_port_num].front().mem.cycle;
					input_port_queue[arbitrator][out_port_num].front().mem.cycle = input_port_queue[arbitrator][out_port_num].front().mem.cycle + switching_delay;
					input_port_queue[arbitrator][out_port_num].front().in_switch_output_port = input_port_queue[arbitrator][out_port_num].front().mem.cycle;
					output_port_queue[out_port_num].push_back(input_port_queue[arbitrator][out_port_num].front());
					input_port_queue[arbitrator][out_port_num].pop_front();
					arbitrator++; // only one packet allowed per input port, so increamented arbitrator
					arbitration_count++;
					if (arbitrator == source_count)
						arbitrator = 0;
					free_output_buffers_count--;
					ready_packets--;
					output_queue_status[out_port_num] = 0; // cannot send more packets to same output queue
					break;								   // no need to search furthur, as one packet already transmitted (max is 1 per port in a cycle)
				}
				// if same output queue doesn not have space, search in next virtual queue with different output port queue
				else
				{
					vq_arbitrator_input_queue[arbitrator]++;
					if (vq_arbitrator_input_queue[arbitrator] == dest_count)
					{
						vq_arbitrator_input_queue[arbitrator] = 0;
					}
				}
			}
			// if ready packet not available at any virtual queue, increase the arbitrator and search next virtual queue (upto max no. of virtual queues)
			else
			{
				vq_arbitrator_input_queue[arbitrator]++;
				if (vq_arbitrator_input_queue[arbitrator] == dest_count)
				{
					vq_arbitrator_input_queue[arbitrator] = 0;
				}
			}
		}
		// if loop didn't break because there was no packet which was able to get transmitted in this cycle, then search in next input-port queue
		// increament the arbitrator
		arbitrator++;
		arbitration_count++;
		if (arbitrator == source_count)
			arbitrator = 0;
	}

	for (int i = 0; i < source_count; i++)
	{
		for (int j = 0; j < dest_count; j++)
		{
			for (long long int k = 0; k < input_port_queue[i][j].size(); k++)
			{
				if (input_port_queue[i][j].at(k).mem.cycle == common_clock)
				{
					input_port_queue[i][j].at(k).mem.cycle++;
				}
				else if (input_port_queue[i][j].at(k).mem.cycle > common_clock)
					break;
			}
		}
	}
}

// send all ready packets from output port to mem_pool nic queue
void switch_to_dest_nic_queue(deque<packet> *output_port_queue, deque<packet> *nic_queue_dest, int dest_count)
{
	for (int i = 0; i < dest_count; i++)
	{
		bool has_size = 0;
		// if space available at nic at mem_pool, select top packet from the output port queue and transmit to pool nic.
		// next packet will wait until current packet is transmitted, appropriate cycles added to all waiting packets
		// propagation delay inside RACK is 5ns (max 1 meter assumed)
		if (output_port_queue[i].size() > 0)
		{
			int current_packet_size = (output_port_queue[i].front().mem.trans_id == 0) ? 128 * (!network_flow) + 64 * (network_flow) : 64 * (!network_flow) + 128 * (network_flow);
			if(output_port_queue[i].front().mem.trans_id > 1e8){
				current_packet_size = RDMA_PKT_SIZE * (network_flow) + 64 * (!network_flow);
			}
			has_size = has_queue_size(nic_queue_dest, i, current_packet_size);
			if (has_size && output_port_queue[i].front().mem.cycle == common_clock)
			{
				if (output_port_queue[i].front().is_transmitting > 0)
				{
					switch_trans_delay = (current_packet_size * 8) / switch_bandwidth;
					if (output_port_queue[i].front().is_transmitting == switch_trans_delay)
					{
						output_port_queue[i].front().out_switch_output_port = output_port_queue[i].front().mem.cycle;
						output_port_queue[i].front().mem.cycle = output_port_queue[i].front().mem.cycle + prop_delay ;
						output_port_queue[i].front().in_nic_dest = output_port_queue[i].front().mem.cycle;
						output_port_queue[i].front().is_transmitting = 0;
						nic_queue_dest[i].push_back(output_port_queue[i].front());

						// cout<<"\n common_clock "<<common_clock<<" start_cycle"<<output_port_queue[i].front().mem.miss_common_clock<<" packet reach at remote pool nic "<<i<<" packet-id: "<<output_port_queue[i].front().mem.id<<" node "<<output_port_queue[i].front().mem.source;
						output_port_queue[i].pop_front();
						for (long long int j = 0; j < output_port_queue[i].size(); j++)
						{
							if (output_port_queue[i].at(j).mem.cycle == common_clock)
								output_port_queue[i].at(j).mem.cycle++;
							else if (output_port_queue[i].at(j).mem.cycle > common_clock)
								break;
						}
					}
					else if (output_port_queue[i].front().is_transmitting < switch_trans_delay)
					{
						output_port_queue[i].front().is_transmitting++;
						for (long long int j = 0; j < output_port_queue[i].size(); j++)
						{
							if (output_port_queue[i].at(j).mem.cycle == common_clock)
								output_port_queue[i].at(j).mem.cycle++;
							else if (output_port_queue[i].at(j).mem.cycle > common_clock)
								break;
						}
					}
				}
				else if (output_port_queue[i].front().is_transmitting == 0)
				{
					output_port_queue[i].front().is_transmitting++;
					for (long long int j = 0; j < output_port_queue[i].size(); j++)
					{
						if (output_port_queue[i].at(j).mem.cycle == common_clock)
							output_port_queue[i].at(j).mem.cycle++;
						else if (output_port_queue[i].at(j).mem.cycle > common_clock)
							break;
					}
				}
			}
		}

		else if (!has_size)
		{
			for (long long int j = 0; j < output_port_queue[i].size(); j++)
			{
				if (output_port_queue[i].at(j).mem.cycle == common_clock)
					output_port_queue[i].at(j).mem.cycle++;
				else if (output_port_queue[i].at(j).mem.cycle > common_clock)
					break;
			}
		}
	}
}

void send_to_dest_collector_queue(deque<packet> *nic_queue_dest, deque<packet> *packet_queue_dest, int dest_count)
{
	for (int i = 0; i < dest_count; i++)
	{
		if (nic_queue_dest[i].size() > 0)
		{
			if (nic_queue_dest[i].front().mem.cycle == common_clock)
			{
				if (nic_queue_dest[i].front().is_processing > 0)
				{
					if (nic_queue_dest[i].front().is_processing == nic_proc_delay)
					{
						nic_queue_dest[i].front().mem.cycle = nic_queue_dest[i].front().mem.cycle ;
						nic_queue_dest[i].front().out_nic_dest = nic_queue_dest[i].front().mem.cycle;
						nic_queue_dest[i].front().is_processing = 0;
						nic_queue_dest[i].front().mem.cycle += packetisation_time; // depacketisaton time 50 ns on both sides
						packet_queue_dest[i].push_back(nic_queue_dest[i].front());

						//		cout<<"\n common_clock "<<common_clock<<" start_cycle"<<packet_queue_dest[i].front().mem.miss_common_clock<<" packet reach at pool nic "<<i<<" packet-id: "<<packet_queue_dest[i].front().mem.id<<" node "<<packet_queue_dest[i].front().mem.source;

						nic_queue_dest[i].pop_front();
						for (long long int j = 0; j < nic_queue_dest[i].size(); j++)
						{
							if (nic_queue_dest[i].at(j).mem.cycle == common_clock)
								nic_queue_dest[i].at(j).mem.cycle++;
							else if (nic_queue_dest[i].at(j).mem.cycle > common_clock)
								break;
						}
					}
					else if (nic_queue_dest[i].front().is_processing < nic_proc_delay)
					{
						nic_queue_dest[i].front().is_processing++;
						for (long long int j = 0; j < nic_queue_dest[i].size(); j++)
						{
							if (nic_queue_dest[i].at(j).mem.cycle == common_clock)
								nic_queue_dest[i].at(j).mem.cycle++;
							else if (nic_queue_dest[i].at(j).mem.cycle > common_clock)
								break;
						}
					}
				}
				else if (nic_queue_dest[i].front().is_processing == 0)
				{
					nic_queue_dest[i].front().is_processing++;
					for (long long int j = 0; j < nic_queue_dest[i].size(); j++)
					{
						if (nic_queue_dest[i].at(j).mem.cycle == common_clock)
							nic_queue_dest[i].at(j).mem.cycle++;
						else if (nic_queue_dest[i].at(j).mem.cycle > common_clock)
							break;
					}
				}
			}
		}
	}
}
int count_c=0;
void remote_memory_add(deque<packet> *packet_queue_pool)
{
	for (int i = 0; i < num_mem_pools; i++)
	{
		while (!packet_queue_pool[i].empty() && packet_queue_pool[i].front().mem.cycle == common_clock)
		{
			count_c++;
			remote_memory_access temp = packet_queue_pool[i].front().mem;
			int node_id = temp.source;
			uint64_t miss_cycle = temp.miss_cycle_num;
			packet_queue_pool[i].pop_front();
			// pthread_mutex_lock(&lock_queue);
			if(temp.isRDMA)
			{
				//for RDMA added 64 packets for the page and add this page id into a queue
				for(int j=0;j<64;j++)
				{
					obj.add_one_and_run(remote_mem[i], temp.mem_access_addr+64*j, temp.isWrite, temp.trans_id, temp.cycle, temp.miss_cycle_num, temp.source);
				}
			}
			else
			{
				overall_total_network_delay = overall_total_network_delay + (common_clock - miss_cycle);
				total_network_delay[node_id] = total_network_delay[node_id] + (common_clock - miss_cycle);
				obj.add_one_and_run(remote_mem[i], temp.mem_access_addr, temp.isWrite, temp.trans_id, temp.cycle, temp.miss_cycle_num, temp.source);
			}
			// pthread_mutex_unlock(&lock_queue);
		}
	}
}

void simulate_network()
{
	network_flow = 0;

	send_to_source_nic_queue(packet_queue_node, tx_nic_queue_node, num_nodes);

	source_to_switch_input_port(tx_nic_queue_node, tx_input_port_queue, num_nodes, num_mem_pools, tx_input_port_queue_size);

	switch_input_port_to_output_port(tx_input_port_queue, tx_output_port_queue, num_nodes, num_mem_pools, tx_arbitrator, tx_vq_arbitrator_input_queue, tx_input_port_queue_size, tx_output_port_queue_size);

	switch_to_dest_nic_queue(tx_output_port_queue, tx_nic_queue_pool, num_mem_pools);

	send_to_dest_collector_queue(tx_nic_queue_pool, packet_queue_pool, num_mem_pools);

	remote_memory_add(packet_queue_pool);
}

void network_memory_completion(deque<packet> *packet_queue_dest, int dest_count)
{

	for (int i = 0; i < dest_count; i++)
	{
		while (!packet_queue_dest[i].empty())
		{
//			mem_response[i].push_back(packet_queue_dest[i].front().mem);
			if(packet_queue_dest[i].front().mem.trans_id<1e8)
				add_remote_access_time(packet_queue_dest[i].front().mem);
			packet_queue_dest[i].pop_front();
		}
	}
}

void simulate_network_reverse()
{
	network_flow = 1;

	send_to_source_nic_queue(rx_packet_queue_pool, rx_nic_queue_pool, num_mem_pools);

	source_to_switch_input_port(rx_nic_queue_pool, rx_input_port_queue, num_mem_pools, num_nodes, rx_output_port_queue_size);

	switch_input_port_to_output_port(rx_input_port_queue, rx_output_port_queue, num_mem_pools, num_nodes, rx_arbitrator, rx_vq_arbitrator_input_queue, rx_input_port_queue_size, rx_output_port_queue_size);

	switch_to_dest_nic_queue(rx_output_port_queue, rx_nic_queue_node, num_nodes);

	send_to_dest_collector_queue(rx_nic_queue_node, rx_packet_queue_node, num_nodes);

	network_memory_completion(rx_packet_queue_node, num_nodes);

}