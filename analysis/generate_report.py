import os
import glob
import pandas as pd
import google.generativeai as genai
from base64 import b64encode
from dotenv import load_dotenv

# Load environment variables from .env file
load_dotenv()

# Setup API Key
# Usually from environment variable
genai.configure(api_key=os.environ.get("GEMINI_API_KEY", ""))

def read_metrics_summary(log_dir):
    summary_path = os.path.join(log_dir, 'summary.log')
    if os.path.exists(summary_path):
        with open(summary_path, 'r') as f:
            return f.read()
    return ""

def generate_report(log_dir):
    plots_dir = os.path.join(log_dir, 'plots')
    imbalance_csv = os.path.join(plots_dir, 'imbalance_predictive_power.csv')
    
    predictive_power_data = "No imbalance data found."
    if os.path.exists(imbalance_csv):
        df = pd.read_csv(imbalance_csv)
        predictive_power_data = df.to_csv(index=False)
        
    engine_summary = read_metrics_summary(log_dir)

    prompt = f"""
You are a Quantitative Researcher analyzing a newly built ultra-low latency C++ hybrid L2/L3 order book engine.

System metrics summary:
{engine_summary}

Imbalance Predictive Power (Pearson & Spearman over time horizons):
{predictive_power_data}

Instructions for the report:
1. Write a formal, academic-tier technical microstructural report analyzing current market conditions, evaluating how well the newly implemented C++ system has performed, and deriving insights into the predictive power of order book imbalances.
2. Maintain a highly professional, objective tone appropriate for quantitative researchers and high-frequency trading engineers. Avoid casual language.
3. I will provide you with several images (plots) that were generated from our live data.
4. For EVERY image I provide, you must include it directly in your response using standard Markdown image syntax exactly as I specify, and IMMEDIATELY BELOW it, write a formal and specific analytical explanation of what the chart implies regarding market variables or the system's execution pipeline.
5. Format the final output entirely in a structured, professional Markdown document.
"""

    contents = [prompt]
    import PIL.Image
    
    plots_info = [
        ("Spread vs Realized Volatility", "spread_vs_vol.png"),
        ("Spread vs Order Flow Imbalance", "spread_vs_imbalance.png"),
        ("Spread vs Liquidity Depth Slope", "spread_vs_depth_slope.png"),
        ("Imbalance Predictive Power Decay", "predictive_power_decay.png"),
        ("Average Slippage by System Latency", "latency_vs_slippage.png")
    ]

    for title, filename in plots_info:
        img_path = os.path.join(plots_dir, filename)
        if os.path.exists(img_path):
            try:
                img = PIL.Image.open(img_path)
                contents.append(f"\nHere is the plot for: {title}. Its filename is {filename}. Embed it now using this exact syntax: `![{title}](./plots/{filename})` and provide a one paragraph explanation below it.")
                contents.append(img)
            except Exception as e:
                print(f"Skipping image {filename} due to error: {e}")

    print("Requesting Gemini to write the report with inline images...")
    try:
        model = genai.GenerativeModel('gemini-2.5-flash')
        response = model.generate_content(contents)
        llm_text = response.text
    except Exception as e:
        print(f"Error calling Gemini: {e}")
        llm_text = f"""
# Quantitative Technical Report
### Note: LLM Generation failed due to lack of API key or connection error.
**System Summary:**
```
{engine_summary}
```
**Imbalance Predictive Power:**
```csv
{predictive_power_data}
```
"""

    md_content = f"""# Quantitative Strategy & Market Microstructure Report

{llm_text}
"""
    
    report_path = os.path.join(log_dir, 'technical_report.md')
    with open(report_path, 'w') as f:
        f.write(md_content)
    print(f"Report generated successfully at: {report_path}")

def main():
    logs_dir = 'logs'
    # Find latest log dir
    subdirs = [os.path.join(logs_dir, d) for d in os.listdir(logs_dir) if os.path.isdir(os.path.join(logs_dir, d))]
    if not subdirs:
        print("No logs found.")
        return
    
    latest_log_dir = sorted(subdirs)[-1]
    generate_report(latest_log_dir)

if __name__ == "__main__":
    main()
