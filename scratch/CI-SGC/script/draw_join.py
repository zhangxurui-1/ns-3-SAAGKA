import matplotlib.pyplot as plt

plt.rcParams.update({
    "font.size": 12,
    "font.family": "serif"
})

def draw(x, ys, x_label, y_label, line_labels,
         title=None, save_path=None):
    """
    Draw a line plot with multiple curves.

    Parameters:
    - x: list or array, x-axis data
    - ys: list of lists, each list is y-axis data for one curve
    - x_label: str, label of x-axis
    - y_label: str, label of y-axis
    - line_labels: list of str, labels for each curve
    - title: str, optional, figure title
    - save_path: str, optional, path to save the figure
    """

    # -------- basic sanity checks --------
    if len(ys) != len(line_labels):
        raise ValueError("ys and line_labels must have the same length")

    for y in ys:
        if len(y) != len(x):
            raise ValueError("Each y must have the same length as x")

    # -------- marker styles (cycle) --------
    markers = ['o', 's', '^', 'D', 'v', 'x', '*', '+']

    plt.figure(figsize=(6, 4))

    for i, y in enumerate(ys):
        plt.plot(
            x,
            y,
            marker=markers[i % len(markers)],
            linewidth=2,
            label=line_labels[i]
        )

    plt.xlabel(x_label)
    plt.ylabel(y_label)

    if title is not None:
        plt.title(title)

    plt.legend()
    plt.grid(True)
    plt.tight_layout()

    if save_path is not None:
        plt.savefig(save_path)

    plt.show()

def draw_two(x1, x2, ys1, ys2, title1, title2,
             x_label1, y_label1, line_labels1,
             x_label2, y_label2, line_labels2,
             save_path=None):

    fig, axes = plt.subplots(1, 2, figsize=(10, 4))

    # -------- 左图 --------
    for y, label in zip(ys1, line_labels1):
        axes[0].plot(x1, y, marker='o', linewidth=2, label=label)

    axes[0].set_title(title1)
    axes[0].set_xlabel(x_label1)
    axes[0].set_ylabel(y_label1)
    axes[0].legend(frameon=False)
    axes[0].grid(True, linestyle='--', alpha=0.5)

    # -------- 右图 --------
    for y, label in zip(ys2, line_labels2):
        axes[1].plot(x2, y, marker='s', linewidth=2, label=label)

    axes[1].set_title(title2)
    axes[1].set_xlabel(x_label2)
    axes[1].set_ylabel(y_label2)
    axes[1].legend(frameon=False)
    axes[1].grid(True, linestyle='--', alpha=0.5)

    plt.tight_layout()

    if save_path:
        plt.savefig(save_path, bbox_inches='tight')

    plt.show()



group_size = [10, 20, 30, 40, 50, 60, 70, 80, 90, 100]

ys1=[
    [7.216, 13.229, 21.212, 25.729, 31.436, 40.111, 44.072, 51.122, 55.633, 63.858],
    [0.995, 1.014, 1.056, 1.129, 1.134, 1.225, 1.221, 1.258, 1.284, 1.354],
    [0.98, 0.96, 1.016, 0.983, 0.961, 0.959, 0.994, 1.024, 0.969, 1]
]
line_labels1 = ["Joining-Step 1", "Joining-Step 2", "Joining-Step 3"]

ys2=[
    [66.645, 137.828, 180.183, 237.834, 291.581, 352.059, 410.259, 470.196, 522.657, 573.358],
    [9.83, 11.04, 10.418, 10.642, 10.816, 10.995, 11.275, 12.014, 11.918, 11.949],
    [9.685, 10.516, 9.756, 9.783, 9.735, 10.111, 10.37, 10.052, 10.046, 9.801]
]
line_labels2 = ["Joining-Step 1", "Joining-Step 2", "Joining-Step 3"]

draw_two(group_size, group_size, ys1, ys2,
         "Fine-grained time cost of Joining (80-bit)", "Fine-grained time cost of Joining (128-bit)",
         "Group Size", "Execution Time (ms)", line_labels1,
         "Group Size", "Execution Time (ms)", line_labels2,
         save_path="/Users/zxr/workspace/ns-3-dev/scratch/CI-SGC/log/figure/join.pdf")


# kTotalJoin:[15.666, 27.355, 41.033, 51.225, 62.605, 77.014, 86.607, 99.014, 109.156, 123.067, 97.669, 186.771, 244.66, 319.114, 389.34, 466.581, 541.396, 618.644, 687.65, 754.552, ]
# kComputeJoinStep4:[0.98, 0.96, 1.016, 0.983, 0.961, 0.959, 0.994, 1.024, 0.969, 1, 
#                   9.685, 10.516, 9.756, 9.783, 9.735, 10.111, 10.37, 10.052, 10.046, 9.801, ]
# kTotalHearbeat:[1.943, 1.499, 1.513, 1.561, 1.518, 1.525, 1.509, 1.501, 1.64, 1.578, 3.951, 3.188, 3.172, 3.176, 3.184, 3.197, 3.206, 3.201, 3.212, 3.225, ]
# kTotalKeyDistribution:[5.369, 5.272, 6.115, 5.965, 5.655, 5.647, 5.25, 7.149, 5.8, 5.47, 19.491, 37.878, 22.364, 23.235, 24.999, 23.964, 25.214, 24.382, 26.748, 23.242, ]
# kComputeEncap:[0.52, 0.463, 0.467, 0.477, 0.467, 0.464, 0.499, 0.502, 0.465, 0.463, 4.671, 4.299, 4.239, 4.288, 4.354, 4.262, 4.372, 4.471, 4.462, 4.267, ]
# kTotalKeyUpdate2:[7.745, 7.419, 7.026, 7.823, 7.263, 7.166, 7.005, 7.092, 8.101, 7.773, 15.06, 14.086, 13.277, 14.87, 14.369, 14.589, 14.451, 14.301, 14.829, 15.903, ]
# kComputeDecap:[1.049, 1.032, 1.048, 1.035, 1.017, 1.017, 1.046, 1.084, 1.018, 1.023, 11.577, 16.049, 11.654, 11.834, 12.043, 11.651, 11.963, 12.152, 12.061, 11.818, ]
# kComputeJoinStep2:[0.995, 1.014, 1.056, 1.129, 1.134, 1.225, 1.221, 1.258, 1.284, 1.354, 
#               9.83, 11.04, 10.418, 10.642, 10.816, 10.995, 11.275, 12.014, 11.918, 11.949, ]
# kComputeInitOneGroup:[2.802, 5.515, 31.47, 12.403, 15.84, 19.66, 23.849, 28.476, 34.434, 40.191, 25.654, 49.509, 72.083, 98.758, 127.322, 158.587, 191.565, 262.245, 264.18, 324.191, ]
# kComputeJoinStep3:[0.002, 0.002, 0.002, 0.003, 0.004, 0.003, 0.003, 0.003, 0.002, 0.003, 0.018, 0.021, 0.018, 0.018, 0.017, 0.018, 0.018, 0.018, 0.019, 0.018, ]
# kComputeKeyGen:[0.407, 0.414, 0.443, 0.412, 0.414, 0.407, 0.407, 0.413, 0.426, 0.425, 3.696, 4.373, 3.713, 3.723, 3.701, 3.716, 3.701, 3.691, 3.741, 3.757, ]
# kComputeSetup:[63.995, 249.822, 560.635, 1006.78, 1697.8, 2516.77, 3056.51, 3975.29, 5019.91, 6251.69, 580.958, 2358.06, 5005.29, 9307.58, 13904.9, 20475.3, 28061, 36323.6, 46524.2, 56758.1, ]
# kTotalKeyUpdate1:[4.097, 5.978, 4.88, 4.958, 4.382, 6.207, 5.025, 5.364, 4.404, 5.427, 21.837, 25.148, 26.812, 21.884, 21.988, 21.965, 21.934, 24.049, 22.335, 22.118, ]
# kComputeJoinStep1:[7.216, 13.229, 21.212, 25.729, 31.436, 40.111, 44.072, 51.122, 55.633, 63.858,
#                    66.645, 137.828, 180.183, 237.834, 291.581, 352.059, 410.259, 470.196, 522.657, 573.358, ]
# kUnknown:[0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ]