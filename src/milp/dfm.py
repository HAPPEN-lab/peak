from __future__ import annotations
import math
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Literal, Optional
from .base import PEAK
import numpy as np
import pandas as pd
import pyomo.environ as pyo

fp = Path(__file__).resolve()

@dataclass
class SimulationResults:
    """All outputs produced by DFM.simulate() and DFM.compute_metrics()."""

    # Threshold schedule (per day, later broadcast to per-timestep)
    model_virtual_wallet_thresholds: list  # shape [K][D]

    # Per-timestep simulation trajectories
    actuation_state: list   # [K][T] - 1 if load k served at timestep t
    virtual_wallet: list   # [T+1]
    real_wallet: list   # [T+1]

    # Model (optimiser) internal trajectories
    model_virtual_wallet: list   # [T]
    model_real_wallet: list   # [T]
    model_actuation_state: list  # [K][T]

    # Metrics (filled by compute_metrics)
    priority_sf: float = 0.0
    energy_sf: float = 0.0
    per_load_tsf: list = field(default_factory=list)
    disconnection_counter: int = 0
    solve_time: float = 0.0

class DFM(PEAK):
    """
    Detailed Forecast MILP

    Inherits data loading, demand-state formatting, priority factors, and
    recharge-parameter helpers from PEAK.
    """

    def __init__(
        self,
        start_day: int,
        load_priority_order: list[int],
        json_path: Path,
        *,
        cols: Optional[list[str]] = None,
        horizon_time_days: int = 30,
        update_time_days: int = 30,
        number_of_loads: int = 4,
        number_of_days: int = 30,
        recharge_percent: float = 90.0,
        real_recharge_logic: int = 1,
        timestep_hours: float = 0.25,
        threshold_constant_days: int = 1,
        solver: str='highs',
    ) -> None:
        self.horizon_time_days = horizon_time_days
        self.update_time_days = update_time_days
        super().__init__(
            start_day, load_priority_order,
            json_path, cols=cols,
            number_of_loads=number_of_loads, number_of_days=number_of_days,
            recharge_percent=recharge_percent, real_recharge_logic=real_recharge_logic,
            timestep_hours=timestep_hours, threshold_constant_days=threshold_constant_days,
            solver=solver,
        )

    def compute_daily_virtual_update(
        self,
        recharge_cost: Literal["recharge", "cost"],
    ) -> list[float]:
        """
        Spread the lump-sum real recharge (or fixed-cost) schedule evenly
        across each inter-recharge interval so that the virtual wallet
        receives a smooth daily top-up.
        """
        schedule: list[list[float]] = (
            self.recharge_params.real_recharge_schedule
            if recharge_cost == "recharge"
            else self.recharge_params.real_cost_schedule
        )
        D = self.number_of_days
        virtual = [0.0] * D

        if not schedule:
            return virtual

        if len(schedule) == 1:
            first_day = schedule[0][0]
            amount = schedule[0][1]
            daily = amount / (D - first_day + 1)
            return [daily] * D

        # Intervals between consecutive recharge days
        for i in range(len(schedule) - 1):
            cur_day = int(schedule[i][0])
            next_day = int(schedule[i + 1][0])
            amount = schedule[i][1]
            daily = amount / (next_day - cur_day)
            for d in range(cur_day - 1, next_day - 1):
                virtual[d] = daily

        # Tail interval: last recharge to end of month
        last_day = int(schedule[-1][0])
        last_amount = schedule[-1][1]
        daily = last_amount / (D - last_day + 1)
        for d in range(last_day - 1, D):
            virtual[d] = daily

        return virtual

    def _init_timesteps(self) -> tuple[list[int], list[int]]:
        """
        Return (update_start_timesteps, day_start_timesteps) as 0-indexed
        positions within the full NUM_TIMESTEPS array.
        """
        steps = int(self.num_timesteps / self.number_of_days)   # 96
        n_upd = math.ceil(self.number_of_days / self.update_time_days)

        update_start_days = [0] * n_upd
        update_start_timesteps = [0] * n_upd
        day_start_timesteps = [0] * self.number_of_days

        for i in range(self.number_of_days):
            day_start_timesteps[i] = steps * i
            if self.update_time_days == 1:
                update_start_days[i] = i + 1
                update_start_timesteps[i] = day_start_timesteps[i]
            elif i % self.update_time_days == 0:
                idx = i // self.update_time_days
                update_start_days[idx] = i + 1
                update_start_timesteps[idx] = day_start_timesteps[i]

        return update_start_timesteps, day_start_timesteps

    def _compute_actuation_state(
        self,
        t: int,
        virtual_wallet: float,
        virtual_wallet_thresholds: list[float],
        real_wallet: float,
    ) -> tuple[list[float], list[float], list[float]]:
        """
        At timestep t decide which loads are served.

        Load k is actuated iff:
          • demand_state[k, t] = 1   (load is requesting power)
          • real_wallet > 0          (real money remains)
          • virtual_wallet >= threshold[k]  (virtual budget permits this load)
        """
        K = self.number_of_loads

        virtual_enable = [
            1.0 if virtual_wallet >= virtual_wallet_thresholds[k] else 0.0
            for k in range(K)
        ]
        real_enable = [1.0] * K if real_wallet > 0 else [0.0] * K

        actuation = [
            virtual_enable[k] * real_enable[k] * self.df[f"{self.cols[k]}_ds"].iloc[t]
            for k in range(K)
        ]
        return actuation, virtual_enable, real_enable

    def compute_thresholds(
        self,
        number_of_days: int,
        demand_data: dict,
        real_wallet_data: dict,
        virtual_wallet_data: dict,
    ) -> tuple:
        """
        Build and solve the DFM mixed-integer linear programme for the given
        horizon, returning optimal thresholds and model trajectories.
        """
        model = pyo.ConcreteModel()

        steps_per_day = int(round(24 / self.timestep_hours))   # 96
        number_of_timesteps = len(demand_data["demand_power_W"])
        update_timesteps = self.update_time_days * steps_per_day

        K_set = range(1, self.number_of_loads + 1)
        T_set = range(1, number_of_timesteps + 1)
        Tp1_set = range(1, number_of_timesteps + 2)
        D_set = range(1, number_of_days + 1)

        demand_P = demand_data["demand_power_W"].to_numpy(dtype=float)   # [T, K]
        demand_DS = demand_data["demand_state"].to_numpy(dtype=float)      # [T, K]

        t_to_day = [0] + [(t - 1) // steps_per_day + 1 for t in range(1, number_of_timesteps + 2)]

        _rr = real_wallet_data["daily_real_recharge"]
        _rc = real_wallet_data["daily_real_cost"]
        _vr = virtual_wallet_data["daily_virtual_recharge"]
        _vc = virtual_wallet_data["daily_virtual_cost"]

        milp_day_starts = {1 + i * steps_per_day for i in range(number_of_days)}

        recharge_arr = [0.0] * (number_of_timesteps + 2)
        cost_arr = [0.0] * (number_of_timesteps + 2)
        vrecharge_arr = [0.0] * (number_of_timesteps + 2)
        vcost_arr = [0.0] * (number_of_timesteps + 2)
        for t in range(1, number_of_timesteps + 2):
            if t in milp_day_starts:
                idx = (t - 1) // steps_per_day
                recharge_arr[t] = _rr[idx] if idx < len(_rr) else 0.0
                cost_arr[t] = _rc[idx] if idx < len(_rc) else 0.0
                vrecharge_arr[t] = _vr[idx] if idx < len(_vr) else 0.0
                vcost_arr[t] = _vc[idx] if idx < len(_vc) else 0.0

        cost_factor = self.timestep_hours * real_wallet_data["cost_perkWh"] / 1000.0

        energy_coeff = [[0.0] * self.number_of_loads] + [
            [cost_factor * demand_P[t - 1, k] for k in range(self.number_of_loads)]
            for t in range(1, number_of_timesteps + 1)
        ]

        ds = [[0.0] * self.number_of_loads] + [
            [demand_DS[t - 1, k] for k in range(self.number_of_loads)]
            for t in range(1, number_of_timesteps + 1)
        ]

        # Sets
        model.K = pyo.Set(initialize=K_set)
        model.T = pyo.Set(initialize=T_set)
        model.Tp1 = pyo.Set(initialize=Tp1_set)
        model.D = pyo.Set(initialize=D_set)

        # Variables
        model.s = pyo.Var(model.K, model.T, domain=pyo.Binary)
        model.rw = pyo.Var(model.Tp1, domain=pyo.Reals)
        model.re = pyo.Var(model.K, model.Tp1, domain=pyo.Binary)
        model.ve = pyo.Var(model.K, model.T, domain=pyo.Binary)
        model.vw = pyo.Var(model.T, domain=pyo.Reals)
        model.thr = pyo.Var(model.K, model.D, domain=pyo.Reals, bounds=(-500.0, 500.0))

        M_BIG = 1_000.0
        M_SMALL = -1_000.0
        EPS = 1e-6

        # Real wallet dynamics
        def rw_rule(mdl, t):
            if t == 1:
                # Pre-recharge balance; MILP adds day-1 recharge at t=1
                return mdl.rw[1] == (
                    real_wallet_data["real_wallet_balance"]
                    + recharge_arr[1] - cost_arr[1]
                )
            ec = pyo.quicksum(
                energy_coeff[t - 1][k - 1] * mdl.s[k, t - 1] for k in K_set
            )
            return (
                mdl.rw[t] == mdl.rw[t - 1] - ec
                              + recharge_arr[t] - cost_arr[t]
            )
        model.c_rw = pyo.Constraint(model.Tp1, rule=rw_rule)

        def re_lo(mdl, k, t):
            return M_SMALL * mdl.re[k, t] <= -mdl.rw[t]
        def re_hi(mdl, k, t):
            return (M_BIG + EPS) * (1 - mdl.re[k, t]) >= EPS - mdl.rw[t]
        model.c_re_lo = pyo.Constraint(model.K, model.Tp1, rule=re_lo)
        model.c_re_hi = pyo.Constraint(model.K, model.Tp1, rule=re_hi)

        # Virtual wallet dynamics
        def vw_rule(mdl, t):
            if t == 1:
                # Pre-recharge balance; MILP adds day-1 virtual recharge at t=1
                return mdl.vw[1] == (
                    virtual_wallet_data["virtual_wallet_balance"]
                    + vrecharge_arr[1] - vcost_arr[1]
                )
            ec = pyo.quicksum(
                energy_coeff[t - 1][k - 1] * mdl.s[k, t - 1] for k in K_set
            )
            return (
                mdl.vw[t] == mdl.vw[t - 1] - ec
                              + vrecharge_arr[t] - vcost_arr[t]
            )
        model.c_vw = pyo.Constraint(model.T, rule=vw_rule)

        def ve_hi(mdl, k, t):
            d = t_to_day[t]
            return (
                mdl.vw[t] - mdl.thr[k, d]
                - (M_BIG + EPS) * mdl.ve[k, t] <= -EPS
            )
        def ve_lo(mdl, k, t):
            d = t_to_day[t]
            return (
                mdl.vw[t] - mdl.thr[k, d]
                - M_SMALL * (1 - mdl.ve[k, t]) >= 0
            )
        model.c_ve_hi = pyo.Constraint(model.K, model.T, rule=ve_hi)
        model.c_ve_lo = pyo.Constraint(model.K, model.T, rule=ve_lo)

        # Actuation-state constraints
        def act_v(mdl, k, t):
            return mdl.s[k, t] <= ds[t][k - 1] * mdl.ve[k, t]
        def act_rn(mdl, k, t):
            return mdl.s[k, t] <= mdl.re[k, t]
        def act_rnx(mdl, k, t):
            return mdl.s[k, t] <= mdl.re[k, t + 1]
        def act_lg(mdl, k, t):
            return (
                ds[t][k - 1] * mdl.ve[k, t]
                + mdl.re[k, t] + mdl.re[k, t + 1]
                <= 2 + mdl.s[k, t]
            )
        model.c_av = pyo.Constraint(model.K, model.T, rule=act_v)
        model.c_arn = pyo.Constraint(model.K, model.T, rule=act_rn)
        model.c_ax = pyo.Constraint(model.K, model.T, rule=act_rnx)
        model.c_alg = pyo.Constraint(model.K, model.T, rule=act_lg)

        # Threshold-constant constraints (when THRESHOLD_CONSTANT_DAYS > 1)
        if self.threshold_constant_days > 1:
            pairs: list[tuple[int, int]] = []
            for k in K_set:
                j = 1
                while j <= number_of_days:
                    for i in range(j, j + self.threshold_constant_days - 1):
                        if i < number_of_days:
                            pairs.append((k, i))
                    j += self.threshold_constant_days
            model.thr_pairs = pyo.Set(initialize=pairs, dimen=2)
            def thr_const(mdl, k, i):
                return mdl.thr[k, i] == mdl.thr[k, i + 1]
            model.c_thr = pyo.Constraint(model.thr_pairs, rule=thr_const)

        # Objective function
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

        # Solve (Gurobi MILP)
        solver = pyo.SolverFactory(self.solver)
        solver.options["MIPGap"] = 0.0001   # 0.01%

        t0 = time.perf_counter()
        results = solver.solve(model, tee=True)
        solve_time = time.perf_counter() - t0

        # Extract results
        Nu = min(update_timesteps, number_of_timesteps)

        def _v(var):
            val = pyo.value(var)
            return val if val is not None else 0.0

        # Per-timestep thresholds for the update window
        vwt_uw = [
            [
                _v(model.thr[k, t_to_day[t]])
                if t <= number_of_timesteps else 0.0
                for t in range(1, update_timesteps + 1)
            ]
            for k in K_set
        ]

        mrw = [_v(model.rw[t]) if t <= number_of_timesteps else 0.0
               for t in range(1, Nu + 1)]
        mvw = [_v(model.vw[t]) if t <= number_of_timesteps else 0.0
               for t in range(1, Nu + 1)]

        mve = [[_v(model.ve[k, t]) if t <= number_of_timesteps else 0.0
                for t in range(1, Nu + 1)] for k in K_set]
        mre = [[_v(model.re[k, t]) if t <= number_of_timesteps else 0.0
                for t in range(1, Nu + 1)] for k in K_set]
        mas = [[_v(model.s[k, t])  if t <= number_of_timesteps else 0.0
                for t in range(1, Nu + 1)] for k in K_set]
        mds = [[ds[t][k - 1]       if t <= number_of_timesteps else 0.0
                for t in range(1, Nu + 1)] for k in K_set]

        # Per-day thresholds
        mvwt = [
            [_v(model.thr[k, d]) for d in D_set]
            for k in K_set
        ]

        return vwt_uw, mrw, mvw, mve, mre, mas, mds, mvwt, solve_time

    def simulate(
        self,
        daily_virtual_recharge: list[float],
        daily_virtual_cost: list[float],
    ) -> SimulationResults:
        """
        Execute the online threshold-based simulation for the full
        NUMBER_OF_DAYS horizon.

        At each update start the MILP is solved to obtain optimal thresholds.
        At each timestep the actuation state is determined by those thresholds
        and the current wallet balances.
        """
        update_start_timesteps, day_start_timesteps = self._init_timesteps()
        T = self.num_timesteps
        K = self.number_of_loads

        # Simulation-state arrays
        actuation_state = [[0.0] * T for _ in range(K)]
        real_wallet = [0.0] * (T + 1)
        real_enable = [[0.0] * T for _ in range(K)]
        virtual_wallet = [0.0] * (T + 1)
        virtual_enable = [[0.0] * T for _ in range(K)]
        virtual_wallet_thresholds = [[0.0] * T for _ in range(K)]

        # Storage for model (optimiser) predictions
        model_real_wallet = [0.0] * T
        model_virtual_wallet = [0.0] * T
        model_real_enable = [[0.0] * T for _ in range(K)]
        model_virtual_enable = [[0.0] * T for _ in range(K)]
        model_actuation_state = [[0.0] * T for _ in range(K)]
        model_demand_state = [[0.0] * T for _ in range(K)]
        model_virtual_wallet_thresholds = [[0.0] * self.number_of_days for _ in range(K)]

        disconnection_counter = 0
        solve_time_total = 0.0
        rp = self.recharge_params

        steps_per_day = int(24 / self.timestep_hours)   # 96

        # Initialise wallet balances before the loop
        # before the first timestep is evaluated)
        real_wallet[0] = rp.real_wallet_balance
        virtual_wallet[0] = rp.virtual_wallet_balance

        # Main loop  t = 0 … T-2  (last timestep handled separately)
        for t in range(T - 1):

            # Solve optimization at update-start timesteps
            # Must happen BEFORE recharge: MILP receives the pre-recharge balance and internally adds day-1 recharge.
            if t in update_start_timesteps:
                Nw = self.horizon_time_days * steps_per_day
                Nw_days = self.horizon_time_days
                if t + Nw > T:
                    Nw = T - t
                    Nw_days = int(math.floor(Nw / steps_per_day))

                demand_data = {
                    "demand_power_W": (self.df[list(self.cols)].iloc[t: t + Nw].reset_index(drop=True)),
                    "demand_state": (self.df[[f"{c}_ds" for c in self.cols]].iloc[t: t + Nw].reset_index(drop=True)),
                }

                day_idx = t // steps_per_day   # 0-indexed current day

                real_wallet_data = {
                    "real_wallet_balance":  real_wallet[t],
                    "daily_real_recharge":  rp.daily_real_recharge[day_idx: day_idx + Nw_days],
                    "daily_real_cost":      rp.daily_real_cost[day_idx: day_idx + Nw_days],
                    "cost_perkWh":          rp.cost_per_kWh,
                }
                virtual_wallet_data = {
                    "virtual_wallet_balance": virtual_wallet[t],
                    "daily_virtual_recharge": daily_virtual_recharge[day_idx: day_idx + Nw_days],
                    "daily_virtual_cost":     daily_virtual_cost[day_idx: day_idx + Nw_days],
                    "threshold_constant_days": self.threshold_constant_days,
                }

                (vwt_uw, mrw, mvw, mve, mre,
                 mas, mds, mvwt, stm) = self.compute_thresholds(
                    Nw_days, demand_data, real_wallet_data, virtual_wallet_data
                )
                solve_time_total += stm

                # Write model predictions into arrays
                Nu = len(mrw)
                for i in range(Nu):
                    if t + i < T:
                        model_real_wallet[t + i] = mrw[i]
                        model_virtual_wallet[t + i] = mvw[i]
                        for k in range(K):
                            virtual_wallet_thresholds[k][t + i] = vwt_uw[k][i]
                            model_virtual_enable[k][t + i] = mve[k][i]
                            model_real_enable[k][t + i] = mre[k][i]
                            model_actuation_state[k][t + i] = mas[k][i]
                            model_demand_state[k][t + i] = mds[k][i]

                # Store per-day thresholds
                for d_off in range(len(mvwt[0])):
                    d_idx = day_idx + d_off
                    if d_idx < self.number_of_days:
                        for k in range(K):
                            model_virtual_wallet_thresholds[k][d_idx] = mvwt[k][d_off]

            # Daily recharge injection - happens AFTER MILP solve but BEFORE actuation so the correct balance is used for actuation
            if t in day_start_timesteps:
                d = t // steps_per_day
                real_wallet[t]    += rp.daily_real_recharge[d] - rp.daily_real_cost[d]
                virtual_wallet[t] += daily_virtual_recharge[d] - daily_virtual_cost[d]

            # Compute actuation state
            act, v_en, r_en = self._compute_actuation_state(
                t,
                virtual_wallet[t],
                [virtual_wallet_thresholds[k][t] for k in range(K)],
                real_wallet[t],
            )
            for k in range(K):
                actuation_state[k][t] = act[k]
                virtual_enable[k][t] = v_en[k]
                real_enable[k][t] = r_en[k]

            # Advance wallet balances
            energy_cost = sum(actuation_state[k][t]
                * self.df[self.cols[k]].iloc[t]
                * self.timestep_hours
                * (rp.cost_per_kWh / 1000.0)
                for k in range(K)
            )
            virtual_wallet[t + 1] = virtual_wallet[t] - energy_cost
            real_wallet[t + 1] = real_wallet[t] - energy_cost

            # Prevent disconnection: retroactively zero actuation
            if real_wallet[t + 1] < 0:
                for k in range(K):
                    actuation_state[k][t] = 0.0
                virtual_wallet[t + 1] = virtual_wallet[t]
                real_wallet[t + 1] = real_wallet[t]

        # Handle last timestep (no wallet advance)
        t_last = T - 1
        if t_last in day_start_timesteps:
            d = t_last // steps_per_day
            real_wallet[t_last]    += rp.daily_real_recharge[d] - rp.daily_real_cost[d]
            virtual_wallet[t_last] += daily_virtual_recharge[d] - daily_virtual_cost[d]

        act, v_en, r_en = self._compute_actuation_state(
            t_last,
            virtual_wallet[t_last],
            [virtual_wallet_thresholds[k][t_last] for k in range(K)],
            real_wallet[t_last],
        )
        for k in range(K):
            actuation_state[k][t_last] = act[k]
            virtual_enable[k][t_last] = v_en[k]
            real_enable[k][t_last] = r_en[k]

        # Count disconnection events
        for t in range(T):
            if real_wallet[t] < 0 and (t == 0 or real_wallet[t - 1] >= 0):
                disconnection_counter += 1

        return SimulationResults(
            model_virtual_wallet_thresholds=model_virtual_wallet_thresholds,
            actuation_state=actuation_state,
            virtual_wallet=virtual_wallet,
            real_wallet=real_wallet,
            model_virtual_wallet=model_virtual_wallet,
            model_real_wallet=model_real_wallet,
            model_actuation_state=model_actuation_state,
            disconnection_counter=disconnection_counter,
            solve_time=solve_time_total,
        )

    def compute_metrics(self, results: SimulationResults) -> SimulationResults:
        """
        Compute TSF, energy SF, and priority-weighted SF from simulation
        results.  Results object is updated in-place and returned.
        """
        T = self.num_timesteps
        K = self.number_of_loads

        # Per-load time service factor
        per_load_tsf: list[float] = []
        for k in range(K):
            col = self.cols[k]
            demand_sum = sum(self.df[f"{col}_ds"].iloc[t] for t in range(T))
            act_sum = sum(results.actuation_state[k][t] for t in range(T))
            per_load_tsf.append(100.0 * act_sum / (demand_sum + 1e-10))

        # Priority-weighted TSF
        priority_sf = sum(
            self.load_pf[self.cols[k]] * per_load_tsf[k]
            for k in range(K)
        )

        # Energy service factor
        num_energy = sum(
            results.actuation_state[k][t] * self.df[self.cols[k]].iloc[t]
            for k in range(K)
            for t in range(T)
        )
        den_energy = sum(
            self.df[f"{self.cols[k]}_ds"].iloc[t] * self.df[self.cols[k]].iloc[t]
            for k in range(K)
            for t in range(T)
        )
        energy_sf = 100.0 * num_energy / (den_energy + 1e-10)

        # Update results
        results.per_load_tsf = per_load_tsf
        results.priority_sf = priority_sf
        results.energy_sf = energy_sf

        # Console output
        print("\n--- DFM Simulation Metrics ---")
        for k in range(K):
            print(f"  Load {k + 1} ({self.cols[k]}) TSF : {per_load_tsf[k]:.2f}%")
        print(f"  Priority-Weighted TSF  : {priority_sf:.4f}%")
        print(f"  Energy Service Factor  : {energy_sf:.2f}%")
        print(f"  Disconnections         : {results.disconnection_counter}")
        print(f"  Solve time (total)     : {results.solve_time:.2f} s")

        return results

    def main(self) -> SimulationResults:
        """
        Full DFM pipeline:
          1. Compute monthly energy use and initialise recharge parameters
          2. Spread recharge/cost schedules into daily virtual arrays
          3. Run the simulation (solves MILP once, then applies thresholds)
          4. Compute and print metrics
          5. Write outputs to testing/jl_testing/
        """
        # Step 1 - energy / recharge setup
        monthly_energy_kWh = self.monthly_energy_use()
        self.init_real_recharge_params(monthly_energy_kWh)

        # Step 2 - virtual recharge arrays
        daily_virtual_recharge = self.compute_daily_virtual_update("recharge")
        daily_virtual_cost = self.compute_daily_virtual_update("cost")

        # Step 3 - simulate
        results = self.simulate(daily_virtual_recharge, daily_virtual_cost)

        # Step 4 - metrics
        results = self.compute_metrics(results)

        return results

    def main_direct(self) -> SimulationResults:
        """
        Direct PSF computation from MILP solution (NO online simulation):
          1. Compute monthly energy use and initialise recharge parameters
          2. Spread recharge/cost schedules into daily virtual arrays
          3. Solve MILP once with full month data to get optimal thresholds
          4. Compute PSF directly from model actuation states
          5. Write outputs to testing/jl_testing/
        """
        T = self.num_timesteps
        K = self.number_of_loads
        steps_per_day = int(24 / self.timestep_hours)  # 96

        # Step 1 - energy / recharge setup
        monthly_energy_kWh = self.monthly_energy_use()
        self.init_real_recharge_params(monthly_energy_kWh)

        # Step 2 - virtual recharge arrays
        daily_virtual_recharge = self.compute_daily_virtual_update("recharge")
        daily_virtual_cost = self.compute_daily_virtual_update("cost")

        # Step 3 - solve MILP once with full month data
        print("\n=== Computing thresholds (MILP) ===")
        
        demand_data = {
            "demand_power_W": self.df[list(self.cols)].reset_index(drop=True),
            "demand_state": self.df[[f"{c}_ds" for c in self.cols]].reset_index(drop=True),
        }

        real_wallet_data = {
            "real_wallet_balance": self.recharge_params.real_wallet_balance,
            "daily_real_recharge": self.recharge_params.daily_real_recharge,
            "daily_real_cost": self.recharge_params.daily_real_cost,
            "cost_perkWh": self.recharge_params.cost_per_kWh,
        }

        virtual_wallet_data = {
            "virtual_wallet_balance": self.recharge_params.virtual_wallet_balance,
            "daily_virtual_recharge": daily_virtual_recharge,
            "daily_virtual_cost": daily_virtual_cost,
            "threshold_constant_days": self.threshold_constant_days,
        }

        (vwt_uw, mrw, mvw, mve, mre, mas, mds, mvwt, 
         solve_time) = self.compute_thresholds(
            self.number_of_days, demand_data, real_wallet_data, virtual_wallet_data
        )

        # Step 4 - compute PSF directly from model actuation states
        print("\n=== Computing metrics from MILP solution ===")
        
        # Extract full actuation state (pad if needed)
        model_actuation_state = [
            mas[k] + [0.0] * (T - len(mas[k])) for k in range(K)
        ]
        
        # Per-load time service factor
        per_load_tsf: list[float] = []
        for k in range(K):
            col = self.cols[k]
            demand_sum = sum(self.df[f"{col}_ds"].iloc[t] for t in range(T))
            act_sum = sum(model_actuation_state[k][t] for t in range(T))
            per_load_tsf.append(100.0 * act_sum / (demand_sum + 1e-10))

        # Priority-weighted TSF
        priority_sf = sum(
            self.load_pf[self.cols[k]] * per_load_tsf[k]
            for k in range(K)
        )

        # Energy service factor
        num_energy = sum(
            model_actuation_state[k][t] * self.df[self.cols[k]].iloc[t]
            for k in range(K)
            for t in range(T)
        )
        den_energy = sum(
            self.df[f"{self.cols[k]}_ds"].iloc[t] * self.df[self.cols[k]].iloc[t]
            for k in range(K)
            for t in range(T)
        )
        energy_sf = 100.0 * num_energy / (den_energy + 1e-10)

        # Create results object
        results = SimulationResults(
            model_virtual_wallet_thresholds=mvwt,
            actuation_state=model_actuation_state,
            virtual_wallet=mvw + [0.0] * (T + 1 - len(mvw)),
            real_wallet=mrw + [0.0] * (T + 1 - len(mrw)),
            model_virtual_wallet=mvw,
            model_real_wallet=mrw,
            model_actuation_state=model_actuation_state,
            per_load_tsf=per_load_tsf,
            priority_sf=priority_sf,
            energy_sf=energy_sf,
            disconnection_counter=0,
            solve_time=solve_time,
        )

        # Console output
        print("\n--- DFM Direct PSF Metrics (MILP solution, no simulation) ---")
        for k in range(K):
            print(f"  Load {k + 1} ({self.cols[k]}) TSF : {per_load_tsf[k]:.2f}%")
        print(f"  Priority-Weighted TSF  : {priority_sf:.4f}%")
        print(f"  Energy Service Factor  : {energy_sf:.2f}%")
        print(f"  Solve time             : {solve_time:.2f} s")

        return results
    