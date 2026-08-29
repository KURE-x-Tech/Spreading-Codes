import pandas as pd
import matplotlib.pyplot as plt
from pathlib import Path

repo_root = Path(__file__).resolve().parents[2]
result_dir = repo_root / "Validation" / "generated" / "gateway7"

df = pd.read_csv(result_dir / "gateway7_snr_results.csv")

#BER vs SNR plot
plt.plot(df["snr_db"], df["ber"], marker="o")
plt.xlabel("SNR (dB)")
plt.ylabel("BER")
plt.grid(True)
plt.savefig(result_dir / "gateway7_ber_vs_snr.png")
plt.close()

#Accepted Frames vs SNR plot
plt.plot(
    df["snr_db"],
    df["acceptance_rate"] * 100,
    marker="o"
)
plt.xlabel("SNR (dB)")
plt.ylabel("Frame Acceptance (%)")
plt.ylim(0, 105)
plt.grid(True)
plt.savefig(result_dir / "gateway7_acceptance_vs_snr.png")
plt.close()
