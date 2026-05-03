#ifndef AFG_HPP
#define AFG_HPP

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <vector>
#include <array>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <SD.h>
// #include "../util/util.hpp"

// Define constants
#define NUMBER_OF_LOADS 4
#define NUMBER_OF_DAYS 30
#define SIM_MONTHS (NUMBER_OF_DAYS / 30)
#define NUMBER_LOAD_DAYS (NUMBER_OF_LOADS * NUMBER_OF_DAYS)
#define COST_PER_WH 0.00015614
#define RECHARGE_PERCENT 70.0
#define TIMESTEP_HOURS 0.25
#define NUM_TIMESTEPS (int)(NUMBER_OF_DAYS * (24 / TIMESTEP_HOURS))

// Define structs
struct IndexValue
{
    int index;
    float value;
};

struct load
{
    std::string name;
    std::array<float, size_t(NUM_TIMESTEPS)> values;
    size_t count = 0; // Number of values loaded
};

// Function declarations
void AFG(const int &start_day, const int (&load_priority_order)[NUMBER_OF_LOADS], float (&threshold)[NUMBER_OF_LOADS][NUMBER_OF_DAYS], float (&enable_duration_max)[NUMBER_OF_LOADS][NUMBER_OF_DAYS]);
void thresholds(const float (&demand_avg_power_W)[NUMBER_OF_LOADS][NUMBER_OF_DAYS], const float (&benefit_per_unit_power)[NUMBER_OF_LOADS][NUMBER_OF_DAYS], const float (&enable_duration_max)[NUMBER_OF_LOADS][NUMBER_OF_DAYS], float threshold[NUMBER_OF_LOADS][NUMBER_OF_DAYS], float daily_virtual_recharge[NUMBER_OF_DAYS], float &initial_wallet_balance);
void sort_benefits(float *arr, int *indices, int len);
int compare(const void *a, const void *b);
float dot_prod(float *arr1, float *arr2, int len);
void flattenArray(const float (&arr)[NUMBER_OF_LOADS][NUMBER_OF_DAYS], float *arr_1D);
void regenerateArray(float arr[NUMBER_OF_LOADS][NUMBER_OF_DAYS], float *arr_1D);
void calc_benefits(const float (&load_pf)[NUMBER_OF_LOADS], const float (&enable_duration_max)[NUMBER_OF_LOADS][NUMBER_OF_DAYS], const float (&demand_avg_power_W)[NUMBER_OF_LOADS][NUMBER_OF_DAYS], float benefit_per_unit_power[NUMBER_OF_LOADS][NUMBER_OF_DAYS]);
void calc_dem_avg(const std::array<load, size_t(NUMBER_OF_LOADS)> &demand_power_W, const std::array<std::array<int, size_t(NUM_TIMESTEPS)>, size_t(NUMBER_OF_LOADS)> &demand_state, float demand_avg_power_W[NUMBER_OF_LOADS][NUMBER_OF_DAYS], float enable_duration_max[NUMBER_OF_LOADS][NUMBER_OF_DAYS]);
void run_simulation(float threshold[NUMBER_OF_LOADS][NUMBER_OF_DAYS], float daily_virtual_recharge[NUMBER_OF_DAYS], float daily_virtual_cost[NUMBER_OF_DAYS], std::array<load, size_t(NUMBER_OF_LOADS)> &demand_power_W, std::array<std::array<int, size_t(NUM_TIMESTEPS)>, size_t(NUMBER_OF_LOADS)> &demand_state, float daily_real_recharge[NUMBER_OF_DAYS], float daily_real_cost[NUMBER_OF_DAYS], const int (&load_priority_order)[NUMBER_OF_LOADS]);
static bool sdReadLine(File &file, std::string &line);
static float get_load_per(const std::string &load_name);
static void parseCSVFields(const std::string &line, const size_t col_indices[NUMBER_OF_LOADS], float out[NUMBER_OF_LOADS]);
void stream_demand_avg(const std::string &filename, const std::array<std::string, size_t(NUMBER_OF_LOADS)> &load_names, const int &start_day, float demand_avg_power_W[NUMBER_OF_LOADS][NUMBER_OF_DAYS], float enable_duration_max[NUMBER_OF_LOADS][NUMBER_OF_DAYS], std::array<load, size_t(NUMBER_OF_LOADS)> *out_demand_power_W = nullptr, std::array<std::array<int, size_t(NUM_TIMESTEPS)>, size_t(NUMBER_OF_LOADS)> *out_demand_state = nullptr);
void calc_load_pf(const int (&load_priority_order)[NUMBER_OF_LOADS], float load_pf[NUMBER_OF_LOADS]);

#endif // AFG_HPP
