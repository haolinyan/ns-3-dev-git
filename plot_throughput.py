import pandas as pd
import matplotlib.pyplot as plt 
import numpy as np

df = pd.read_csv("firstClientTx.csv", sep=",")
beding_time = 0
time_interval = 5000


num_timestamps = np.ceil(df.iloc[-1]["Time(ns)"] / time_interval)
print("Total number of timestamps: ", num_timestamps)
y = [0] * int(num_timestamps)
X = list(range(time_interval, (1+int(num_timestamps)) * time_interval, time_interval))


index = 0
for i in range(int(num_timestamps)):
    for j in range(index, len(df)):
        if df.iloc[j]["Time(ns)"] < (i+1) * time_interval and df.iloc[j]["Time(ns)"] >= i * time_interval:
            y[i] += df.iloc[j]["Bytes"]
        else:
            index = j
            y[i] = y[i] * 8 / time_interval
            break
y[i] = y[i] * 8 / time_interval


total_time = df.iloc[-1]["Time(ns)"] - df.iloc[0]["Time(ns)"]
total_bytes = df["Bytes"].sum()
total_throughput = total_bytes * 8 / total_time
print("Total throughput: ", total_throughput)
plt.plot(X, y)
plt.xlabel("Time(ns)")
plt.ylabel("Throughput (Gps)")
plt.title("Throughput vs Time (resolution = %dns)" % time_interval)
plt.show()