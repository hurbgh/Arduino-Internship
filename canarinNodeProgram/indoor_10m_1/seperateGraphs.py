import pandas as pd
import json
import re
import matplotlib.pyplot as plt

def generate_report(log_file, csv_file):
    # 1. Load Data
    log_rows = []
    pattern = re.compile(r'\[(.*?)\] (.*?): (\{.*\})')
    with open(log_file, 'r', encoding='utf-8') as f:
        for line in f:
            m = pattern.search(line)
            if m:
                p = json.loads(m.group(3))
                clean = {k.rstrip(':'): v for k, v in p.items()}
                log_rows.append(clean)
    
    df = pd.DataFrame(log_rows)
    df.rename(columns={'Packet ID': 'Packet_ID', 'PM2.5': 'PM2_5', 'Temperature': 'Temp'}, inplace=True)
    
    # 2. Convert to Numeric & Filter
    for col in ['Packet_ID', 'Temp', 'Humidity', 'CO2', 'CO', 'PM2_5']:
        df[col] = pd.to_numeric(df[col], errors='coerce')
    
    df = df[(df['Packet_ID'] >= 1) & (df['Packet_ID'] <= 1000)].drop_duplicates('Packet_ID').sort_values('Packet_ID')

    # 3. Create 5 Separate Graphs
    sensors = ['Temp', 'Humidity', 'CO2', 'CO', 'PM2_5']
    labels = ['Temperature ($^\circ$C)', 'Humidity (%)', 'CO2 (ppm)', 'CO (ppm)', 'PM2.5 ($\mu$g/m$^3$)']
    
    fig, axes = plt.subplots(5, 1, figsize=(10, 25))
    
    for i, s in enumerate(sensors):
        # Apply Smoothing (Rolling Average)
        smoothed = df[s].rolling(window=10, min_periods=1).mean()
        
        # Plot Raw (Gray) and Smoothed (Blue)
        axes[i].plot(df['Packet_ID'], df[s], alpha=0.3, color='gray', label='Raw Data')
        axes[i].plot(df['Packet_ID'], smoothed, color='blue', linewidth=2, label='Smoothed Trend')
        
        axes[i].set_title(f"Sensor Analysis: {labels[i]}", fontsize=14, fontweight='bold')
        axes[i].set_ylabel(labels[i])
        axes[i].grid(True, alpha=0.3)
        axes[i].legend()

    plt.tight_layout()
    plt.savefig('SmartFire_Separate_Sensor_Graphs.png')
    print("Graph generated: SmartFire_Separate_Sensor_Graphs.png")

# Run it
generate_report('indoor_ten_meters_1.log', 'Indoors10meters1.csv')