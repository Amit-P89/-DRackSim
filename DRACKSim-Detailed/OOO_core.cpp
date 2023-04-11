#define RS_Size 64
#define ROB_Size 192
#define LSQ_Size 64
#define num_reg 1024
#define decode_width 8
#define issue_width 8
#define dispatch_width 8
#define commit_width 8
#define ld_str_width 8
#define num_exec_units 8

#define decode_latency 2
#define branch_penalty 100

#define decode_buffer_size 64


#define num_ins_types 9

//add instruction latency
//to be modeled behind target system latency
enum INS_Category
{
        INT_ALU=2, //0
        INT_MUL=2, //1
        INT_DIV=2, //2
        x87=2, //3
        x87_DIV=2, //4
        x87_MUL=2, //5
        SSE=2, //6
        NOP=2, //7
        AGU=2, //last
};



//#define scheduling_latency will be decided by core logic, time to clear dependencies and get a free execution unit

#define max_read_reg 4
#define max_write_reg 4

uint64_t num_inst_issued[num_nodes][core_count] = {0};
uint64_t num_inst_exec[num_nodes][core_count] = {0};
uint64_t num_inst_ready[num_nodes][core_count] = {0};
uint64_t num_inst_commited[num_nodes][core_count] = {0};
uint64_t num_ld_str_completed[num_nodes][core_count] = {0};
uint64_t num_ld_str_total[num_nodes][core_count] = {0};
uint64_t ins_latencies[num_ins_types] = {INT_ALU,INT_MUL,INT_DIV,x87,x87_DIV,x87_MUL,SSE,NOP,AGU};
// to keep the decoded instructions if ROB entry is not available

uint64_t num_branches[num_nodes][core_count]={0};
uint64_t num_branch_misprediction[num_nodes][core_count]={0};

ofstream abc;   

void broadcast_agu(int,int,int);
// comments:RAT enteries points to ROB
// structure for register-Alias-Table
struct RAT
{
        int ROB_index = -1; //-1 -> Registee file, !=-1 -> ROB index
};

RAT reg_alias_table[num_nodes][core_count][num_reg];

// comments for ROB: instructions in ROB in-order, issue in-order, commit in-order
// update the ROB and RS after any instruction completes
// declare inside thread function

// struture for Re-order Buffer
struct ROB
{
        int32_t write_reg_num[max_write_reg];
        int64_t mem_write_operand;
        int16_t mem_write_size;
        bool write_completed = 0;
        int status = -1; // 0 for issued, 1 for dispatched, 2 executed, 3 write-back and commit
        uint64_t request_cycle;
        vector<int> INS_RS_Dependency;
        int proc_id;
        int64_t ins_id;
};

class circular_queue
{
public:
        vector<ROB> rob;
        int issue_ptr = -1;
        int commit_ptr = -1;

        circular_queue()
        {
                rob.reserve(ROB_Size);
                for (int i = 0; i < ROB_Size; i++)
                {
                        rob[i].status = -1;
                }
        }

        void do_commit()
        {

                if (commit_ptr == issue_ptr)
                {
                        commit_ptr = -1;
                        issue_ptr = -1;
                }
                else
                {
                        commit_ptr = (commit_ptr + 1) % ROB_Size;
                }
        }
        int get_commit_ptr()
        {
                return this->commit_ptr;
        }

        int do_issue(ROB new_entry)
        {
                if (commit_ptr == -1 && issue_ptr == -1)
                {
                        issue_ptr = 0;
                        commit_ptr = 0;
                }
                else if (issue_ptr == ROB_Size - 1 && commit_ptr != 0)
                {
                        issue_ptr = 0;
                }
                else
                {
                        issue_ptr++;
                }

                rob[issue_ptr] = new_entry;
                // cout << " added at rob-" << issue_ptr;
                return issue_ptr;
        }

        bool is_empty()
        {
                if(commit_ptr==-1 && issue_ptr==-1)
                        return true;
                else
                        return false;
        }

        bool has_size()
        {
                if ((commit_ptr == 0 && issue_ptr == ROB_Size - 1) || (issue_ptr == (commit_ptr - 1) % (ROB_Size - 1)))
                        return 0;
                else
                        return 1;
        }

        void add_dependency(int rs_index, int rob_index)
        {
                rob[rob_index].INS_RS_Dependency.push_back(rs_index);
        }
};

circular_queue reorder_buffer[num_nodes][core_count];

// data structure for execution units
struct exe_units
{
public:
        int rob_index;
        int rs_index;
        int cycle_completed;
        int status = 0;
};
// 5 exe units for 5 type of ins and 0=>ALU,1=>MUL,2=>SHIFT,3=>DIV,4=> AGU instructions
exe_units execution_unit[num_nodes][core_count][num_ins_types][num_exec_units];

int has_exe_unit(int node_id, int core_id,int id)
{
        int index = 0;
        for (index = 0; index < num_exec_units; index++)
        {
                if (execution_unit[node_id][core_id][id][index].status == 0)    //id represents the type of execution unit, ALU exe for ALU instruction
                {
                        return index;
                }
        }
        return -1;
}

// structure for load/store queue
struct LSQ
{
        int64_t ins_vaddr; // vaddr of the data (not the ins)
        uint64_t ld_str_id;
        int access_size;
        int proc_id;
        int rs_index;         // to broadcast the load result back to the rs_instruction entry
        int rob_index;        // to broadcast the store result back to the rob_instruction entry
        bool INS_type_ld_str; // 0 for load, 1 for store
        bool completed = 0;   // 0 means not completed yet
};

vector<LSQ> load_store_queue[num_nodes][core_count];

// comments: free the RS as soon as instruction is dispatched.
// structure for reservation-stations
struct res_station
{
        bool valid = 0; // 0 means empty and usable
        int ROB_index;  // stores the ROB index where this instruction is added
        int64_t ins_id;
        int32_t RR[max_read_reg];
        int reg_read_dependencies[max_read_reg];
        int64_t mem_read_addr[2];
        int16_t mem_read_size[2];
        int64_t mem_read_dependencies[2];
        bool branch_miss_predicted;
        int proc_id;
        int ins_type;

        res_station()
        {
                for (int i = 0; i < max_read_reg; i++)
                {
                        // initial value, -1 value is clear dependency, positive value is index of ROB
                        if (i < 2)
                                mem_read_dependencies[i] = -2;
                        reg_read_dependencies[i] = -2;
                }
        }
};

res_station reservation_station[num_nodes][core_count][RS_Size];
// check for the availability of space in rs and returns the free slot if available
int rs_has_size(int node_id, int core_id)
{
        for (int i = 0; i < RS_Size; i++)
        {
                if (reservation_station[node_id][core_id][i].valid == 0)
                        return i;
        }
        return -1;
}
struct decode_buf
{
        INST_FETCH_QUEUE ins;
        int complete_cycle;
};
queue<decode_buf> decode_buffer[num_nodes][core_count];

// ready instructions to be executed.
deque<pair<int, int>> ready_queue[num_nodes][core_count][num_ins_types];

int get_ins_type(INST ins)
{

        return ins.ins_type;
}

// Decoding the decode _width num of instructions and add them to decode buffer
void decode_ins(int node_id, int core_id)
{
        int count = 0;
        while (count < decode_width && !inst_queue[node_id][core_id].empty())
        {
                INST_FETCH_QUEUE temp = inst_queue[node_id][core_id].top();
                // cout<<"core_id: "<<core_id<<" mem_com_cyc: "<<temp.ins.ins_id<<" "<<temp.mem_complete_cycle<<" "<< temp.tlb_complete_cycle<<endl;
                if (decode_buffer[node_id][core_id].size() < decode_buffer_size)
                {
                        if (max(temp.mem_complete_cycle, temp.tlb_complete_cycle) <= common_clock)
                        {
                                decode_buf decode_buffer_entry;
                                decode_buffer_entry.ins = temp;
                                decode_buffer_entry.ins.ins.ins_type = get_ins_type(temp.ins);
                                // cout<<"opcode: "<<temp.ins.opcode<<"category: "<<temp.ins.category<<"ins_type: "<< get_ins_type(temp.ins);
                                // cin.get();
                                decode_buffer_entry.complete_cycle = common_clock + decode_latency;
                                decode_buffer[node_id][core_id].push(decode_buffer_entry);
                                // inst_queu_out << "core_id: " << core_id << "  " << hex << temp.ins.addr << " " << dec << temp.ins.ins_id << " " << temp.mem_complete_cycle << endl;
                                inst_queue[node_id][core_id].pop();
                        }
                        else
                        {
                                break;
                        }
                }
                else
                {
                        break;
                }
                count++;
        }
        // while(!decode_buffer[node_id][core_id].empty())decode_buffer[node_id][core_id].pop();
}

// used to check whether dependency has bee cleared for a rs entry
bool check_dependecy_status(int node_id, int core_id, int rs_index)
{
        // have to check in mem-read and read dependencies
        bool cleared = 1;
        for (int i = 0; i < 2; i++)
        {
                if (reservation_station[node_id][core_id][rs_index].mem_read_dependencies[i] != -1)
                {
                        cleared = 0;
                }
        }

        for (int i = 0; i < max_read_reg; i++)
        {
                if (reservation_station[node_id][core_id][rs_index].reg_read_dependencies[i] != -1)
                {
                        cleared = 0;
                }
        }
        return cleared;
}

// Assigning the decoded instruction to a execution unit
void dispatch(int node_id, int core_id)
{       
        for(int id=0;id<num_ins_types;id++)
        {
                int count = 0;
                while (count < dispatch_width && !ready_queue[node_id][core_id][id].empty())
                {
                        //if(common_clock>30900000)cout<<"dispatch"<<endl;
                        int rs_id = ready_queue[node_id][core_id][id].front().first;
                        int cycle = ready_queue[node_id][core_id][id].front().second;
                        int exec_index = has_exe_unit(node_id, core_id, id);
                        if (cycle >= 1 && exec_index != -1)
                        {
                                if (reservation_station[node_id][core_id][rs_id].branch_miss_predicted)
                                {
                                        branch_misprediction_penalty[node_id][core_id] = branch_penalty;
                                        reservation_station[node_id][core_id][rs_id].branch_miss_predicted = 0;
                                        break;
                                }

                                ready_queue[node_id][core_id][id].pop_front();
                                exe_units temp;
                                temp.rs_index = rs_id;
                                temp.rob_index = reservation_station[node_id][core_id][rs_id].ROB_index;
                                temp.cycle_completed = 0;
                                temp.status = 1;
                                execution_unit[node_id][core_id][id][exec_index] = temp;
                                if(id==num_ins_types-1){
                                        continue;
                                }
                                // remove entry  in reservation station
                                reservation_station[node_id][core_id][rs_id].valid = 0;
                                // update status from Issued to dispatched i.e. 1in ROB entry
                                reorder_buffer[node_id][core_id].rob[temp.rob_index].status = 1;
                        }
                        count++;
                }
        }
}
// Just updating the number off cycles it has been present in exec unit
void update_exec_units(int node_id, int core_id)
{
        for(int id=0;id<num_ins_types;id++){
                for (int i = 0; i < num_exec_units; i++)
                {
                        if (execution_unit[node_id][core_id][id][i].status == 1)
                                execution_unit[node_id][core_id][id][i].cycle_completed++;
                }
        }
}
// To store the completed instructions rob entry to broadcast
vector<int> completed_inst_rob_index[num_nodes][core_count];

// check if any inst has been completed
// and add it in  completed_inst_rob_index
void check_executed_instruction(int node_id, int core_id)
{
        for(int id=0;id<num_ins_types;id++){
                for (int i = 0; i < num_exec_units; i++)
                {
                        if (execution_unit[node_id][core_id][id][i].cycle_completed == ins_latencies[id] && execution_unit[node_id][core_id][id][i].status)
                        {
                                if(id!=8)
                                        num_inst_exec[node_id][core_id]++;
                                execution_unit[node_id][core_id][id][i].status = 0;
                                if(id==num_ins_types-1)
                                {
                                        // AGU ins
                                        int rs_id = execution_unit[node_id][core_id][id][i].rs_index;
                                        broadcast_agu(node_id,core_id,rs_id);
                                        continue;
                                }
                                int rob_index = execution_unit[node_id][core_id][id][i].rob_index;
                                completed_inst_rob_index[node_id][core_id].push_back(rob_index);
                        }
                }
        }
}

// Broadcast the completed ins present in completed_inst_rob_index
void broadcast_ins_complete(int node_id, int core_id)
{
        // call before check_executed_instruction
        for (int i = 0; i < completed_inst_rob_index[node_id][core_id].size(); i++)
        {
                int rob_index = completed_inst_rob_index[node_id][core_id][i];
                // erased from completed_inst_rob_index
                //                completed_inst_rob_index[node_id][core_id].erase(completed_inst_rob_index[node_id][core_id].begin());
                // update status from dispatched to executed i.e 2
                reorder_buffer[node_id][core_id].rob[rob_index].status = 2;

                // counter for total instructions executed

                //  get list of rs entries depending on this ROB entry.
                while (reorder_buffer[node_id][core_id].rob[rob_index].INS_RS_Dependency.size())
                {
                        //if(common_clock>30900000)cout<<"bic"<<endl;
                        int rs_index = reorder_buffer[node_id][core_id].rob[rob_index].INS_RS_Dependency[0];
                        // erase 0th element
                        reorder_buffer[node_id][core_id].rob[rob_index].INS_RS_Dependency.erase(reorder_buffer[node_id][core_id].rob[rob_index].INS_RS_Dependency.begin());
                        // update dependency in reservation station entry
                        for (int i = 0; i < max_read_reg; i++)
                        {
                                int index = reservation_station[node_id][core_id][rs_index].reg_read_dependencies[i];
                                if (index == rob_index)
                                {
                                        reservation_station[node_id][core_id][rs_index].reg_read_dependencies[i] = -1;
                                        break;
                                }
                        }
                        // check whether all dependecies are cleared
                        bool cleared = check_dependecy_status(node_id, core_id, rs_index);
                        if (cleared)
                        {
                                int ins_index = reservation_station[node_id][core_id][rs_index].ins_type;
                                ready_queue[node_id][core_id][ins_index].push_back({rs_index, 0});
                        }
                }
        }
        completed_inst_rob_index[node_id][core_id].clear();
}

// used to update RAT entry if a ins is commited
void update_RAT(int node_id, int core_id, int commit_ptr)
{
        for (int i = 0; i < num_reg; i++)
        {
                if (reg_alias_table[node_id][core_id][i].ROB_index == commit_ptr)
                {
                        reg_alias_table[node_id][core_id][i].ROB_index = -1;
                }
        }
}

// search same address in the LSQ while adding new loads
bool found_in_LSQ(int node_id, int core_id, uint64_t ld_addr, int access_size)
{
        for (auto it = load_store_queue[node_id][core_id].begin(); it != load_store_queue[node_id][core_id].end(); it++)
        {
                if ((*it).ins_vaddr == ld_addr && (*it).access_size == access_size && (*it).INS_type_ld_str == 1)
                {
                        return true;
                }
        }
        return false;
}

// pending load/stores, could not be added to LSQ at same cycle due to load/store with
queue<LSQ> pending_load_store[num_nodes][core_count];

// maximum load/store requests that are sent per cycle, can;t be more than load/store width
int num_ld_str_inCycle[num_nodes][core_count] = {0};

// used for giving a unique ID to the load store requests
uint64_t ld_str_id[num_nodes][core_count];

// Add an entry into the LSQ if  size is not available we are keeping in pending LSQ
int add_to_load_store_queue(int node_id, int core_id, uint64_t mem_req_addr, int16_t mem_req_size, int proc_id, int rs_index, int rob_index)
{
        num_ld_str_total[node_id][core_id]++;
        // if a store in the load/store queue has a same address as the new load request, perform operator forwarding
        bool operator_forward = false;

        if (rs_index != -1)
        {
                operator_forward = found_in_LSQ(node_id, core_id, mem_req_addr, mem_req_size);

                if (operator_forward)
                {
                        for (int i = 0; i < 2; i++)
                        {
                                if (reservation_station[node_id][core_id][rs_index].mem_read_addr[i] == mem_req_addr &&
                                    reservation_station[node_id][core_id][rs_index].mem_read_size[i] == mem_req_size)
                                {
                                        reservation_station[node_id][core_id][rs_index].mem_read_dependencies[i] = -1;
                                }
                        }
                        return operator_forward;
                }

                // bool cleared = check_dependecy_status(node_id, core_id, rs_index);
                // if (cleared)
                // {
                //         ready_queue[node_id][core_id].push_back({rs_index, 0});
                // }
        }
        if (rob_index != -1)
        {
                // delete old entry
                for (auto it = load_store_queue[node_id][core_id].begin(); it != load_store_queue[node_id][core_id].end(); it++)
                {
                        if ((*it).ins_vaddr == mem_req_addr && (*it).access_size == mem_req_size && (*it).INS_type_ld_str == 1)
                        {
                                load_store_queue[node_id][core_id].erase(it);
                                break;
                        }
                        if(it==load_store_queue[node_id][core_id].end())
                        {
                                break;
                        }
                }
        }
        LSQ temp;
        temp.ld_str_id = ld_str_id[node_id][core_id];
        temp.ins_vaddr = mem_req_addr;
        temp.access_size = mem_req_size;
        temp.proc_id = proc_id;
        temp.rs_index = rs_index;
        temp.rob_index = rob_index;

        if (rs_index == -1)
                temp.INS_type_ld_str = 1; // for store request
        else if (rob_index == -1)
                temp.INS_type_ld_str = 0; // for load request

        if (load_store_queue[node_id][core_id].size() < LSQ_Size && num_ld_str_inCycle[node_id][core_id] <= ld_str_width)
        {
                load_store_queue[node_id][core_id].push_back(temp);
                ld_str_id[node_id][core_id]++;
                num_ld_str_inCycle[node_id][core_id]++;

                // sending to caches the load/store entry
                miss_queue cache_access;
                cache_access.ins_id = temp.ld_str_id;
                cache_access.ins_vaddr = temp.ins_vaddr;
                cache_access.inst_fetch.ins.proc_id = temp.proc_id;

                if (temp.INS_type_ld_str == 0)
                        cache_access.write = 0;
                else
                        cache_access.write = 1;
                cache_access.node_id = node_id;
                cache_access.core_id = core_id;
                cache_access.cache_access_size = temp.access_size;

                load_store_buffer[node_id][core_id].push(cache_access);
        }
        else
        {
                ld_str_id[node_id][core_id]++;
                pending_load_store[node_id][core_id].push(temp);
        }

        return operator_forward;
}

// Add the entry from pending LSQ to the LSQ if space is available
void update_load_store_queue(int node_id, int core_id)
{
        while (!pending_load_store[node_id][core_id].empty() && num_ld_str_inCycle[node_id][core_id] <= ld_str_width && load_store_queue[node_id][core_id].size() < LSQ_Size)
        {
                //if(common_clock>30900000)cout<<"ulsq"<<endl;
                LSQ temp = pending_load_store[node_id][core_id].front();

                int rs_index = temp.rs_index;

                // if a store in the load/store queue has a same address as the new load request, perform operator forwarding
                bool operator_forward = false;

                if (rs_index != -1)
                {
                        operator_forward = found_in_LSQ(node_id, core_id, temp.ins_vaddr, temp.access_size);
                }

                if (operator_forward)
                {
                        for (int i = 0; i < 2; i++)
                        {
                                if (reservation_station[node_id][core_id][rs_index].mem_read_addr[i] == temp.ins_vaddr &&
                                    reservation_station[node_id][core_id][rs_index].mem_read_size[i] == temp.access_size)
                                {
                                        reservation_station[node_id][core_id][rs_index].mem_read_dependencies[i] = -1;
                                }
                        }
                }
                if(rs_index!=-1){
                        bool cleared = check_dependecy_status(node_id, core_id, rs_index);
                        if (cleared)
                        {
                                int ins_index = reservation_station[node_id][core_id][rs_index].ins_type;
                                ready_queue[node_id][core_id][ins_index].push_back({rs_index, 0});
                                return;
                        }
                }
                load_store_queue[node_id][core_id].push_back(temp);
                pending_load_store[node_id][core_id].pop();
                num_ld_str_inCycle[node_id][core_id]++;

                // sending to caches the load/store entry
                miss_queue cache_access;
                cache_access.ins_id = temp.ld_str_id;
                cache_access.ins_vaddr = temp.ins_vaddr;
                cache_access.inst_fetch.ins.proc_id = temp.proc_id;

                if (temp.INS_type_ld_str == 0)
                        cache_access.write = 0;
                else
                        cache_access.write = 1;

                cache_access.node_id = node_id;
                cache_access.core_id = core_id;
                cache_access.cache_access_size = temp.access_size;

                load_store_buffer[node_id][core_id].push(cache_access);
        }
}

// every cycle, need to update num_ld_str_inCycle[node_id][core_id]=0
// should be called seperately no need to  call it here
// It also being called in DL1 hit case
void broadcast_completed_LSQ(int node_id, int core_id, uint64_t ins_id)
{
        // search in lsq

        auto it = load_store_queue[node_id][core_id].begin();
        for (it = load_store_queue[node_id][core_id].begin(); it != load_store_queue[node_id][core_id].end(); it++)
        {
                if ((*it).ld_str_id == ins_id)
                {
                        (*it).completed = 1;
                        // keep count of completed load/store
                        num_ld_str_completed[node_id][core_id]++;
                        break;
                }
        }
        if(it==load_store_queue[node_id][core_id].end()){
                return;
        }
        if ((*it).rs_index != -1)
        {
                // update rs entry
                int rs_index = (*it).rs_index;
                for (int i = 0; i < 2; i++)
                {
                        if (reservation_station[node_id][core_id][rs_index].mem_read_addr[i] == (*it).ins_vaddr &&
                            reservation_station[node_id][core_id][rs_index].mem_read_size[i] == (*it).access_size)
                        {
                                reservation_station[node_id][core_id][rs_index].mem_read_dependencies[i] = -1;
                        }
                }
                bool cleared = check_dependecy_status(node_id, core_id, rs_index);
                if (cleared)
                {
                        int ins_index = reservation_station[node_id][core_id][rs_index].ins_type;
                        ready_queue[node_id][core_id][ins_index].push_back({rs_index, 0});
                }
        }
        if ((*it).rob_index != -1)
        {
                // int rob_index = (*it).rob_index;
                // if (reorder_buffer[node_id][core_id].rob[rob_index].mem_write_operand == (*it).ins_vaddr &&
                //     reorder_buffer[node_id][core_id].rob[rob_index].mem_write_size == (*it).access_size)
                // {
                //         reorder_buffer[node_id][core_id].rob[rob_index].status = 3;
                // }
                // cout << "write completed " << endl;
        }
        load_store_queue[node_id][core_id].erase(it);
}

// Update the load store completion status of entries present in response_queue
void load_store_completion(int node_id, int core_id)
{
        // remove entry from response queue and update LSQ status
        if (response_queue[node_id][core_id].size())
        {
                miss_queue temp = response_queue[node_id][core_id].front();
                // cout << "tlb:" << temp.inst_fetch.tlb_complete_cycle << "mem:" << temp.complete_cycle << endl;
                while (max(temp.inst_fetch.tlb_complete_cycle, temp.complete_cycle) <= common_clock)
                {
                        //if(common_clock>30900000)cout<<"lsc"<<endl;
                        response_queue[node_id][core_id].pop();
                        broadcast_completed_LSQ(node_id, core_id, temp.ins_id);
                        if (response_queue[node_id][core_id].size())
                                temp = response_queue[node_id][core_id].front();
                        else
                                break;
                }
        }
}
// commit the ins at the commit ptr if completed
void commit(int node_id, int core_id)
{
        for (int i = 0; i < commit_width; i++)
        {
                int commit_ptr = reorder_buffer[node_id][core_id].get_commit_ptr();
                if (reorder_buffer[node_id][core_id].rob[commit_ptr].status == 2)
                {
                        uint64_t write_back_address = reorder_buffer[node_id][core_id].rob[commit_ptr].mem_write_operand;
                        int write_back_size = reorder_buffer[node_id][core_id].rob[commit_ptr].mem_write_size;
                        int write_back_proc_id = reorder_buffer[node_id][core_id].rob[commit_ptr].proc_id;
                        if (write_back_address == 0)
                        {
                                reorder_buffer[node_id][core_id].rob[commit_ptr].status = 3;
                                // counter of total instructions commited
                                num_inst_commited[node_id][core_id]++;
                                update_RAT(node_id, core_id, commit_ptr);
                                reorder_buffer[node_id][core_id].do_commit();
                        }
                        if (load_store_queue[node_id][core_id].size() < LSQ_Size && write_back_address != 0)
                        {

                                reorder_buffer[node_id][core_id].rob[commit_ptr].status = 3;
                                // counter of total instructions commited
                                num_inst_commited[node_id][core_id]++;
                                update_RAT(node_id, core_id, commit_ptr);
                                add_to_load_store_queue(node_id, core_id, write_back_address, write_back_size, write_back_proc_id, -1, commit_ptr);
                                reorder_buffer[node_id][core_id].do_commit();
                        }
                }
        }
}
int add_to_rob(int node_id, int core_id, INST_FETCH_QUEUE temp)
{
        ROB new_rob;
        for (int i = 0; i < max_write_reg; i++)
        {
                new_rob.write_reg_num[i] = temp.ins.WR[i];
        }
        new_rob.mem_write_operand = temp.ins.write_opr;
        new_rob.mem_write_size = temp.ins.write_size;
        new_rob.status = 0;
        new_rob.ins_id = temp.ins.ins_id; 
        new_rob.request_cycle = temp.request_cycle;
        new_rob.proc_id = temp.ins.proc_id;

        // index of newly added instruction in ROB
        int index = reorder_buffer[node_id][core_id].do_issue(new_rob);

        // updating the RAT based on write registers of instruction added to ROB
        for (int i = 0; i < max_write_reg; i++)
        {
                int reg_num = temp.ins.WR[i];
                if (reg_num != -1)
                        reg_alias_table[node_id][core_id][reg_num].ROB_index = index;
        }
        return index;
}

// adding an instruction to the rservation station
// while adding, note the rs_index and push into all the ROB enteries on which there is any register dependency
// making easy to broadcast results, after the completion of instruction
void add_to_rs(int node_id, int core_id, INST_FETCH_QUEUE temp, int rs_index)
{
        res_station new_ins_entry;
        new_ins_entry.valid = 1;
        // new_ins_entry.ROB_index = rob_index;
        new_ins_entry.proc_id = temp.ins.proc_id;
        new_ins_entry.ins_id = temp.ins.ins_id;
        new_ins_entry.mem_read_addr[0] = temp.ins.read_opr;
        new_ins_entry.mem_read_addr[1] = temp.ins.read_opr2;
        new_ins_entry.mem_read_size[0] = temp.ins.read_size;
        new_ins_entry.mem_read_size[1] = temp.ins.read_size2;
        new_ins_entry.branch_miss_predicted = temp.ins.branch_miss_predicted;
        new_ins_entry.ins_type = temp.ins.ins_type;
        if(temp.ins.is_Branch)
                num_branches[node_id][core_id]++;
        if(temp.ins.branch_miss_predicted)
                num_branch_misprediction[node_id][core_id]++;
        // checking the reg dependencies if cleared updated dependency variable to -1
        // else points to the respected rob entry
        // Initially it will check through RAT if RAT is not  mapped to any rob
        // then it will be found in register file and dependency is cleared
        for (int i = 0; i < max_read_reg; i++)
        {
                int reg_num = new_ins_entry.RR[i] = temp.ins.RR[i];
                if (new_ins_entry.RR[i] == -1)
                        new_ins_entry.reg_read_dependencies[i] = -1; // dependency cleared
                else
                {
                        // check first in register file
                        if (reg_alias_table[node_id][core_id][reg_num].ROB_index == -1)
                        {
                                new_ins_entry.reg_read_dependencies[i] = -1;
                        }
                        else
                        {
                                new_ins_entry.reg_read_dependencies[i] = reg_alias_table[node_id][core_id][reg_num].ROB_index;
                                if (reorder_buffer[node_id][core_id].rob[reg_alias_table[node_id][core_id][reg_num].ROB_index].status == 2)
                                        new_ins_entry.reg_read_dependencies[i] = -1;
                                else
                                        reorder_buffer[node_id][core_id].add_dependency(rs_index, new_ins_entry.reg_read_dependencies[i]);
                        }
                }
        }

        if (!temp.ins.read_opr)
        {
                new_ins_entry.mem_read_dependencies[0] = -1;
        }
        else
        {
                //add to it ready queue 
                int ins_index = num_ins_types-1; // AGU instruction
                ready_queue[node_id][core_id][ins_index].push_back({rs_index, 0});
                // bool operator_forward = add_to_load_store_queue(node_id, core_id, temp.ins.read_opr, temp.ins.read_size,
                //                                                 new_ins_entry.proc_id, rs_index, -1);
                // if (operator_forward)
                //         new_ins_entry.mem_read_dependencies[0] = -1;
        }
        if (!temp.ins.read_opr2)
        {
                new_ins_entry.mem_read_dependencies[1] = -1;
        }
        else
        {
                //add it to ready queue 
                int ins_index = num_ins_types-1; // AGU instruction
                ready_queue[node_id][core_id][ins_index].push_back({rs_index, 0});
                // bool operator_forward = add_to_load_store_queue(node_id, core_id, temp.ins.read_opr2, temp.ins.read_size2,
                //                                                 new_ins_entry.proc_id, rs_index, -1);
                // if (operator_forward)
                //         new_ins_entry.mem_read_dependencies[1] = -1;
        }
        reservation_station[node_id][core_id][rs_index] = new_ins_entry;
        bool cleared = check_dependecy_status(node_id, core_id, rs_index);
        if (cleared)
        {
                int ins_index = reservation_station[node_id][core_id][rs_index].ins_type;
                ready_queue[node_id][core_id][ins_index].push_back({rs_index, 0});
        }
}

void broadcast_agu(int node_id,int core_id,int rs_id){
        // dependency !=-2=>  
        res_station temp;
        temp = reservation_station[node_id][core_id][rs_id];
        if(temp.mem_read_dependencies[0]==-2){
                reservation_station[node_id][core_id][rs_id].mem_read_dependencies[0] = -3;
                bool operator_forward = add_to_load_store_queue(node_id, core_id, temp.mem_read_addr[0], temp.mem_read_size[0],
                                                                        temp.proc_id, rs_id, -1);
                if (operator_forward)
                        reservation_station[node_id][core_id][rs_id].mem_read_dependencies[0] = -1;
        }
        else if(temp.mem_read_dependencies[1]==-2){
               reservation_station[node_id][core_id][rs_id].mem_read_dependencies[1] = -3;
                bool operator_forward = add_to_load_store_queue(node_id, core_id, temp.mem_read_addr[1], temp.mem_read_size[1],
                                                                        temp.proc_id, rs_id, -1);
                if (operator_forward)
                        reservation_station[node_id][core_id][rs_id].mem_read_dependencies[1] = -1; 
        }
        bool cleared = check_dependecy_status(node_id, core_id, rs_id);
        if (cleared)
        {
                int ins_index = reservation_station[node_id][core_id][rs_id].ins_type;
                ready_queue[node_id][core_id][ins_index].push_back({rs_id, 0});
        }
}

void update_ready_queue(int node_id, int core_id)
{       for(int id=0;id<num_ins_types;id++){
                for (int i = 0; i < ready_queue[node_id][core_id][id].size(); i++)
                {
                        ready_queue[node_id][core_id][id].front().second = ready_queue[node_id][core_id][id].front().second + 1;
                }
        }
}

void issue_instruction(int node_id, int core_id)
{
        int count = 0;
        while (!decode_buffer[node_id][core_id].empty() && count < issue_width && decode_buffer[node_id][core_id].front().complete_cycle <= common_clock)
        {
                //if(common_clock>30900000)cout<<"issue"<<endl;fcf 
                int rs_index = rs_has_size(node_id, core_id);
                if (rs_index != -1 && reorder_buffer[node_id][core_id].has_size())
                {
                        num_inst_issued[node_id][core_id]++;
                        INST_FETCH_QUEUE temp = decode_buffer[node_id][core_id].front().ins;
                        add_to_rs(node_id, core_id, temp, rs_index);
                        int rob_index = add_to_rob(node_id, core_id, temp);
                        // adding rob index in the corresponding rs_entry to make broadcasting simple.
                        reservation_station[node_id][core_id][rs_index].ROB_index = rob_index;
                        decode_buffer[node_id][core_id].pop();
                }
                count++;
        }
}

void update_core(int node_id, int core_id)
{
        if (branch_misprediction_penalty[node_id][core_id] > 0)
        {
                branch_misprediction_penalty[node_id][core_id]--;
                return;
        }
        
        decode_ins(node_id, core_id);
        issue_instruction(node_id, core_id);
        dispatch(node_id, core_id);
        check_executed_instruction(node_id, core_id);
        broadcast_ins_complete(node_id, core_id);
        commit(node_id, core_id);
        load_store_completion(node_id, core_id);
        update_load_store_queue(node_id, core_id);
        update_ready_queue(node_id, core_id);
        update_exec_units(node_id, core_id);

        num_ld_str_inCycle[node_id][core_id] = 0;


        // if(inst_queue[node_id][core_id].size() || !(reorder_buffer[node_id][core_id].commit_ptr==-1 && reorder_buffer[node_id][core_id].issue_ptr==-1) || load_store_queue[node_id][core_id].size())
        //         core_simulated_cycle[node_id][core_id]++;
}
