from pathlib import Path
from typing import Optional
import pandas as pd
import math as m

class PEAK:
    MONTHLY_REAL_RECHARGE_SCHEDULE = {
        1: [[1, 1]],
        2: [[1, 0.5], [15, 0.5]],
        21: [[1, 0.75], [22, 0.25]],
        3: [[1, 0.33], [11, 0.33], [21, 0.34]],
        4: [[1, 0.25], [8, 0.25], [15, 0.25], [22, 0.25]],
        5: [[1, 0.2], [7, 0.2], [13, 0.2], [19, 0.2], [25, 0.2]],
        6: [[1, 0.16], [6, 0.16], [11, 0.17], [16, 0.17], [21, 0.17], [26, 0.17]],
        7: [[1, 0.14], [5, 0.14], [9, 0.14], [13, 0.14], [17, 0.14], [21, 0.15], [25, 0.15]],
    }

    def __init__(
            self,
            start_day: int,
            load_priority_order: list[int],
            data_path: Path,
            *,
            cols: Optional[list[str]] = None,
            number_of_loads: int = 4,
            number_of_days: int = 30,
            recharge_percent: float = 90.0,
            real_recharge_logic: int = 1,
            timestep_hours: float = 0.25,
            threshold_constant_days: int = 1,
            solver: str='highs',
    ):
        self.start_day = start_day
        self.load_priority_order = load_priority_order

        # Configurable parameters (defaults match original class constants)
        self.number_of_loads = number_of_loads
        self.number_of_days = number_of_days
        self.recharge_percent = recharge_percent
        self.real_recharge_logic = real_recharge_logic
        self.timestep_hours = timestep_hours
        self.threshold_constant_days = threshold_constant_days
        self.solver = solver

        # Derived parameters
        self.sim_months = self.number_of_days // 30
        self.number_load_days = self.number_of_loads * self.number_of_days
        self.num_timesteps = int(self.number_of_days * (24 / self.timestep_hours))

        self.recharge_params = Recharge_Parameters(
            number_of_days=self.number_of_days,
            sim_months=self.sim_months,
        )
        self.rates = Rates()
        self.init(data_path, cols)

    def init(self, json_path: Path, cols: Optional[list[str]]):
        self.load_data(json_path, cols)
        self.format_data()
        self.calc_load_pf()

    def load_data(
            self,
            data_path: Path,
            cols: Optional[list[str]],
    ):
        with open(data_path, 'r') as file:
            steps_per_day = int(24 / self.timestep_hours)
            skip_rows = range(1, (self.start_day - 1) * steps_per_day + 1)  # skip to start_day
            if cols:
                self.df = pd.read_csv(file, header=0, skiprows=skip_rows, nrows=self.num_timesteps, usecols=cols)
            else:
                self.df = pd.read_csv(file, header=0, skiprows=skip_rows, nrows=self.num_timesteps)
                self.df.drop(columns=['dataid', 'local_15min'], inplace=True)
            self.df.reset_index(drop=True, inplace=True)
            self.cols = self.df.columns
    
    def format_data(self):
        self.df = self.df.mul(1000) # convert kW -> W

        for col in self.df.columns: # create demand states and denoise data
            max = self.df[col].max()
            match col:
                case 'air1':
                    per = 0.01
                case 'clotheswasher_dryg1':
                    per = 0.1
                case 'microwave1':
                    per = 0.1
                case 'refrigerator1':
                    per = 0.1
                case _:
                    per = 0.1
            threshold = max * per
            ds_col = f'{col}_ds'    # column for demand state
            self.df[ds_col] = self.df[col].apply(lambda x: m.ceil((x - threshold) / (m.fabs((x - threshold) - 0.001))))
            self.df[ds_col] = self.df[ds_col].apply(lambda x: 1 if x > 0 else 0)
            self.df[col] = self.df[col].mul(self.df[ds_col])
            
    def calc_load_pf(self):
        factor = 1 / sum([1 / i for i in self.load_priority_order])
        self.load_pf = {col: factor / i for col, i in zip(self.cols, self.load_priority_order)}

    def monthly_energy_use(self) -> list[float]:
        timesteps = {month: int(30 * (24 / self.timestep_hours) * (month)) for month in range(self.sim_months + 1)}
        
        # Find demand_energy_Wh
        demand_energy_Wh = {load: [] for load in self.cols}
        for month in range(1, self.sim_months + 1):
            for load in self.cols:
                month_start = timesteps[month - 1]
                month_end = timesteps[month]
                energy = (self.df[load].iloc[month_start:month_end] * self.df[f'{load}_ds'].iloc[month_start:month_end]).sum() * self.timestep_hours
                demand_energy_Wh[load].append(energy)
        
        # Find monthly_energy_Wh
        monthly_energy_kWh = [float(sum(demand_energy_Wh[load][month] for load in demand_energy_Wh) / 1000) for month in range(self.sim_months)]

        return monthly_energy_kWh
    
    def init_real_recharge_params(self, monthly_energy_kWh: list[float]):
        # Calculate static amounts
        monthly_variable_cost = [self.rates.elec_rate * i for i in monthly_energy_kWh]
        self.recharge_params.monthly_recharge_amount = [(self.recharge_percent / 100) * (self.rates.monthly_fixed_cost + i) for i in monthly_variable_cost]
        monthly_real_recharge_schedule = PEAK.MONTHLY_REAL_RECHARGE_SCHEDULE[self.real_recharge_logic]
        
        # Compute schedules
        for month in range(self.sim_months):
            for entry in monthly_real_recharge_schedule:
                day = 30 * month + entry[0]
                amount = self.recharge_params.monthly_recharge_amount[month] * entry[1]
                month_end_day = 30 * (month + 1)
                self.recharge_params.real_recharge_schedule.append([day, amount])
                self.recharge_params.real_cost_schedule.append([month_end_day, self.rates.monthly_fixed_cost])
        
        # Computing daily recharge and cost schedules
        self.recharge_params.daily_real_recharge = [0] * self.number_of_days
        self.recharge_params.daily_real_cost = [0] * self.number_of_days
        for i in range(self.number_of_days):
            for entry in self.recharge_params.real_recharge_schedule:   # check if day has a recharge
                if entry[0] - 1 == i:   # schedule uses 1-indexed days
                    self.recharge_params.daily_real_recharge[i] = entry[1]
                    break
            for entry in self.recharge_params.real_cost_schedule:   # check if day has a deduction
                if entry[0] - 1 == i:   # schedule uses 1-indexed days
                    self.recharge_params.daily_real_cost[i] = entry[1]
                    break
        self.recharge_params.cost_per_kWh = self.rates.elec_rate


class Recharge_Parameters:
    def __init__(self, number_of_days: int = 30, sim_months: int = 1):
        self.daily_real_recharge: list[float] = [0.0] * number_of_days
        self.daily_real_cost: list[float] = [0.0] * number_of_days
        self.cost_per_kWh: float = 0.0
        self.real_wallet_balance: float = 0.0      # $ starting balance for real wallet
        self.virtual_wallet_balance: float = 0.0   # $ starting balance for virtual wallet
        self.real_recharge_schedule: list[list[float]] = []
        self.real_cost_schedule: list[list[float]] = []
        self.monthly_recharge_amount: list[float] = [0.0] * sim_months

class Rates:
    grid_connection_customer_service = 0.56 # $ / day
    distribution_service = 0.050 # $ / kWh
    electricity_service = 0.098 # $ / kWh
    state_tax = 5 # % on total bill
    county_tax = 0.5 # % on total bill
    monthly_fixed_cost = 0
    elec_rate = (distribution_service + electricity_service) * (100 + state_tax + county_tax) / 100 # $ / kWh
    disconnection_cost = 25 # $
    reconnection_cost = 25 # $