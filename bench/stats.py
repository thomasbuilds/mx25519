# Summarise interleaved mx25519-bench results: "<variant> <ticks/op>" per line.
import random, statistics as st, sys
d = {}
for line in open(sys.argv[1]):
    v, x = line.split(); d.setdefault(v, []).append(float(x))
import itertools
def compare(name, m, c):
    def pct(a, b): return 100 * (b - a) / a
    print(f"runs: master {len(m)}, {name} {len(c)}")
    print(f"master: median {st.median(m):.1f}  min {min(m):.1f}  max {max(m):.1f}")
    print(f"{name}: median {st.median(c):.1f}  min {min(c):.1f}  max {max(c):.1f}")
    print(f"median difference: {pct(st.median(m), st.median(c)):+.3f}%   min-to-min difference: {pct(min(m), min(c)):+.3f}%")
    random.seed(1)
    boot = sorted(pct(st.median(random.choices(m, k=len(m))), st.median(random.choices(c, k=len(c)))) for _ in range(5000))
    print(f"bootstrap 95% interval of the median difference: [{boot[125]:+.3f}%, {boot[4874]:+.3f}%]")
    # Mann-Whitney U, normal approximation
    allv = sorted([(x, 0) for x in m] + [(x, 1) for x in c]); ranks = {}
    i = 0
    while i < len(allv):
        j = i
        while j < len(allv) and allv[j][0] == allv[i][0]: j += 1
        for k in range(i, j): ranks.setdefault(k, (i + j + 1) / 2)
        i = j
    R1 = sum(ranks[k] for k, (x, g) in enumerate(allv) if g == 0); n1, n2 = len(m), len(c)
    U = R1 - n1 * (n1 + 1) / 2; mu = n1 * n2 / 2; sd = (n1 * n2 * (n1 + n2 + 1) / 12) ** 0.5
    from math import erf, sqrt
    z = (U - mu) / sd; p = 2 * (1 - 0.5 * (1 + erf(abs(z) / sqrt(2))))
    print(f"Mann-Whitney: z = {z:+.2f}, two-sided p = {p:.3f}")

for name in ('change', 'control'):
    print(f'===== {name} vs master')
    compare(name, d['master'], d[name])
