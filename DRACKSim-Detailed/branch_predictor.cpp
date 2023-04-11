
#define BTB_Table_Size 16384
#define BTB_Prime 16381
#define MAX_COUNTER 3


int branch_pred_table[num_nodes][core_count][BTB_Table_Size];

void initialize_branch_predictor(int node_id)
{
    for(int i=0;i<core_count;i++)
        for(int j = 0; j < BTB_Table_Size; j++)
            branch_pred_table[node_id][i][j] = 0;
}

int predict_branch(uint64_t ip, int node_id, int core_id)
{
    uint32_t hash = ip % BTB_Prime;
    int prediction = (branch_pred_table[node_id][core_id][hash] >= ((MAX_COUNTER + 1)/2)) ? 1 : 0;

    return prediction;
}

void last_branch_result(uint64_t ip, int taken, int node_id, int core_id)
{
    uint32_t hash = ip % BTB_Prime;

    if (taken && (branch_pred_table[node_id][core_id][hash] < MAX_COUNTER))
        branch_pred_table[node_id][core_id][hash]++;
    else if ((taken == 0) && (branch_pred_table[node_id][core_id][hash] > 0))
        branch_pred_table[node_id][core_id][hash]--;
}
