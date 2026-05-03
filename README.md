# PEAK: Prepaid Energy Assistance Kit

## Installation

1. Clone the repository:
```bash
git clone https://github.com/HAPPEN-lab/peak.git
cd peak
```

2. Create and activate a virtual environment:
```bash
python -m venv .venv

# Windows
.\.venv\Scripts\activate

# Linux
source .venv/bin/activate
```

3. Install dependencies:
```bash
pip install -r requirements.txt
```

## Usage

### Python MILP Models

Navigate to the `src` directory containing the project's code. Inside the main python file, `milp_main.py`, choose an optimization algorithm to run. This can be done by setting the constant `ALGORITHM` to either 'OBM' or 'DFM' at the top of the file. Along with choosing an algorithm, users must input the start day, load_order, data path (for the forecast), column names for loads, and the recharge percentage. Users must additionally add the Gurobi license file if they are running the DFM algorithm. This can be done by uncommenting the following line at the beginning of the script:

```python
# os.environ["GRB_LICENSE_FILE"] = r'/path/to/gurobi.lic'
```

Once an algorithm is selected and ran, outputs from the model and simulation will be output in the terminal, but results can additionally be accessed by storing the output of the algorithm's `main` function.

### AFG Fractional-Knapsack Model

AFG is designed to be run on an ESP32-S3 with data stored on a connected SD card. It is recommended to compile and run AFG using the `platformio.ini` file inside this repository. Compilation and flashing to the ESP32 can be done using the following command:

```bash
platformio run -e afg -t upload
```

Additionally, the code can be monitored over the serial port by adding the `-t monitor` flag. *Note: The platformio `.ini` file contains upload/monitor port options for Linux, MacOS, and Windows. Make sure to choose the port option for your operating system. Also, ensure you are not communicating with another device over serial, or change the port selection to upload/monitor a specific port.*
