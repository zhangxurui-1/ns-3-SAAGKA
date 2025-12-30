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
    [1.655, 1.475, 1.463, 1.485, 1.497, 1.456, 1.506, 1.499, 1.506, 1.492],
    [15.593, 27.27, 38.941, 50.571, 62.279, 75.193, 85.303, 96.907, 110.842, 120.393],
    [5.211, 5.64, 6.586, 7.74, 5.429, 5.514, 6.098, 6.35, 6.428, 6.107],
    [6.205, 4.86, 5.572, 5.606, 6.198, 6.295, 6.58, 4.985, 6.154, 4.838]
]
line_labels1 = ["Prepare", "Joining", "Session Key Distribution", "Session Key Update"]

ys2=[
    [3.686, 3.178, 3.179, 3.179, 3.183, 3.18, 3.189, 3.199, 3.199, 3.204],
    [97.229, 182.554, 257.374, 314.095, 391.722, 474.71, 534.482, 596.094, 671.694, 749.108],
    [23.991, 23.98, 24.165, 24.076, 23.68, 26.102, 23.097, 22.987, 22.992, 24.95],
    [24.31, 21.858, 21.168, 21.698, 21.956, 22.851, 21.998, 21.582, 22.495, 24.163]
]
line_labels2 = ["Prepare", "Joining", "Session Key Distribution", "Session Key Update"]

draw_two(group_size, group_size, ys1, ys2,
         "Time Cost of Key Establishment (80-bit)", "Time Cost of Key Establishment (128-bit)",
         "Group Size", "Execution Time (ms)", line_labels1,
         "Group Size", "Execution Time (ms)", line_labels2,
         save_path="/Users/zxr/workspace/ns-3-dev/scratch/CI-SGC/log/figure/key_estab.pdf")
