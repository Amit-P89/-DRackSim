//yet to link with main code

#include <math.h>

// ---------------------SIMULATION ---------------------
#define simulation_time   1000000000
#define Result_cycle  10000000
#define max_ins  1000000

// --------------- NODES AND MEMORY---------------
#define num_nodes  12
#define num_mem_pools  4
#define core_count  8
#define local_mem_size  0.25
#define remote_mem_size1  32
#define local_DRAM_size  256
#define remote_DRAM_size  remote_mem_size1 * pow(2.0, 10.0)
#define Page_Size  4096

// ------------TLB AND CACHE ------------

#define TLB_HIT 9
#define TLB_MISS 60
#define L1_HIT 4
#define L2_HIT 12
#define L3_HIT 25
#define page_fault_latency 9000
#define chunk_allocation_plus_page_fault_latency 10000
#define iqueue_size 50
#define mshr 8 

// -----------------PROCESSOR-----------------------
#define RS_Size  64
#define ROB_Size   192
#define LSQ_Size   128
#define num_reg   512
#define decode_width   2
#define issue_width  2
#define dispatch_width   2
#define commit_width   2
#define ld_str_width   2
#define num_exec_units   5
#define decode_latency   2
#define execution_latency  5
#define branch_penalty  100
#define max_read_reg  4
#define max_write_reg  4
#define decode_buffer_size  8

// -----------------INTERCONNECT----------


#define tx_packet_size 512	// bits (64-bytes)
#define rx_packet_size 1024 // bits (128-bytes)

// both tx/rx, for all packet sizes, assuming enough memory
#define nic_queue_size 16384 * 64

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

// 1ns=1cycle
int nic_trans_delay = 0;
int switch_trans_delay = 0;

#define nic_proc_delay 5
#define switch_proc_delay 10
#define prop_delay 5 // max 1-meter inside rack
#define switching_delay 2

// switch input port arbitrator (transmitting/receiving)
static int tx_arbitrator = 0;
static int rx_arbitrator = 0;

// ---------------BRANCH PREDICTOR -------

#define BTB_Table_Size 16384
#define BTB_Prime 16381
#define MAX_COUNTER 3

