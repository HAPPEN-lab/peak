# from __future__ import annotations
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional
import numpy as np
import pandas as pd
import pyomo.environ as pyo
from .base import PEAK

fp = Path(__file__).resolve()

@dataclass
class SimulationResults:
    """All outputs produced by OBM.simulate() and OBM.compute_metrics()."""

    # Model (MILP) outputs - shape [K][T]
    model_actuation_state: list

    # Forward simulation trajectories
    actuation_state: list   # [K][T] - 1 if load k served at timestep t
    real_wallet:     list   # [T+1] - real wallet balance at each timestep

    # Metrics (filled by compute_metrics)
    priority_sf: float = 0.0
    energy_sf: float = 0.0
    per_load_tsf: list = field(default_factory=list)
    model_priority_sf: float = 0.0
    model_energy_sf: float = 0.0
    model_per_load_tsf: list = field(default_factory=list)
    disconnection_counter: int = 0
    solve_time: float = 0.0


class OBM(PEAK):
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
        solver: str='highs',
    ) -> None:
        super().__init__(
            start_day,
            load_priority_order,
            data_path,
            cols=cols,
            number_of_loads=number_of_loads,
            number_of_days=number_of_days,
            recharge_percent=recharge_percent,
            real_recharge_logic=real_recharge_logic,
            timestep_hours=timestep_hours,
            solver=solver,
        )


    def compute_hem_schedule(self) -> tuple:
        T = self.num_timesteps
        K = self.number_of_loads
        rp = self.recharge_params

        demand_P = self.df[list(self.cols)].to_numpy(dtype=float)  # [T, K]
        demand_DS = self.df[[f"{c}_ds" for c in self.cols]].to_numpy(dtype=float)  # [T, K]

        cost_factor = self.timestep_hours * rp.cost_per_kWh / 1000.0  # $ per W per step

        # Energy cost coefficient for each (k, t): cost_factor * P[t, k]
        energy_coeff = [[cost_factor * demand_P[t, k] for k in range(K)] for t in range(T)]

        # Recharge day indices (0-based) and their cumulative amounts
        recharge_days = [
            d for d in range(self.number_of_days)
            if rp.daily_real_recharge[d] != 0.0
        ]
        total_recharge = sum(rp.daily_real_recharge[d] for d in recharge_days)

        steps_per_day = int(round(24.0 / self.timestep_hours))   # 96

        # 0-based timestep *start* of each recharge day
        recharge_timesteps = [d * steps_per_day for d in recharge_days]

        TOLERANCE = 1e-4

        model = pyo.ConcreteModel()

        K_set = range(1, K + 1)
        T_set = range(1, T + 1)

        model.K = pyo.Set(initialize=K_set)
        model.T = pyo.Set(initialize=T_set)

        # Variables
        model.s = pyo.Var(model.K, model.T, domain=pyo.Binary)

        # Constraint (1): actuation cannot exceed demand state
        def c_demand(mdl, k, t):
            return mdl.s[k, t] <= demand_DS[t - 1, k - 1]
        model.c_demand = pyo.Constraint(model.K, model.T, rule=c_demand)

        # Constraint (2): global budget
        @model.Constraint()
        def c_global_budget(mdl):
            return (
                pyo.quicksum(
                    energy_coeff[t - 1][k - 1] * mdl.s[k, t]
                    for k in K_set for t in T_set
                )
                <= total_recharge - TOLERANCE
            )

        # Constraint (3): per-interval cumulative budget
        interval_constraints: list[tuple[int, int, float]] = []  # (t_start, t_end, budget)
        cumulative = 0.0
        for i, rd in enumerate(recharge_days[:-1]):
            cumulative += rp.daily_real_recharge[rd]
            next_ts = recharge_timesteps[i + 1]   # 0-based, exclusive upper bound
            interval_constraints.append((0, next_ts, cumulative))

        if interval_constraints:
            model.interval_idx = pyo.Set(initialize=range(len(interval_constraints)))

            def c_interval_budget(mdl, i):
                _, t_end, budget = interval_constraints[i]
                return (
                    pyo.quicksum(
                        energy_coeff[t - 1][k - 1] * mdl.s[k, t]
                        for k in K_set for t in range(1, t_end + 1)
                    )
                    <= budget - TOLERANCE
                )
            model.c_interval = pyo.Constraint(model.interval_idx, rule=c_interval_budget)

        # Objective: priority-weighted time service factor
        demand_denom = {
            k: float(np.sum(demand_DS[:, k - 1])) + 1e-4
            for k in K_set
        }

        def obj_rule(mdl):
            return pyo.quicksum(
                self.load_pf[self.cols[k - 1]] * (
                    pyo.quicksum(mdl.s[k, t] for t in T_set) / demand_denom[k]
                )
                for k in K_set
            )
        model.obj = pyo.Objective(rule=obj_rule, sense=pyo.maximize)

        solver = pyo.SolverFactory(self.solver)
        solver.options["mip_rel_gap"] = 0.001
        t0 = time.perf_counter()
        solver.solve(model, tee=True)
        solve_time = time.perf_counter() - t0

        def _v(var):
            val = pyo.value(var)
            return val if val is not None else 0.0

        actuation_state = [
            [_v(model.s[k, t]) for t in T_set]
            for k in K_set
        ]

        # Model-side metrics (before applying simulation wallet dynamics)
        priority_sf = 100.0 * sum(
            self.load_pf[self.cols[k - 1]] * (
                sum(_v(model.s[k, t]) for t in T_set) / demand_denom[k]
            )
            for k in K_set
        )

        numerator_E = sum(
            _v(model.s[k, t]) * demand_P[t - 1, k - 1]
            for k in K_set for t in T_set
        )
        denominator_E = sum(
            demand_DS[t - 1, k - 1] * demand_P[t - 1, k - 1]
            for k in K_set for t in T_set
        ) + 1e-10
        energy_sf = 100.0 * numerator_E / denominator_E

        per_load_tsf = [
            100.0 * sum(_v(model.s[k, t]) for t in T_set) / demand_denom[k]
            for k in K_set
        ]

        return actuation_state, priority_sf, energy_sf, per_load_tsf, solve_time

    def simulate(self, model_actuation_state: list[list[float]]) -> SimulationResults:
        T = self.num_timesteps
        K = self.number_of_loads
        rp = self.recharge_params

        demand_DS = self.df[[f"{c}_ds" for c in self.cols]].to_numpy(dtype=float)  # [T, K]

        steps_per_day = int(round(24.0 / self.timestep_hours))   # 96
        cost_factor = self.timestep_hours * rp.cost_per_kWh / 1000.0

        # Simulation arrays
        actuation_state = [[0.0] * T for _ in range(K)]
        real_wallet = [0.0] * (T + 1)

        # real_wallet starts at 0 (no pre-loaded balance)
        real_wallet[0] = 0.0

        for t in range(T):
            d = t // steps_per_day
            if t % steps_per_day == 0:
                real_wallet[t] += rp.daily_real_recharge[d]

            if real_wallet[t] < 0.0:
                # Wallet empty: no loads served
                for k in range(K):
                    actuation_state[k][t] = 0.0
            else:
                for k in range(K):
                    if demand_DS[t, k] == 0:
                        actuation_state[k][t] = 0.0
                    else:
                        actuation_state[k][t] = model_actuation_state[k][t]

            if t < T - 1:
                energy_cost = sum(
                    actuation_state[k][t] * self.df[self.cols[k]].iloc[t]
                    for k in range(K)
                ) * cost_factor
                real_wallet[t + 1] = real_wallet[t] - energy_cost

        disconnection_counter = 0
        for t in range(T):
            if real_wallet[t] < 0.0:
                if t == 0 or real_wallet[t - 1] >= 0.0:
                    disconnection_counter += 1

        return SimulationResults(
            model_actuation_state=model_actuation_state,
            actuation_state=actuation_state,
            real_wallet=real_wallet,
            disconnection_counter=disconnection_counter,
        )

    def compute_metrics(self, results: SimulationResults) -> SimulationResults:
        T = self.num_timesteps
        K = self.number_of_loads

        demand_P = self.df[list(self.cols)].to_numpy(dtype=float)
        demand_DS = self.df[[f"{c}_ds" for c in self.cols]].to_numpy(dtype=float)

        # Simulation metrics
        per_load_tsf: list[float] = []
        for k in range(K):
            demand_sum = float(np.sum(demand_DS[:, k])) + 1e-10
            act_sum = sum(results.actuation_state[k][t] for t in range(T))
            per_load_tsf.append(100.0 * act_sum / demand_sum)

        priority_sf = sum(
            self.load_pf[self.cols[k]] * per_load_tsf[k]
            for k in range(K)
        )

        num_E = sum(
            results.actuation_state[k][t] * demand_P[t, k]
            for k in range(K) for t in range(T)
        )
        den_E = float(np.sum(demand_DS * demand_P)) + 1e-10
        energy_sf = 100.0 * num_E / den_E

        # Model metrics (MILP output before wallet simulation)
        model_per_load_tsf: list[float] = []
        for k in range(K):
            demand_sum = float(np.sum(demand_DS[:, k])) + 1e-10
            act_sum = sum(results.model_actuation_state[k][t] for t in range(T))
            model_per_load_tsf.append(100.0 * act_sum / demand_sum)

        model_priority_sf = sum(
            self.load_pf[self.cols[k]] * model_per_load_tsf[k]
            for k in range(K)
        )

        num_mE = sum(
            results.model_actuation_state[k][t] * demand_P[t, k]
            for k in range(K) for t in range(T)
        )
        model_energy_sf = 100.0 * num_mE / den_E

        # Update results
        results.per_load_tsf = per_load_tsf
        results.priority_sf = priority_sf
        results.energy_sf = energy_sf
        results.model_per_load_tsf = model_per_load_tsf
        results.model_priority_sf = model_priority_sf
        results.model_energy_sf = model_energy_sf

        # Console summary
        print("\n--- OBM Simulation Metrics ---")
        for k in range(K):
            print(f"  Load {k + 1} ({self.cols[k]}) TSF : {per_load_tsf[k]:.2f}%")
        print(f"  Priority-Weighted TSF  : {priority_sf:.4f}%")
        print(f"  Energy Service Factor  : {energy_sf:.2f}%")
        print(f"  Disconnections         : {results.disconnection_counter}")
        print(f"  Solve time             : {results.solve_time:.2f} s")
        print("\n--- OBM Model Metrics (MILP schedule) ---")
        for k in range(K):
            print(f"  Load {k + 1} ({self.cols[k]}) TSF : {model_per_load_tsf[k]:.2f}%")
        print(f"  Priority-Weighted TSF  : {model_priority_sf:.4f}%")
        print(f"  Energy Service Factor  : {model_energy_sf:.2f}%")

        return results

    def main(self) -> SimulationResults:
        monthly_energy_kWh = self.monthly_energy_use()
        self.init_real_recharge_params(monthly_energy_kWh)

        print("Solving OBM MILP...")
        (model_actuation_state,
         model_priority_sf,
         model_energy_sf,
         model_per_load_tsf,
         solve_time) = self.compute_hem_schedule()

        results = self.simulate(model_actuation_state)
        results.solve_time = solve_time
        results.model_priority_sf = model_priority_sf
        results.model_energy_sf = model_energy_sf
        results.model_per_load_tsf = model_per_load_tsf

        results = self.compute_metrics(results)

        return results
    