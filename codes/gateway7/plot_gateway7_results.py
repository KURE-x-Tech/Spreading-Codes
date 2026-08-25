import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("gateway7_snr_fine_results.csv")

#BER vs SNR plot
plt.plot(df["snr_db"], df["ber"], marker="o")
plt.xlabel("SNR (dB)")
plt.ylabel("BER")
plt.grid(True)
plt.savefig("gateway7_ber_vs_snr_fine.png")
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
plt.savefig("gateway7_acceptance_vs_snr_fine.png")
plt.close()
