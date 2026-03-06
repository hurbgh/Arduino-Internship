import pandas as pd
import json
import re
import matplotlib.pyplot as plt

# ========================================================
# CONFIGURATION AREA: CHANGE THESE THREE LINES
# ========================================================
LOG_FILE_NAME = 'indoor_fifty_meters_2.log'  # <--- Your .log (JSON) file
CSV_FILE_NAME = 'Indoor50meter2.csv' # <--- Your .csv (Phone) file
TRIAL_LABEL = 'Indoor_50meters_trial_2'  # <--- Name for your graph
# ========================================================

def run_analysis():
    # 1. Parse the Log (JSON)
    log_rows = []
    pattern = re.compile(r'\[(.*?)\] (.*?): (\{.*\})')
    with open(LOG_FILE_NAME, 'r', encoding='utf-8') as f:
        for line in f:
            m = pattern.search(line)
            if m:
                try:
                    p = json.loads(m.group(3))
                    clean = {k.rstrip(':'): v for k, v in p.items()}
                    clean['ServerTime'] = pd.to_datetime(m.group(1))
                    log_rows.append(clean)
                except: continue
    df_log = pd.DataFrame(log_rows)
    df_log.rename(columns={'Packet ID': 'Packet_ID', 'PM2.5': 'PM2_5', 'Temperature': 'Temp'}, inplace=True)
    
    # 2. Parse the CSV (Phone)
    df_csv = pd.read_csv(CSV_FILE_NAME, names=["Timestamp", "Packet_ID", "PM1_0", "PM2_5", "PM10", "CO2", "CO", "Temp", "Humidity", "Pressure", "Altitude"])

    # 3. Filter for Packets 1 to 1000 (Ignoring Packet 0)
    df_log['Packet_ID'] = pd.to_numeric(df_log['Packet_ID'], errors='coerce')
    df_csv['Packet_ID'] = pd.to_numeric(df_csv['Packet_ID'], errors='coerce')
    df_log = df_log[(df_log['Packet_ID'] >= 1) & (df_log['Packet_ID'] <= 1000)].drop_duplicates('Packet_ID').sort_values('Packet_ID')
    df_csv = df_csv[(df_csv['Packet_ID'] >= 1) & (df_csv['Packet_ID'] <= 1000)].drop_duplicates('Packet_ID').sort_values('Packet_ID')

    # 4. Calculate Throughput
    duration = (df_log['ServerTime'].max() - df_log['ServerTime'].min()).total_seconds()
    throughput = len(df_log) / duration if duration > 0 else 0

    # 5. Calculate Accuracy (Matching Packet ID x in CSV to Packet ID x in Log)
    merged = pd.merge(df_csv, df_log, on='Packet_ID', suffixes=('_Phone', '_Server'))
    acc_pct = 0
    if not merged.empty:
        # Check if CO2 matches bit-for-bit
        matches = (merged['CO2_Phone'].astype(float) == merged['CO2_Server'].astype(float)).sum()
        acc_pct = (matches / len(merged)) * 100

    # 6. Generate Plot
    fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(10, 15))

    # GRAPH 1: All Sensors
    for s in ['Temp', 'CO2', 'PM2_5', 'Humidity']:
        v = pd.to_numeric(df_log[s], errors='coerce').dropna()
        if not v.empty:
            norm = (v - v.min()) / (v.max() - v.min())
            ax1.plot(df_log['Packet_ID'], norm, label=s)
    ax1.set_title(f"Graph 1: Accuracy & Sensor Integrity\n(Matches: {acc_pct:.1f}%)")
    ax1.legend()

    # GRAPH 2: Latency (Relative Travel Time)
    df_log['Latency'] = (df_log['ServerTime'] - df_log['ServerTime'].min()).dt.total_seconds() - \
                        (pd.to_numeric(df_log['Timestamp']) - pd.to_numeric(df_log['Timestamp']).min())/1000
    ax2.scatter(df_log['Packet_ID'], df_log['Latency'], s=5, alpha=0.5, color='blue')
    ax2.set_title("Graph 2: Latency Jitter (Y-Axis: Trip Delay in Seconds)")
    ax2.set_ylabel("Delay (s)")

    # GRAPH 3: Packet Loss & Throughput
    ax3.step(df_log['Packet_ID'], df_log['Packet_ID'].diff(), color='red')
    ax3.set_title(f"Graph 3: Reliability (Received: {len(df_log)}/1000)")
    ax3.set_ylabel("ID Gap (1.0 = Perfect)")
    ax3.annotate(f"THROUGHPUT: {throughput:.3f} packets/sec", xy=(0.02, 0.85), xycoords='axes fraction', 
                 bbox=dict(boxstyle="round", fc="cyan", alpha=0.5), fontsize=12)

    plt.tight_layout()
    image_name = f'{TRIAL_LABEL}_Analysis.png'
    plt.savefig(image_name)
    print(f"Analysis Complete! Graph saved as: {image_name}")
    print(f"Throughput: {throughput:.4f} pkts/s")
    print(f"Data Match Accuracy: {acc_pct:.2f}%")

if __name__ == "__main__":
    run_analysis()