from pathlib import Path
from milp.dfm import DFM
from milp.obm import OBM
import os

ALGORITHM = 'OBM'

# os.environ["GRB_LICENSE_FILE"] = r'/path/to/gurobi.lic'

# Define parameters
start_day = 1
load_order = [2, 4, 3, 1]
data_path = Path('/home/thocmurphy/Documents/School/2025-2026/Repositories/Honors-Thesis/prepaid-energy-manager/confidential/SmartGridComm-May2024/datasets/cleanData-357days.csv')
cols = ['air1', 'clotheswasher_dryg1', 'microwave1', 'refrigerator1']
recharge_percent = 70.0

# OBM simulation
if ALGORITHM == 'OBM':
    algo = OBM(
        start_day,
        load_order,
        data_path,
        cols=cols,
        recharge_percent=recharge_percent,
        solver='highs',
    )
# DFM simulation
elif ALGORITHM == 'DFM':
    algo = DFM(
        start_day,
        load_order,
        data_path,
        cols=cols,
        recharge_percent=recharge_percent,
        solver='gurobi',
    )
# No algorithm specified
else:
    raise ValueError('ALGORITHM not set to valid option.')

# Run algorithm
algo.main()
