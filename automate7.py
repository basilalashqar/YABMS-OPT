#!/usr/bin/env python3
import subprocess, shlex

# Path to your built mmult binary
MMULT = "./build/mmult"

# The five datasets: (name, M, N, P)
DATASETS = [
    ("testing",  16,   12,    8),
    ("small",   121,  180,  115),
    ("medium",  550,  620,  480),
    ("large",   962, 1012, 1221),
    ("native", 2500, 3000, 2100),
]

# Blocks to sweep
BLOCK_SIZES = [4, 8, 16, 32, 64, 128]

def run_mmult(impl, M, N, P, b=None):
    """Run mmult once; return (matched_ref, runtime_ns)."""
    cmd = f"{MMULT} -i {impl} --M {M} --N {N} --P {P} --nruns 1 --nstdevs 3"
    if impl == "opt":
        cmd += f" --b {b}"
    out = subprocess.check_output(shlex.split(cmd),
                                  text=True, stderr=subprocess.STDOUT)
    ok = "Verifying results ... Success" in out
    t = None
    for line in reversed(out.splitlines()):
        if "Final runtime:" in line:
            t = int(line.split("Final runtime:")[1].strip().split()[0])
            break
    return ok, t

# Build header:
hdr = ["Dataset", "M×N×P", "naive(ns)"]
for b in BLOCK_SIZES:
    hdr += [f"opt b={b}", "spd"]
hdr += ["MATCH?"]

# Column widths (uniform)
W = 12
col_widths = [W]*len(hdr)

# Build separator lines
border = "+" + "+".join("-"*w for w in col_widths) + "+"

# Print top border
print(border)
# Header row
print("|" + "|".join(f"{h:^{w}}" for h, w in zip(hdr, col_widths)) + "|")
# Separator below header
print(border)

# Print each dataset row
for name, M, N, P in DATASETS:
    ok_n, t_n = run_mmult("naive", M, N, P)
    dims = f"{M}×{N}×{P}"
    row = [name, dims, str(t_n)]

    all_ok = ok_n
    for b in BLOCK_SIZES:
        ok_o, t_o = run_mmult("opt", M, N, P, b)
        spd = f"{t_n / t_o:.2f}×" if (t_o and t_n) else "  N/A  "
        row += [str(t_o), spd]
        all_ok &= ok_o

    row.append("Y" if all_ok else "N")
    # Print the row
    print("|" + "|".join(f"{c:^{w}}" for c, w in zip(row, col_widths)) + "|")
    # Print a border after each row
    print(border)

