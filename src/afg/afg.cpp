#include "afg.hpp"

// Declare arrays in DRAM
static std::array<load, size_t(NUMBER_OF_LOADS)> g_demand_power_W;
static std::array<std::array<int, size_t(NUM_TIMESTEPS)>, size_t(NUMBER_OF_LOADS)> g_demand_state;

void AFG(const int &start_day, const int (&load_priority_order)[NUMBER_OF_LOADS],
         float (&threshold)[NUMBER_OF_LOADS][NUMBER_OF_DAYS],
         float (&enable_duration_max)[NUMBER_OF_LOADS][NUMBER_OF_DAYS])
{
    printf("Inside AFG function\n");
    fflush(stdout);

    const std::array<std::string, size_t(NUMBER_OF_LOADS)> load_names = {"air1", "clotheswasher_dryg1", "microwave1", "refrigerator1"};
    const std::string filename = "/cleanData-357days.csv";

    float demand_avg_power_W[NUMBER_OF_LOADS][NUMBER_OF_DAYS];
    stream_demand_avg(filename, load_names, start_day, demand_avg_power_W, enable_duration_max, &g_demand_power_W, &g_demand_state);
    printf("stream_demand_avg completed\n");
    fflush(stdout);

    // Print demand_power_W (from global)
    printf("Demand power (W):\n");
    for (int i = 0; i < NUMBER_OF_LOADS; i++)
    {
        printf("Load: %s\n", g_demand_power_W[i].name.c_str());
        printf("Count: %zu\n", g_demand_power_W[i].count);

        int nonzero_count = 0;
        for (size_t j = 0; j < g_demand_power_W[i].count; j++)
        {
            if (g_demand_power_W[i].values[j] != 0.0f)
                nonzero_count++;
        }
        printf("  %s: %zu timesteps, %d active (%.1f%%)\n",
               g_demand_power_W[i].name.c_str(),
               g_demand_power_W[i].count,
               nonzero_count,
               100.0f * nonzero_count / g_demand_power_W[i].count);
    }
    printf("\n");

    // Print demand state (from global)
    printf("Demand state:\n");
    for (int i = 0; i < NUMBER_OF_LOADS; i++)
    {
        printf("Load #%d: ", i + 1);
        for (int j = 0; j < NUMBER_OF_DAYS; j++)
        {
            printf("%d ", g_demand_state[i][j]);
        }
        printf("\n");
    }
    printf("\n");

    // Calculate load priority factor
    float load_pf[NUMBER_OF_LOADS];
    calc_load_pf(load_priority_order, load_pf);

    // Print load priority factors
    printf("Load priority factors:\n");
    for (int i = 0; i < NUMBER_OF_LOADS; i++)
    {
        printf("%f ", load_pf[i]);
    }
    printf("\n\n");

    // Print demand average power
    printf("Demand average power (W):\n");
    for (int i = 0; i < NUMBER_OF_LOADS; i++)
    {
        printf("Load #%d: ", i + 1);
        for (int j = 0; j < NUMBER_OF_DAYS; j++)
        {
            printf("%f ", demand_avg_power_W[i][j]);
        }
        printf("\n");
    }
    printf("\n");

    // Print maximum enabled duration
    printf("Maximum enabled duration (hours):\n");
    for (int i = 0; i < NUMBER_OF_LOADS; i++)
    {
        printf("Load #%d: ", i + 1);
        for (int j = 0; j < NUMBER_OF_DAYS; j++)
        {
            printf("%f ", enable_duration_max[i][j]);
        }
        printf("\n");
    }
    printf("\n");

    // Calculate the benefit-per-unit-power ratios
    float benefit_per_unit_power[NUMBER_OF_LOADS][NUMBER_OF_DAYS];
    calc_benefits(load_pf, enable_duration_max, demand_avg_power_W, benefit_per_unit_power);

    // Print benefit-per-unit-power ratios
    printf("Benefit-per-unit-power ratios:\n");
    for (int i = 0; i < NUMBER_OF_LOADS; i++)
    {
        printf("Load #%d: ", i + 1);
        for (int j = 0; j < NUMBER_OF_DAYS; j++)
        {
            printf("%f ", benefit_per_unit_power[i][j]);
        }
        printf("\n");
    }
    printf("\n");

    // Run the algorithm to compute the thresholds
    float daily_virtual_recharge[NUMBER_OF_DAYS];
    float initial_wallet_balance;
    thresholds(demand_avg_power_W, benefit_per_unit_power, enable_duration_max, threshold, daily_virtual_recharge, initial_wallet_balance);

    // Prepare real wallet parameters for simulation
    float daily_real_recharge[NUMBER_OF_DAYS] = {0.0};
    float daily_real_cost[NUMBER_OF_DAYS] = {0.0};
    float daily_virtual_cost[NUMBER_OF_DAYS] = {0.0};
    daily_real_recharge[0] = initial_wallet_balance; // Recharge full amount on day 1
    // No fixed costs for now (all zeros)

    printf("\nInitial Wallet Balance:\n\t%f\n", initial_wallet_balance);

    printf("\n=== Running Simulation with AFG Thresholds ===\n");
    run_simulation(threshold, daily_virtual_recharge, daily_virtual_cost, g_demand_power_W, g_demand_state, daily_real_recharge, daily_real_cost, load_priority_order);
}

// AFG algorithm
void thresholds(const float (&demand_avg_power_W)[NUMBER_OF_LOADS][NUMBER_OF_DAYS],
                const float (&benefit_per_unit_power)[NUMBER_OF_LOADS][NUMBER_OF_DAYS],
                const float (&enable_duration_max)[NUMBER_OF_LOADS][NUMBER_OF_DAYS],
                float threshold[NUMBER_OF_LOADS][NUMBER_OF_DAYS],
                float daily_virtual_recharge[NUMBER_OF_DAYS],
                float &initial_wallet_balance)
{
    // Flatten inputs
    float benefit_per_unit_power_1D[NUMBER_LOAD_DAYS];
    float demand_avg_power_W_1D[NUMBER_LOAD_DAYS];
    float enable_duration_max_1D[NUMBER_LOAD_DAYS];
    flattenArray(benefit_per_unit_power, benefit_per_unit_power_1D);
    flattenArray(demand_avg_power_W, demand_avg_power_W_1D);
    flattenArray(enable_duration_max, enable_duration_max_1D);
    int indices[NUMBER_LOAD_DAYS];

    // Print flattened array
    printf("Benefits before sorting:\n");
    for (int i = 0; i < NUMBER_LOAD_DAYS; i++)
    {
        printf("%f ", benefit_per_unit_power_1D[i]);
    }
    printf("\n\n");

    // Sort benefit ratios in non-increasing order
    sort_benefits(benefit_per_unit_power_1D, indices, NUMBER_LOAD_DAYS);

    // Print benefits after sorting
    printf("Benefits after sorting:\n");
    for (int i = 0; i < NUMBER_LOAD_DAYS; i++)
    {
        printf("%f ", benefit_per_unit_power_1D[i]);
    }
    printf("\n\n");

    printf("Permutation indices:\n");
    for (int i = 0; i < NUMBER_LOAD_DAYS; i++)
    {
        printf("%d ", indices[i]);
    }
    printf("\n\n");

    // Calculate total monthly energy consumption in Wh
    float total_monthly_energy_Wh = 0.0;
    for (int i = 0; i < NUMBER_LOAD_DAYS; i++)
    {
        total_monthly_energy_Wh += enable_duration_max_1D[i] * demand_avg_power_W_1D[i];
    }

    // Calculate wallet balance based on recharge percentage
    initial_wallet_balance = COST_PER_WH * total_monthly_energy_Wh * (RECHARGE_PERCENT / 100.0);

    float enable_duration_1D[NUMBER_LOAD_DAYS] = {0.0}; // initialize all durations to 0
    float tolerance = 1e-4;
    for (int i = 0; i < NUMBER_LOAD_DAYS; i++)
    {
        // Use the maximum enable duration for this load/day
        enable_duration_1D[indices[i]] = enable_duration_max_1D[indices[i]];

        // Calculate total cost - sum only over the sorted indices we've enabled so far
        float temp_sum = 0.0;
        for (int j = 0; j < NUMBER_LOAD_DAYS; j++)
        {
            temp_sum += enable_duration_1D[indices[j]] * demand_avg_power_W_1D[indices[j]];
        }

        float total_cost = COST_PER_WH * temp_sum;

        if (total_cost >= (initial_wallet_balance - tolerance)) // if the total cost exceeds (or is very close to) the available wallet balance
        {
            enable_duration_1D[indices[i]] = 0; // disable load to check impact

            // Recalculate total cost with load disabled
            temp_sum = 0.0;
            for (int j = 0; j < NUMBER_LOAD_DAYS; j++)
            {
                temp_sum += enable_duration_1D[indices[j]] * demand_avg_power_W_1D[indices[j]];
            }

            // Set load to match remaining wallet balance
            enable_duration_1D[indices[i]] = ((initial_wallet_balance - tolerance) / COST_PER_WH - temp_sum) / demand_avg_power_W_1D[indices[i]];
            break;
        }
    }

    // Make 2d array
    float enable_duration[NUMBER_OF_LOADS][NUMBER_OF_DAYS];
    regenerateArray(enable_duration, enable_duration_1D);

    // Print duration of each load each day
    printf("Enable durations:\n");
    for (int i = 0; i < NUMBER_OF_LOADS; i++)
    {
        printf("Load #%d: ", i + 1);
        for (int j = 0; j < NUMBER_OF_DAYS; j++)
        {
            printf("%f ", enable_duration[i][j]);
        }
        printf("\n");
    }
    printf("\n");

    // Calculate daily_virtual_recharge
    for (int j = 0; j < NUMBER_OF_DAYS; j++)
    {
        daily_virtual_recharge[j] = 0.0;
        for (int i = 0; i < NUMBER_OF_LOADS; i++)
        {
            daily_virtual_recharge[j] += COST_PER_WH * enable_duration[i][j] * demand_avg_power_W[i][j]; // add the cost for load i on day j to the daily total
        }
    }

    printf("Daily Virtual Recharge:\n");
    for (int i = 0; i < NUMBER_OF_DAYS; i++)
    {
        printf("%f", daily_virtual_recharge[i]);
    }
    printf("\n\n");

    const float threshold_epsilon = 1e-4f;
    for (int j = 0; j < NUMBER_OF_DAYS; j++)
    {
        // Default disabled-load threshold: slightly above daily recharge.
        for (int i = 0; i < NUMBER_OF_LOADS; i++)
        {
            threshold[i][j] = 0.0001f + daily_virtual_recharge[j];
        }

        // Sort loads by ascending enable duration for this day.
        int sorted_indices[NUMBER_OF_LOADS];
        for (int i = 0; i < NUMBER_OF_LOADS; i++)
        {
            sorted_indices[i] = i;
        }
        for (int a = 0; a < NUMBER_OF_LOADS - 1; a++)
        {
            for (int b = a + 1; b < NUMBER_OF_LOADS; b++)
            {
                if (enable_duration[sorted_indices[a]][j] > enable_duration[sorted_indices[b]][j])
                {
                    int temp = sorted_indices[a];
                    sorted_indices[a] = sorted_indices[b];
                    sorted_indices[b] = temp;
                }
            }
        }

        // Keep only loads with non-zero enabled duration.
        int active_indices[NUMBER_OF_LOADS];
        int active_count = 0;
        for (int p = 0; p < NUMBER_OF_LOADS; p++)
        {
            int load_idx = sorted_indices[p];
            if (enable_duration[load_idx][j] > threshold_epsilon)
            {
                active_indices[active_count++] = load_idx;
            }
        }

        if (active_count == 0)
        {
            continue;
        }

        // Suffix sums of average power over active loads in ascending enable-duration order.
        float suffix_power_sum[NUMBER_OF_LOADS] = {0.0f};
        for (int p = active_count - 1; p >= 0; p--)
        {
            suffix_power_sum[p] = demand_avg_power_W[active_indices[p]][j];
            if (p + 1 < active_count)
            {
                suffix_power_sum[p] += suffix_power_sum[p + 1];
            }
        }

        // Align with the current C++ wallet convention used by simulation.
        float virtual_wallet_balance = daily_virtual_recharge[j];

        int first_idx = active_indices[0];
        threshold[first_idx][j] = virtual_wallet_balance -
                                  enable_duration[first_idx][j] * COST_PER_WH * suffix_power_sum[0];

        for (int p = 1; p < active_count; p++)
        {
            int prev_idx = active_indices[p - 1];
            int curr_idx = active_indices[p];
            float enable_delta = enable_duration[curr_idx][j] - enable_duration[prev_idx][j];
            threshold[curr_idx][j] = threshold[prev_idx][j] -
                                     enable_delta * COST_PER_WH * suffix_power_sum[p];
        }
    }

    // Print updated thresholds
    printf("Thresholds:\n");
    for (int i = 0; i < NUMBER_OF_LOADS; i++)
    {
        printf("Load #%d ", i + 1);
        for (int j = 0; j < NUMBER_OF_DAYS; j++)
        {
            printf("%f ", threshold[i][j]);
        }
        printf("\n");
    }
}

// Sorts the benefit per unit cost ratios in descending order
void sort_benefits(float *arr, int *indices, int len)
{
    struct IndexValue indexedArray[len];

    // Create an IndexValue pair for each ratio
    for (int i = 0; i < len; i++)
    {
        indexedArray[i].value = arr[i];
        indexedArray[i].index = i;
    }

    // Sort the IndexValue pairs in decreasing order
    qsort(indexedArray, len, sizeof(struct IndexValue), compare);

    // Put the sorted ratios in arr and record their corresponding indices
    for (int i = 0; i < len; i++)
    {
        arr[i] = indexedArray[i].value;
        indices[i] = indexedArray[i].index;
    }
}

// Helper function to determine qsort order (descending)
int compare(const void *a, const void *b)
{
    struct IndexValue *ia = (struct IndexValue *)a;
    struct IndexValue *ib = (struct IndexValue *)b;
    // -1 -> a<b
    // 0 -> a==b
    // 1 -> a>b
    return -((ia->value > ib->value) - (ia->value < ib->value));
}

// Dot product of two arrays
float dot_prod(float *arr1, float *arr2, int len)
{
    float temp_sum = 0.0;
    for (int i = 0; i < len; i++)
    {
        temp_sum += arr1[i] * arr2[i];
    }
    return temp_sum;
}

// Transform 2-d array into 1-d array
void flattenArray(const float (&arr)[NUMBER_OF_LOADS][NUMBER_OF_DAYS], float *arr_1D)
{
    int k = 0;
    for (int j = 0; j < NUMBER_OF_DAYS; j++)
    {
        for (int i = 0; i < NUMBER_OF_LOADS; i++)
        {
            arr_1D[k] = arr[i][j];
            k++;
        }
    }
}

// Transform 1-d array to 2-d array
void regenerateArray(float arr[NUMBER_OF_LOADS][NUMBER_OF_DAYS], float *arr_1D)
{
    int k = 0;
    for (int j = 0; j < NUMBER_OF_DAYS; j++)
    {
        for (int i = 0; i < NUMBER_OF_LOADS; i++)
        {
            arr[i][j] = arr_1D[k++];
        }
    }
}

// Calculate the benefit_per_unit_power
void calc_benefits(const float (&load_pf)[NUMBER_OF_LOADS],
                   const float (&enable_duration_max)[NUMBER_OF_LOADS][NUMBER_OF_DAYS],
                   const float (&demand_avg_power_W)[NUMBER_OF_LOADS][NUMBER_OF_DAYS],
                   float benefit_per_unit_power[NUMBER_OF_LOADS][NUMBER_OF_DAYS])
{
    // Initialize a variable to store the sum for each load
    float enable_sum = 0.0;
    for (int i = 0; i < NUMBER_OF_LOADS; i++)
    {
        // Calculate the enable_sum across all days for this load
        enable_sum = 0.0;
        for (int j = 0; j < NUMBER_OF_DAYS; j++)
        {
            enable_sum += enable_duration_max[i][j];
        }

        // Calculate the benefit_per_unit_power for each day
        for (int j = 0; j < NUMBER_OF_DAYS; j++)
        {
            if (demand_avg_power_W[i][j] > 0)
            {
                benefit_per_unit_power[i][j] = load_pf[i] / (enable_sum * demand_avg_power_W[i][j]);
            }
            else
            {
                benefit_per_unit_power[i][j] = 0.0;
            }
        }
    }
}

// Calculate demand_avg_power_W
void calc_dem_avg(const std::array<load, size_t(NUMBER_OF_LOADS)> &demand_power_W,
                  const std::array<std::array<int, size_t(NUM_TIMESTEPS)>, size_t(NUMBER_OF_LOADS)> &demand_state,
                  float demand_avg_power_W[NUMBER_OF_LOADS][NUMBER_OF_DAYS],
                  float enable_duration_max[NUMBER_OF_LOADS][NUMBER_OF_DAYS])
{
    // Fill in day_start_timesteps
    int day_start_timesteps[NUMBER_OF_DAYS];
    for (int i = 0; i < NUMBER_OF_DAYS; i++)
    {
        // day_start_timesteps[i] = (24 / TIMESTEP_HOURS) * (i - 1) + 1;
        day_start_timesteps[i] = (24 / TIMESTEP_HOURS) * i;
    }

    // Compute averages over each day (24 hours)
    int timesteps_per_day = (int)(24 / TIMESTEP_HOURS);
    for (int j = 0; j < NUMBER_OF_DAYS; j++)
    {
        int day_start = day_start_timesteps[j];
        int day_end = day_start + timesteps_per_day;

        for (int i = 0; i < NUMBER_OF_LOADS; i++)
        {
            // Calculate the mean of (demand_power * demand_state) over the day
            float sum = 0.0;
            int count = 0;
            for (int t = day_start; t < day_end && t < int(demand_power_W[i].count); t++)
            {
                // Multiply by demand_state to match Julia's calculation
                sum += demand_power_W[i].values[t] * demand_state[i][t];
                count++;
            }

            if (count > 0)
            {
                demand_avg_power_W[i][j] = sum / count;
            }
            else
            {
                demand_avg_power_W[i][j] = 0.0;
            }

            if (demand_avg_power_W[i][j] > 0)
            {
                enable_duration_max[i][j] = 24;
            }
            else
            {
                enable_duration_max[i][j] = 0;
            }
        }
    }
}

// Reads a single line from an SD File into a std::string, stripping \r
static bool sdReadLine(File &file, std::string &line)
{
    line = "";
    if (!file.available())
        return false;
    while (file.available())
    {
        char c = (char)file.read();
        if (c == '\n')
            break;
        if (c != '\r')
            line += c;
    }
    return true;
}

// Returns the denoise percentage threshold for a given load name.
static float get_load_per(const std::string &load_name)
{
    if (load_name == "air1")
        return 0.01f;
    return 0.1f; // clotheswasher_dryg1, microwave1, refrigerator1
}

// Extracts float values from a comma-separated line at the specified column indices (0-based for load k).
static void parseCSVFields(const std::string &line, const size_t col_indices[NUMBER_OF_LOADS], float out[NUMBER_OF_LOADS])
{
    // Find the highest column index we need so we can stop early
    size_t max_col = 0;
    for (int k = 0; k < NUMBER_OF_LOADS; k++)
    {
        if (col_indices[k] > max_col)
        {
            max_col = col_indices[k];
        }
    }

    size_t field = 0;
    char buf[32];
    size_t buf_len = 0;
    const size_t len = line.size();

    for (size_t ci = 0; ci <= len && field <= max_col; ci++)
    {
        char c = (ci < len) ? line[ci] : ',';
        if (c == ',')
        {
            for (int k = 0; k < NUMBER_OF_LOADS; k++)
            {
                if (col_indices[k] == field)
                {
                    buf[buf_len] = '\0';
                    out[k] = (buf_len > 0) ? (float)atof(buf) : 0.0f;
                }
            }
            field++;
            buf_len = 0;
        }
        else
        {
            if (buf_len < sizeof(buf) - 1)
            {
                buf[buf_len++] = c;
            }
        }
    }
}

// Two-pass method for reading csv from SD card and filling in demand_avg_power_W, enable_duration_max, demand_power_W, demand_state
void stream_demand_avg(const std::string &filename,
                       const std::array<std::string, size_t(NUMBER_OF_LOADS)> &load_names,
                       const int &start_day,
                       float demand_avg_power_W[NUMBER_OF_LOADS][NUMBER_OF_DAYS],
                       float enable_duration_max[NUMBER_OF_LOADS][NUMBER_OF_DAYS],
                       std::array<load, size_t(NUMBER_OF_LOADS)> *out_demand_power_W,
                       std::array<std::array<int, size_t(NUM_TIMESTEPS)>, size_t(NUMBER_OF_LOADS)> *out_demand_state)
{
    const int timesteps_per_day = (int)(24.0f / TIMESTEP_HOURS);
    const int start_row = (start_day - 1) * timesteps_per_day; // 0-based data rows after header
    size_t col_indices[NUMBER_OF_LOADS] = {0};

    // Pre-step: resolve column indices from header
    {
        File file = SD.open(filename.c_str());
        if (!file)
        {
            printf("stream_demand_avg ERROR: Cannot open %s\n", filename.c_str());
            return;
        }
        std::string line;
        sdReadLine(file, line);
        file.close();

        size_t field = 0;
        std::string cell;
        const size_t len = line.size();
        for (size_t ci = 0; ci <= len; ci++)
        {
            char c = (ci < len) ? line[ci] : ',';
            if (c == ',')
            {
                for (int k = 0; k < NUMBER_OF_LOADS; k++)
                {
                    if (cell == load_names[k])
                    {
                        col_indices[k] = field;
                        printf("stream_demand_avg: '%s' -> col %zu\n", cell.c_str(), field);
                    }
                }
                cell = "";
                field++;
            }
            else
            {
                cell += c;
            }
        }
    }

    // Pass 1: find per-load global max (in W) across the time window
    float load_max[NUMBER_OF_LOADS] = {0.0f};
    {
        File file = SD.open(filename.c_str());
        if (!file)
        {
            printf("stream_demand_avg ERROR: Cannot open %s (pass 1)\n", filename.c_str());
            return;
        }
        std::string line;
        sdReadLine(file, line); // skip header
        for (int r = 0; r < start_row && file.available(); r++)
            sdReadLine(file, line); // skip rows before start_day

        for (int t = 0; t < NUM_TIMESTEPS && file.available(); t++)
        {
            sdReadLine(file, line);
            float vals[NUMBER_OF_LOADS] = {0.0f};
            parseCSVFields(line, col_indices, vals);
            for (int k = 0; k < NUMBER_OF_LOADS; k++)
            {
                float w = vals[k] * 1000.0f; // kW -> W
                if (w > load_max[k])
                    load_max[k] = w;
            }
        }
        file.close();
    }
    printf("stream_demand_avg load maxima (W):");
    for (int k = 0; k < NUMBER_OF_LOADS; k++)
        printf(" %.2f", load_max[k]);
    printf("\n");

    // Pass 2: compute daily averages with inline denoise
    // Optionally also fill demand_power_W and demand_state in the same pass.
    float per[NUMBER_OF_LOADS];
    for (int k = 0; k < NUMBER_OF_LOADS; k++)
        per[k] = get_load_per(load_names[k]);

    // Initialise load metadata if caller wants demand_power_W populated
    if (out_demand_power_W)
    {
        for (int k = 0; k < NUMBER_OF_LOADS; k++)
        {
            (*out_demand_power_W)[k].name = load_names[k];
            (*out_demand_power_W)[k].count = (size_t)NUM_TIMESTEPS;
        }
    }

    {
        File file = SD.open(filename.c_str());
        if (!file)
        {
            printf("stream_demand_avg ERROR: Cannot open %s (pass 2)\n", filename.c_str());
            return;
        }
        std::string line;
        sdReadLine(file, line); // skip header
        for (int r = 0; r < start_row && file.available(); r++)
            sdReadLine(file, line); // skip rows before start_day

        for (int j = 0; j < NUMBER_OF_DAYS; j++)
        {
            float day_sum[NUMBER_OF_LOADS] = {0.0f};
            int count = 0;
            for (int ts = 0; ts < timesteps_per_day && file.available(); ts++)
            {
                sdReadLine(file, line);
                float vals[NUMBER_OF_LOADS] = {0.0f};
                parseCSVFields(line, col_indices, vals);
                int t_abs = j * timesteps_per_day + ts; // absolute timestep index
                for (int k = 0; k < NUMBER_OF_LOADS; k++)
                {
                    float w = vals[k] * 1000.0f; // kW -> W
                    // Inline denoise
                    float dn_threshold = per[k] * load_max[k];
                    float numerator = w - dn_threshold;
                    float denominator = fabsf(numerator) + 0.001f;
                    float calc = ceilf(numerator / denominator); // 0 or 1
                    float denoised = w * ((calc > 0.0f) ? calc : 0.0f);
                    int state = (calc > 0.0f) ? 1 : 0;
                    day_sum[k] += denoised;
                    // Optionally fill simulation buffers at the same time
                    if (out_demand_power_W)
                        (*out_demand_power_W)[k].values[t_abs] = denoised;
                    if (out_demand_state)
                        (*out_demand_state)[k][t_abs] = state;
                }
                count++;
            }
            for (int k = 0; k < NUMBER_OF_LOADS; k++)
            {
                demand_avg_power_W[k][j] = (count > 0) ? day_sum[k] / count : 0.0f;
                enable_duration_max[k][j] = (demand_avg_power_W[k][j] > 0.0f) ? 24.0f : 0.0f;
            }
        }
        file.close();
    }
    printf("stream_demand_avg complete.\n");
}

// Calculate the load priority factors
void calc_load_pf(const int (&load_priority_order)[NUMBER_OF_LOADS],
                  float load_pf[NUMBER_OF_LOADS])
{
    // Calculate the factor
    float factor = 0.0;
    for (int i = 0; i < NUMBER_OF_LOADS; i++)
    {
        factor += 1.0 / load_priority_order[i];
    }
    factor = 1.0 / factor;

    // Calculate the pf for each load
    for (int i = 0; i < NUMBER_OF_LOADS; i++)
    {
        load_pf[i] = factor / load_priority_order[i];
    }
}