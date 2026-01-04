"""
Post-processing and analysis of trading logs.
Generates performance metrics, plots, and summary statistics.
"""

import argparse
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from pathlib import Path
import glob


class LogAnalyzer:
    """Analyzes trading logs and generates performance reports."""
    
    def __init__(self, log_dir: str = "./logs"):
        self.log_dir = Path(log_dir)
        self.trades_df = None
        self.latency_df = None
        self.inventory_df = None
        self.pnl_df = None
        self.orderbook_df = None
        
    def load_logs(self):
        """Load all log files into dataframes."""
        try:
            log_files_exist = list(self.log_dir.glob("*.log"))
            
            if not log_files_exist:
                subdirs = sorted([d for d in self.log_dir.iterdir() if d.is_dir()])
                if subdirs:
                    print(f"[INFO] No logs found in {self.log_dir}, using latest subdirectory: {subdirs[-1].name}")
                    self.log_dir = subdirs[-1]
                else:
                    print(f"[WARN] No log files or subdirectories found in {self.log_dir}")
                    return

            # Find log files in the directory
            trades_files = glob.glob(str(self.log_dir / "trades.log"))
            latency_files = glob.glob(str(self.log_dir / "latency.log"))
            inventory_files = glob.glob(str(self.log_dir / "inventory.log"))
            pnl_files = glob.glob(str(self.log_dir / "pnl.log"))
            orderbook_files = glob.glob(str(self.log_dir / "orderbook.log"))
            
            if trades_files:
                self.trades_df = pd.read_csv(sorted(trades_files)[-1])
                print(f"[INFO] Loaded {len(self.trades_df)} trades")
            
            if latency_files:
                self.latency_df = pd.read_csv(sorted(latency_files)[-1])
                print(f"[INFO] Loaded {len(self.latency_df)} latency records")
            
            if inventory_files:
                self.inventory_df = pd.read_csv(sorted(inventory_files)[-1])
                print(f"[INFO] Loaded {len(self.inventory_df)} inventory records")
            
            if pnl_files:
                self.pnl_df = pd.read_csv(sorted(pnl_files)[-1])
                print(f"[INFO] Loaded {len(self.pnl_df)} PnL records")
            
            if orderbook_files:
                self.orderbook_df = pd.read_csv(sorted(orderbook_files)[-1])
                print(f"[INFO] Loaded {len(self.orderbook_df)} order book snapshots")
                
        except Exception as e:
            print(f"[ERROR] Failed to load logs: {e}")
    
    def calculate_statistics(self):
        """Calculate trading and system statistics."""
        stats = {}
        
        # Trading statistics
        if self.trades_df is not None and len(self.trades_df) > 0:
            stats['total_trades'] = len(self.trades_df)
            stats['buy_trades'] = len(self.trades_df[self.trades_df['Side'] == 'BUY'])
            stats['sell_trades'] = len(self.trades_df[self.trades_df['Side'] == 'SELL'])
            stats['avg_trade_price'] = self.trades_df['Price'].mean()
            stats['total_volume'] = self.trades_df['Quantity'].sum()
        
        # PnL statistics
        if self.pnl_df is not None and len(self.pnl_df) > 0:
            stats['final_pnl'] = self.pnl_df['NetPnL'].iloc[-1]
            stats['max_pnl'] = self.pnl_df['NetPnL'].max()
            stats['min_pnl'] = self.pnl_df['NetPnL'].min()
            stats['max_drawdown'] = (self.pnl_df['NetPnL'].cummax() - self.pnl_df['NetPnL']).max()
            
            # Sharpe ratio (simplified, assuming returns)
            if len(self.pnl_df) > 1:
                returns = self.pnl_df['NetPnL'].diff().dropna()
                if returns.std() > 0:
                    stats['sharpe_ratio'] = returns.mean() / returns.std() * np.sqrt(252)
        
        # Latency statistics
        if self.latency_df is not None and len(self.latency_df) > 0:
            latencies = self.latency_df['Engine_Latency_us']
            stats['avg_latency_us'] = latencies.mean()
            stats['p50_latency_us'] = latencies.quantile(0.50)
            stats['p95_latency_us'] = latencies.quantile(0.95)
            stats['p99_latency_us'] = latencies.quantile(0.99)
            stats['max_latency_us'] = latencies.max()
        
        # Order book statistics
        if self.orderbook_df is not None and len(self.orderbook_df) > 0:
            stats['avg_spread'] = self.orderbook_df['Spread'].mean()
            stats['avg_imbalance'] = self.orderbook_df['Imbalance'].mean()
        
        return stats
    
    
    def plot_pnl_curve(self, output_file: str = "pnl_curve.png"):
        """Plot PnL over time."""
        if self.pnl_df is None or len(self.pnl_df) == 0:
            print("[WARN] No PnL data to plot")
            return
        
        output_path = self.log_dir / output_file
        
        plt.figure(figsize=(12, 6))
        plt.plot(self.pnl_df.index, self.pnl_df['GrossPnL'], label='Gross PnL', alpha=0.7)
        plt.plot(self.pnl_df.index, self.pnl_df['NetPnL'], label='Net PnL', linewidth=2)
        plt.xlabel('Event Index')
        plt.ylabel('PnL ($)')
        plt.title('Profit and Loss Over Time')
        plt.legend()
        plt.grid(True, alpha=0.3)
        plt.tight_layout()
        plt.savefig(output_path, dpi=150)
        print(f"[INFO] Saved PnL curve to {output_path}")
        plt.close()
    
    def plot_inventory(self, output_file: str = "inventory.png"):
        """Plot inventory over time."""
        if self.inventory_df is None or len(self.inventory_df) == 0:
            print("[WARN] No inventory data to plot")
            return
        
        output_path = self.log_dir / output_file
        
        plt.figure(figsize=(12, 6))
        plt.plot(self.inventory_df.index, self.inventory_df['Position'], linewidth=2)
        plt.axhline(y=0, color='r', linestyle='--', alpha=0.5)
        plt.xlabel('Event Index')
        plt.ylabel('Position (BTC)')
        plt.title('Inventory Over Time')
        plt.grid(True, alpha=0.3)
        plt.tight_layout()
        plt.savefig(output_path, dpi=150)
        print(f"[INFO] Saved inventory plot to {output_path}")
        plt.close()
    
    def plot_latency(self, output_file: str = "latency.png"):
        """Plot latency distribution."""
        if self.latency_df is None or len(self.latency_df) == 0:
            print("[WARN] No latency data to plot")
            return
        
        output_path = self.log_dir / output_file
        
        latencies = self.latency_df['Engine_Latency_us']
        
        plt.figure(figsize=(12, 6))
        plt.hist(latencies, bins=50, edgecolor='black', alpha=0.7)
        plt.axvline(latencies.mean(), color='r', linestyle='--', label=f'Mean: {latencies.mean():.2f} μs')
        plt.axvline(latencies.quantile(0.95), color='g', linestyle='--', label=f'P95: {latencies.quantile(0.95):.2f} μs')
        plt.axvline(latencies.quantile(0.99), color='orange', linestyle='--', label=f'P99: {latencies.quantile(0.99):.2f} μs')
        plt.xlabel('Latency (μs)')
        plt.ylabel('Frequency')
        plt.title('Engine Processing Latency Distribution')
        plt.legend()
        plt.grid(True, alpha=0.3)
        plt.tight_layout()
        plt.savefig(output_path, dpi=150)
        print(f"[INFO] Saved latency histogram to {output_path}")
        plt.close()
    
    def plot_order_book_depth(self, output_file: str = "orderbook_depth.png"):
        """Plot order book metrics over time."""
        if self.orderbook_df is None or len(self.orderbook_df) == 0:
            print("[WARN] No order book data to plot")
            return
        
        output_path = self.log_dir / output_file
        
        fig, axes = plt.subplots(3, 1, figsize=(12, 10))
        
        # Mid price
        axes[0].plot(self.orderbook_df.index, self.orderbook_df['MidPrice'], linewidth=1)
        axes[0].set_ylabel('Mid Price ($)')
        axes[0].set_title('Order Book Metrics Over Time')
        axes[0].grid(True, alpha=0.3)
        
        # Spread
        axes[1].plot(self.orderbook_df.index, self.orderbook_df['Spread'], linewidth=1, color='orange')
        axes[1].set_ylabel('Spread ($)')
        axes[1].grid(True, alpha=0.3)
        
        # Imbalance
        axes[2].plot(self.orderbook_df.index, self.orderbook_df['Imbalance'], linewidth=1, color='green')
        axes[2].axhline(y=0, color='r', linestyle='--', alpha=0.5)
        axes[2].set_ylabel('Imbalance')
        axes[2].set_xlabel('Event Index')
        axes[2].grid(True, alpha=0.3)
        
        plt.tight_layout()
        plt.savefig(output_path, dpi=150)
        print(f"[INFO] Saved order book depth plot to {output_path}")
        plt.close()
    
    def generate_report(self):
        """Generate comprehensive analysis report and save to file."""
        report_lines = []
        report_lines.append("="*60)
        report_lines.append("MARKET MICROSTRUCTURE ANALYSIS REPORT")
        report_lines.append("="*60)
        
        stats = self.calculate_statistics()
        
        report_lines.append("\n--- Trading Statistics ---")
        for key, value in stats.items():
            if 'trade' in key.lower() or 'volume' in key.lower():
                report_lines.append(f"{key}: {value}")
        
        report_lines.append("\n--- Performance Metrics ---")
        for key, value in stats.items():
            if 'pnl' in key.lower() or 'sharpe' in key.lower() or 'drawdown' in key.lower():
                report_lines.append(f"{key}: {value:.4f}")
        
        report_lines.append("\n--- System Metrics ---")
        for key, value in stats.items():
            if 'latency' in key.lower():
                report_lines.append(f"{key}: {value:.2f}")
        
        report_lines.append("\n--- Market Microstructure ---")
        for key, value in stats.items():
            if 'spread' in key.lower() or 'imbalance' in key.lower():
                report_lines.append(f"{key}: {value:.6f}")
        
        report_lines.append("\n" + "="*60)
        
        report_content = "\n".join(report_lines)
        print(report_content)
        
        summary_path = self.log_dir / "summary.txt"
        with open(summary_path, "w") as f:
            f.write(report_content)
        print(f"\n[INFO] Saved summary report to {summary_path}")

def main():
    """Main analysis entry point."""
    parser = argparse.ArgumentParser(description="Analyze trading logs.")
    parser.add_argument("--log_dir", type=str, default="../logs", help="Directory containing log files")
    args = parser.parse_args()
    
    analyzer = LogAnalyzer(log_dir=args.log_dir)
    
    print(f"[INFO] Analyzing logs in: {args.log_dir}")
    analyzer.load_logs()
    
    print("\n[INFO] Generating plots...")
    analyzer.plot_pnl_curve()
    analyzer.plot_inventory()
    analyzer.plot_latency()
    analyzer.plot_order_book_depth()
    
    print("\n[INFO] Generating report...")
    analyzer.generate_report()
    
    print("\n[INFO] Analysis complete!")

if __name__ == "__main__":
    main()
