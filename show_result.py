import pandas as pd
import matplotlib.pyplot as plt
SKIP = 1

def plot_windowsize():
    plt.clf()
    df = pd.read_csv("WindowSizeTraced.csv", sep=",")
    plt.plot(df["Time"], df["WindowSize"], "-o", label="WindowSize")
    for i in range(len(df)):
        if (df.iloc[i]["Ecn"]):
            plt.scatter(df.iloc[i]["Time"], df.iloc[i]["WindowSize"], color="red")

    plt.xlabel("Time (ns)")
    plt.ylabel("WindowSize (bytes)")
    plt.title("WindowSize")
    plt.legend()
    plt.grid()
    plt.show()

def plot_thoughput():
    plt.clf()
    df_w0 = pd.read_csv("W0Throughput.csv", sep=",").iloc[::SKIP]
    df_w1 = pd.read_csv("W1Throughput.csv", sep=",")
    num_points = len(df_w0)
    df_w1 = df_w1[:num_points]

    nanosec = df_w0["Time"].iloc[-1] - df_w0["Time"].iloc[0]
    total_RX = df_w0["Rx(Gbps)"].sum()
    print(nanosec / 1e9)
    print(total_RX / (nanosec / 1e9))

    # plt.plot(df_w0["Time"], df_w0["Tx(Gbps)"], label="W0:Tx(Gbps)")
    plt.plot(df_w0["Time"], df_w0["Rx(Gbps)"], label="W0:Rx(Gbps)")
    plt.plot(df_w1["Time"], df_w1["Tx(Gbps)"], label="W1:Tx(Gbps)")
    # plt.plot(df_w1["Time"], df_w1["Rx(Gbps)"], label="W1:Rx(Gbps)")
    # 设置坐标轴刻度间隔为100
    plt.yticks(range(0, 100, 10))
    plt.xlabel("Time (ns)")
    plt.ylabel("Throughput (Gbps)")
    plt.title("Throughput of W0 and W1(PS)")
    plt.legend()
    plt.grid()
    plt.show()



if __name__ == "__main__":
    plot_windowsize()
    plot_thoughput()