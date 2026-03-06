import pandas as pd
import json
import re
import matplotlib.pyplot as plt

# CONFIGURATION
LOG_FILE = 'indoor_thirty_meters_1.log'
CSV_FILE = 'Indoors30meters1.csv'
TRIAL_NAME = "Indoor_30meters_trial_1_Analysis_Better"

def run_smoothed_analysis():
    # 1. Load and Parse Log
    log_data = []
    pattern = re.compile(r'\[(.*?)\] (.*?): (\{.*\})')
    with open(LOG_FILE, 'r', encoding='utf-8') as f:
        for line in f:
            m = pattern.search(line)
            if m:
                p = json.loads(m.group(3))
                clean = {k.rstrip(':'): v for k, v in p.items()}
                clean['ServerTime'] = pd.to_datetime(m.group(1))
                log_data.append(clean)
    
    df_log = pd.DataFrame(log_data)
    df_log.rename(columns={'Packet ID': 'Packet_ID', 'PM2.5': 'PM2_5', 'Temperature': 'Temp'}, inplace=True)
    df_log['Packet_ID'] = pd.to_numeric(df_log['Packet_ID'], errors='coerce')
    
    # 2. Filter 1-1000
    df_log = df_log[(df_log['Packet_ID'] >= 1) & (df_log['Packet_ID'] <= 1000)].drop_duplicates('Packet_ID').sort_values('Packet_ID')
    
    # 3. Calculate Throughput & Latency
    duration = (df_log['ServerTime'].max() - df_log['ServerTime'].min()).total_seconds()
    throughput = len(df_log) / duration if duration > 0 else 0
    
    # Latency Jitter (Zero-Floored)
    df_log['TS'] = pd.to_numeric(df_log['Timestamp'], errors='coerce')
    raw_lat = (df_log['ServerTime'] - df_log['ServerTime'].min()).dt.total_seconds() - (df_log['TS'] - df_log['TS'].min())/1000
    df_log['Jitter'] = raw_lat - raw_lat.min()

    # 4. PLOTTING
    fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(10, 15))

    # GRAPH 1: SMOOTHED SENSORS
    sensors = ['CO2', 'Temp', 'Humidity', 'PM2_5']
    for s in sensors:
        # 10-Packet Rolling Average for "Smoothing"
        smoothed_v = pd.to_numeric(df_log[s], errors='coerce').rolling(window=10, min_periods=1).mean()
        norm = (smoothed_v - smoothed_v.min()) / (smoothed_v.max() - smoothed_v.min())
        ax1.plot(df_log['Packet_ID'], norm, label=f"Smoothed {s}")
    
    ax1.set_title("Graph 1: Normalized Sensor Trends (10-Packet Rolling Average)")
    ax1.legend()

    # GRAPH 2: JITTER
    ax2.scatter(df_log['Packet_ID'], df_log['Jitter'], s=8, alpha=0.4, color='blue')
    ax2.set_title("Graph 2: Network Latency Variation (The 'Zero-Floor' Speed Test)")
    ax2.set_ylabel("Seconds of Delay")

    # GRAPH 3: THROUGHPUT
    ax3.step(df_log['Packet_ID'], df_log['Packet_ID'].diff(), color='darkred')
    ax3.set_title(f"Graph 3: Reliability (Throughput: {throughput:.3f} pkts/sec)")

    plt.tight_layout()
    plt.savefig(f'{TRIAL_NAME}_Smoothed_Report.png')
    print(f"Success! Report saved as {TRIAL_NAME}_Smoothed_Report.png")

run_smoothed_analysis()