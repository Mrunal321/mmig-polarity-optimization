#!/usr/bin/env python3
"""One-column vector figure for the matched M-versus-P edge comparison."""

import sys
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

sys.path.insert(0, str(Path(__file__).resolve().parent))
from generate_tables import HERE, ORDER, load, pct


def main():
    data, _ = load()
    values = [pct(int(data[name, 'M']['I_nonconst']),
                  int(data[name, 'P']['I_nonconst'])) for name in ORDER]
    plt.rcParams.update({'font.family': 'DejaVu Sans', 'font.size': 8,
                         'pdf.fonttype': 42, 'ps.fonttype': 42})
    fig, ax = plt.subplots(figsize=(3.4, 3.0), layout='constrained')
    ys = range(len(ORDER))
    ax.barh(ys, values, height=0.67, color='#274760')
    ax.scatter([0], [0], marker='o', s=12, color='#274760', zorder=3)
    ax.axvline(0, color='black', linewidth=0.7)
    ax.set_yticks(list(ys), ORDER)
    ax.invert_yaxis()
    ax.set_xlim(-55, 3)
    ax.set_xticks([-50, -40, -30, -20, -10, 0])
    ax.set_xlabel('Change in nonconstant complemented edges (%)')
    ax.grid(axis='x', color='#dddddd', linewidth=0.45)
    ax.set_axisbelow(True)
    ax.spines[['top', 'right', 'left']].set_visible(False)
    ax.tick_params(axis='y', length=0)
    dest = HERE / 'figures'
    dest.mkdir(exist_ok=True)
    fig.savefig(dest / 'm_vs_p_edges.pdf')
    fig.savefig(dest / 'm_vs_p_edges.png', dpi=300)
    plt.close(fig)
    print('Wrote m_vs_p_edges.pdf and .png')


if __name__ == '__main__':
    main()
