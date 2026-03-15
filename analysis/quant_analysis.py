import os
import glob
import struct
import numpy as np
import pandas as pd
import plotly.express as px
import plotly.graph_objects as go
from scipy.stats import pearsonr, spearmanr

# 1. Parsing the binary quant_features.bin
def load_quant_features(bin_path):
    print(f"Loading {bin_path}...")
    record_struct = struct.Struct('<QdddddQ')
    record_size = record_struct.size
    
    records = []
    with open(bin_path, 'rb') as f:
        while True:
            chunk = f.read(record_size)
            if len(chunk) < record_size:
                break
            records.append(record_struct.unpack(chunk))
            
    df = pd.DataFrame(records, columns=[
        'timestamp_ms', 'mid_price', 'spread', 'imbalance', 
        'depth_slope', 'realized_vol', 'system_latency_us'
    ])
    # Convert timestamp to datetime for easier plotting
    df['datetime'] = pd.to_datetime(df['timestamp_ms'], unit='ms')
    return df

def load_trades(csv_path):
    # Time,ExecutionPrice_USD,IntendedPrice_USD,Quantity_BTC,Side,Slippage_BPS
    if not os.path.exists(csv_path):
        return pd.DataFrame()
    df = pd.read_csv(csv_path)
    # Parse Time (HH:MM:SS) if possible, but actually we need timestamp or we can merge on index/time
    return df

def analyze_spread_relationships(df, output_dir):
    print("Analyzing spread relationships...")
    
    # Pre-filter extreme outliers for plotting
    plot_df = df[
        (df['spread'] < df['spread'].quantile(0.99)) & 
        (df['realized_vol'] < df['realized_vol'].quantile(0.99))
    ].copy()

    # 1. Spread vs Realized Vol
    fig1 = px.density_heatmap(
        plot_df, x='realized_vol', y='spread', 
        title='Spread vs Realized Volatility',
        labels={'realized_vol': 'Rolling Volatility (100 ticks)', 'spread': 'Spread (USD)'},
        color_continuous_scale='Viridis'
    )
    fig1.write_html(os.path.join(output_dir, 'spread_vs_vol.html'))
    fig1.write_image(os.path.join(output_dir, 'spread_vs_vol.png'))

    # 2. Spread vs Imbalance
    fig2 = px.density_heatmap(
        plot_df, x='imbalance', y='spread', 
        title='Spread vs Order Flow Imbalance',
        labels={'imbalance': 'Normalized Imbalance [-1, 1]', 'spread': 'Spread (USD)'},
        color_continuous_scale='Plasma'
    )
    fig2.write_html(os.path.join(output_dir, 'spread_vs_imbalance.html'))
    fig2.write_image(os.path.join(output_dir, 'spread_vs_imbalance.png'))

    # 3. Spread vs Depth Slope
    plot_df_slope = plot_df[plot_df['depth_slope'] < plot_df['depth_slope'].quantile(0.95)]
    fig3 = px.scatter(
        plot_df_slope.sample(min(10000, len(plot_df_slope))), 
        x='depth_slope', y='spread', 
        trendline='ols',
        title='Spread vs Liquidity Depth Slope',
        labels={'depth_slope': 'Depth Slope (Vol/Tick)', 'spread': 'Spread (USD)'},
        opacity=0.3
    )
    fig3.write_html(os.path.join(output_dir, 'spread_vs_depth_slope.html'))
    fig3.write_image(os.path.join(output_dir, 'spread_vs_depth_slope.png'))

def analyze_imbalance_predictive_power(df, horizons_ms, output_dir):
    print("Analyzing Imbalance Predictive Power...")
    df = df.sort_values('timestamp_ms').reset_index(drop=True)
    
    results = []
    
    for h in horizons_ms:
        # Create a shifted dataframe to find future mid price
        # Using merge_asof to find the exact price at t + h
        target_times = df[['timestamp_ms']].copy()
        target_times['target_ts'] = target_times['timestamp_ms'] + h
        
        # Merge to find the closest future row
        merged = pd.merge_asof(
            target_times.sort_values('target_ts'),
            df[['timestamp_ms', 'mid_price']].sort_values('timestamp_ms'),
            left_on='target_ts',
            right_on='timestamp_ms',
            direction='forward',
            suffixes=('', '_future')
        )
        
        merged = merged.sort_values('timestamp_ms').reset_index(drop=True)
        # Calculate Forward Return (BPS)
        df[f'fwd_ret_{h}ms'] = (merged['mid_price'] - df['mid_price']) / df['mid_price'] * 10000
        
        # Drop NaNs
        valid = df.dropna(subset=[f'fwd_ret_{h}ms', 'imbalance'])
        if len(valid) > 100:
            p_corr, p_pval = pearsonr(valid['imbalance'], valid[f'fwd_ret_{h}ms'])
            s_corr, s_pval = spearmanr(valid['imbalance'], valid[f'fwd_ret_{h}ms'])
            
            results.append({
                'Horizon (ms)': h,
                'Pearson r': p_corr,
                'Pearson p': p_pval,
                'Spearman r': s_corr,
                'Spearman p': s_pval
            })

    res_df = pd.DataFrame(results)
    res_df.to_csv(os.path.join(output_dir, 'imbalance_predictive_power.csv'), index=False)
    
    fig = go.Figure()
    fig.add_trace(go.Scatter(x=res_df['Horizon (ms)'], y=res_df['Pearson r'], mode='lines+markers', name='Pearson r'))
    fig.add_trace(go.Scatter(x=res_df['Horizon (ms)'], y=res_df['Spearman r'], mode='lines+markers', name='Spearman rho'))
    fig.update_layout(title='Imbalance Predictive Power Decay', xaxis_title='Prediction Horizon (ms)', yaxis_title='Correlation Coefficient')
    fig.write_html(os.path.join(output_dir, 'predictive_power_decay.html'))
    fig.write_image(os.path.join(output_dir, 'predictive_power_decay.png'))

def analyze_execution_latency(df, trades_df, output_dir):
    if trades_df.empty:
        print("No trades to analyze latency impact.")
        return

    print("Analyzing Execution Latency Impact...")
    # Trades log has 'Time' (HH:MM:SS), making it hard to merge seamlessly if they are spread across days.
    # However, since quant_features has chronological data, we can just assume trades are sequential and we can
    # try to align them, or better, bucket 'system_latency_us' from quant_features directly as a proxy 
    # for the system's busyness during that period and its effect on spread/volatility.
    # Actually, we logged Slippage_BPS in trades_log. Let's merge purely by sequence if lengths match? No.
    # We can group trades by the time string and average them.
    
    df['TimeStr'] = df['datetime'].dt.strftime('%H:%M:%S')
    
    # Average latency per second
    latency_per_sec = df.groupby('TimeStr')['system_latency_us'].mean().reset_index()
    
    # Merge trades with average latency of that second
    trades_df['TimeStr'] = trades_df['Time']
    merged_trades = pd.merge(trades_df, latency_per_sec, on='TimeStr', how='inner')
    
    if len(merged_trades) > 0:
        # Create deciles
        merged_trades['latency_decile'] = pd.qcut(merged_trades['system_latency_us'], 10, labels=False, duplicates='drop')
        
        slippage_by_latency = merged_trades.groupby('latency_decile')['Slippage_BPS'].mean().reset_index()
        
        fig = px.bar(
            slippage_by_latency, x='latency_decile', y='Slippage_BPS',
            title='Average Slippage by System Latency Decile',
            labels={'latency_decile': 'Latency Decile (0=Fastest, 9=Slowest)'}
        )
        fig.write_html(os.path.join(output_dir, 'latency_vs_slippage.html'))
        fig.write_image(os.path.join(output_dir, 'latency_vs_slippage.png'))

def main():
    logs_dir = 'logs'
    # Find the latest log folder
    subdirs = [os.path.join(logs_dir, d) for d in os.listdir(logs_dir) if os.path.isdir(os.path.join(logs_dir, d))]
    if not subdirs:
        print("No logs found.")
        return
    
    # Sort by directory name (timestamp format BTCUSDT_YYYY_MM_DD_HH_MM_SS) instead of mtime to avoid false positives
    latest_log_dir = sorted(subdirs)[-1]
    print(f"Using log directory: {latest_log_dir}")
    
    bin_file = os.path.join(latest_log_dir, 'quant_features.bin')
    trades_file = os.path.join(latest_log_dir, 'trades.log')
    
    output_dir = os.path.join(latest_log_dir, 'plots')
    os.makedirs(output_dir, exist_ok=True)
    
    if os.path.exists(bin_file):
        df = load_quant_features(bin_file)
        print(f"Loaded {len(df)} feature ticks.")
        
        analyze_spread_relationships(df, output_dir)
        analyze_imbalance_predictive_power(df, [100, 500, 1000, 5000, 10000, 30000], output_dir)
        
        trades_df = load_trades(trades_file)
        analyze_execution_latency(df, trades_df, output_dir)
        
        print("Analysis complete. Visualizations saved in", output_dir)
    else:
        print("quant_features.bin not found!")

if __name__ == "__main__":
    main()
