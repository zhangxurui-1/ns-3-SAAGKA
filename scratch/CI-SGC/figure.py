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
    [63, 250, 562, 1015, 1542, 2419, 3082, 3971, 5491, 6497],
    [583, 2247, 5172, 9035, 14309, 21195, 28544, 37172, 45599, 56901],
]
line_labels1 = ["AES-80-bit", "AES-128-bit"]

ys2=[
    [0.406, 0.409, 0.448, 0.404, 0.413, 0.410, 0.421, 0.405, 0.432, 0.461],
    [4.013, 3.719, 3.726, 3.723, 3.926, 3.947, 3.752, 3.729, 3.734, 3.874]
]
line_labels2 = ["AES-80-bit", "AES-128-bit"]

draw_two(group_size, group_size, ys1, ys2,
         "Time Cost of Global Setup", "Time Cost of Enrollment",
         "Group Size", "Execution Time (ms)", line_labels1,
         "Group Size", "Execution Time (ms)", line_labels2,
         save_path="/Users/zxr/workspace/ns-3-dev/scratch/CI-SGC/log/figure/setup_keygen.pdf")