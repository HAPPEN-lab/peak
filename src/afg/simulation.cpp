#include "afg.hpp"

static float s_real_wallet[NUM_TIMESTEPS];
static float s_virtual_wallet[NUM_TIMESTEPS];
static int s_actuation_state[NUMBER_OF_LOADS][NUM_TIMESTEPS];
static int s_virtual_enable[NUMBER_OF_LOADS][NUM_TIMESTEPS];
static int s_real_enable[NUMBER_OF_LOADS][NUM_TIMESTEPS];

// Simulation function that uses AFG thresholds to control loads
void run_simulation(
    float threshold[NUMBER_OF_LOADS][NUMBER_OF_DAYS],
    float daily_virtual_recharge[NUMBER_OF_DAYS],
    float daily_virtual_cost[NUMBER_OF_DAYS],
    std::array<load, size_t(NUMBER_OF_LOADS)> &demand_power_W,
    std::array<std::array<int, size_t(NUM_TIMESTEPS)>, size_t(NUMBER_OF_LOADS)> &demand_state,
    float daily_real_recharge[NUMBER_OF_DAYS],
    float daily_real_cost[NUMBER_OF_DAYS],
    const int (&load_priority_order)[NUMBER_OF_LOADS])
{
    // Zero-initialize the static simulation arrays for this run
    memset(s_real_wallet, 0, sizeof(s_real_wallet));
    memset(s_virtual_wallet, 0, sizeof(s_virtual_wallet));
    memset(s_actuation_state, 0, sizeof(s_actuation_state));
    memset(s_virtual_enable, 0, sizeof(s_virtual_enable));
    memset(s_real_enable, 0, sizeof(s_real_enable));

    // Convenience aliases so the rest of the function is unchanged
    float (&real_wallet)[NUM_TIMESTEPS] = s_real_wallet;
    float (&virtual_wallet)[NUM_TIMESTEPS] = s_virtual_wallet;
    int (&actuation_state)[NUMBER_OF_LOADS][NUM_TIMESTEPS] = s_actuation_state;
    int (&virtual_enable)[NUMBER_OF_LOADS][NUM_TIMESTEPS] = s_virtual_enable;
    int (&real_enable)[NUMBER_OF_LOADS][NUM_TIMESTEPS] = s_real_enable;

    // Day start indices
    int day_start_timesteps[NUMBER_OF_DAYS];
    for (int d = 0; d < NUMBER_OF_DAYS; d++)
    {
        day_start_timesteps[d] = (int)((24.0 / TIMESTEP_HOURS) * d);
    }

    const float wallet_zero_epsilon = 1.25e-4f;

    // Run simulation timestep by timestep
    for (int t = 0; t < NUM_TIMESTEPS - 1; t++)
    {
        // Check if this is start of a new day
        bool is_day_start = false;
        int current_day = -1;
        for (int d = 0; d < NUMBER_OF_DAYS; d++)
        {
            if (t == day_start_timesteps[d])
            {
                is_day_start = true;
                current_day = d;
                break;
            }
        }

        // Daily updates
        if (is_day_start)
        {
            // Add daily recharge and subtract daily costs
            real_wallet[t] += daily_real_recharge[current_day] - daily_real_cost[current_day];
            virtual_wallet[t] += daily_virtual_recharge[current_day] - daily_virtual_cost[current_day];
        }

        // Determine current day for threshold lookup
        int day_index = (int)(t * TIMESTEP_HOURS / 24.0);

        // Determine virtual enable for each load
        for (int k = 0; k < NUMBER_OF_LOADS; k++)
        {
            if (virtual_wallet[t] >= threshold[k][day_index])
            {
                virtual_enable[k][t] = 1;
            }
            else
            {
                virtual_enable[k][t] = 0;
            }
        }

        // Determine real enable (all loads if real wallet > 0)
        if (real_wallet[t] > 0)
        {
            for (int k = 0; k < NUMBER_OF_LOADS; k++)
            {
                real_enable[k][t] = 1;
            }
        }
        else
        {
            for (int k = 0; k < NUMBER_OF_LOADS; k++)
            {
                real_enable[k][t] = 0;
            }
        }

        // Compute actuation state = virtual_enable AND real_enable AND demand_state
        for (int k = 0; k < NUMBER_OF_LOADS; k++)
        {
            actuation_state[k][t] = virtual_enable[k][t] * real_enable[k][t] * demand_state[k][t];
        }

        // Calculate energy cost for this timestep
        double energy_cost = 0.0;
        for (int k = 0; k < NUMBER_OF_LOADS; k++)
        {
            energy_cost += actuation_state[k][t] * demand_power_W[k].values[t] * TIMESTEP_HOURS * COST_PER_WH;
        }

        // Update wallets for next timestep
        virtual_wallet[t + 1] = virtual_wallet[t] - energy_cost;
        real_wallet[t + 1] = real_wallet[t] - energy_cost;

        // Clamp tiny negative drift around zero to improve parity with Julia Float64 behavior.
        if (virtual_wallet[t + 1] < 0.0f && fabs(virtual_wallet[t + 1]) < wallet_zero_epsilon)
        {
            virtual_wallet[t + 1] = 0.0f;
        }
        if (real_wallet[t + 1] < 0.0f && fabs(real_wallet[t + 1]) < wallet_zero_epsilon)
        {
            real_wallet[t + 1] = 0.0f;
        }

        // Disconnection prevention
        if (real_wallet[t + 1] < 0)
        {
            // Turn off all loads
            for (int k = 0; k < NUMBER_OF_LOADS; k++)
            {
                actuation_state[k][t] = 0;
            }

            // Recalculate with all loads off
            energy_cost = 0.0;
            virtual_wallet[t + 1] = virtual_wallet[t];
            real_wallet[t + 1] = real_wallet[t];
        }
    }

    // Compute actuation state for the last timestep
    int last_t = NUM_TIMESTEPS - 1;
    int last_day_index = (int)(last_t * TIMESTEP_HOURS / 24.0);

    for (int k = 0; k < NUMBER_OF_LOADS; k++)
    {
        virtual_enable[k][last_t] = (virtual_wallet[last_t] >= threshold[k][last_day_index]) ? 1 : 0;
        real_enable[k][last_t] = (real_wallet[last_t] > 0) ? 1 : 0;
        actuation_state[k][last_t] = virtual_enable[k][last_t] * real_enable[k][last_t] * demand_state[k][last_t];
    }

    // Calculate service factors
    float time_service_factor[NUMBER_OF_LOADS] = {0.0};
    float daily_load_sf[NUMBER_OF_LOADS][NUMBER_OF_DAYS] = {{0.0f}};
    float daily_psf[NUMBER_OF_DAYS] = {0.0f};
    int total_demand_timesteps[NUMBER_OF_LOADS] = {0};
    int total_actuation_timesteps[NUMBER_OF_LOADS] = {0};

    // Load priority setup
    float sum_inv_priority = 0.0;
    for (int k = 0; k < NUMBER_OF_LOADS; k++)
    {
        sum_inv_priority += 1.0 / load_priority_order[k];
    }
    float pf_factor = 1.0 / sum_inv_priority;

    float load_pf[NUMBER_OF_LOADS];
    for (int k = 0; k < NUMBER_OF_LOADS; k++)
    {
        load_pf[k] = pf_factor / load_priority_order[k];
    }

    float priority_weighted_tsf = 0.0;

    for (int k = 0; k < NUMBER_OF_LOADS; k++)
    {
        for (int t = 0; t < NUM_TIMESTEPS; t++)
        {
            total_demand_timesteps[k] += demand_state[k][t];
            total_actuation_timesteps[k] += actuation_state[k][t];
        }

        if (total_demand_timesteps[k] > 0)
        {
            time_service_factor[k] = 100.0 * total_actuation_timesteps[k] / total_demand_timesteps[k];
        }
        
        priority_weighted_tsf += load_pf[k] * time_service_factor[k];
    }

            for (int d = 0; d < NUMBER_OF_DAYS; d++)
            {
                int day_begin = day_start_timesteps[d];
                int day_end = day_begin + (int)(24.0 / TIMESTEP_HOURS);
                if (day_end > NUM_TIMESTEPS)
                {
                    day_end = NUM_TIMESTEPS;
                }

                for (int k = 0; k < NUMBER_OF_LOADS; k++)
                {
                    int day_demand_timesteps = 0;
                    int day_actuation_timesteps = 0;
                    for (int t = day_begin; t < day_end; t++)
                    {
                        day_demand_timesteps += demand_state[k][t];
                        day_actuation_timesteps += actuation_state[k][t];
                    }

                    if (day_demand_timesteps == 0)
                    {
                        daily_load_sf[k][d] = -1.0f;
                    }
                    else
                    {
                        daily_load_sf[k][d] = 100.0f * day_actuation_timesteps / day_demand_timesteps;
                    }
                }

                for (int k = 0; k < NUMBER_OF_LOADS; k++)
                {
                    if (daily_load_sf[k][d] < 0.0f)
                    {
                        daily_psf[d] += load_pf[k] * 100.0f;
                    }
                    else
                    {
                        daily_psf[d] += load_pf[k] * daily_load_sf[k][d];
                    }
                }
            }

    // Print simulation results
    printf("\n=== Simulation Results ===\n");

    printf("\nTime Service Factors (%%):\n");
    for (int k = 0; k < NUMBER_OF_LOADS; k++)
    {
        printf("\tLoad %d (%s): %.2f%% (%d/%d timesteps)\n",
               k + 1, demand_power_W[k].name.c_str(), time_service_factor[k],
               total_actuation_timesteps[k], total_demand_timesteps[k]);
    }
    printf("\nPriority Weighted Time Service Factor: %f%%\n", priority_weighted_tsf);


    // Count disconnections
    int disconnection_count = 0;
    for (int t = 0; t < NUM_TIMESTEPS; t++)
    {
        if (t == 0)
        {
            if (real_wallet[t] < 0)
            {
                disconnection_count++;
            }
        }
        else
        {
            if (real_wallet[t - 1] >= 0 && real_wallet[t] < 0)
            {
                disconnection_count++;
            }
        }
    }

    printf("\nDisconnections: %d\n", disconnection_count);

    // Print final wallet balances
    printf("\nFinal Balances:\n");
    printf("\tReal Wallet: $%.4f\n", real_wallet[NUM_TIMESTEPS - 1]);
    printf("\tVirtual Wallet: $%.4f\n", virtual_wallet[NUM_TIMESTEPS - 1]);
}

