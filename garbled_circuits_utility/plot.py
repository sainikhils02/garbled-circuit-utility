#!/usr/bin/env python3
import subprocess
import random
import re
import time
import matplotlib.pyplot as plt
import os
import socket

GARB = "./build/garbler"
EVAL = "./build/evaluator"
HOST = "localhost"
CIRCUIT = "./examples/mult64.txt"

circuit_name = os.path.basename(CIRCUIT).replace(".txt", "")
os.makedirs("plots", exist_ok=True)


runs = 10


def get_free_port():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.bind((HOST, 0))
        return str(sock.getsockname()[1])
    finally:
        sock.close()

def rand_bits(n=64):
    return ''.join(random.choice("01") for _ in range(n))
    # return '0000000000000000000000000000000000000000000000000000000000010100'

def run_once(use_pandp, g_input, e_input):
    port = get_free_port()
    cmd_g = [GARB, "--port", port, "--circuit", CIRCUIT, "--input", g_input]
    cmd_e = [EVAL, "--host", HOST, "--port", port, "--input", e_input]
    if use_pandp:
        cmd_g.append("--pandp")
        cmd_e.append("--pandp")

    p_g = subprocess.Popen(cmd_g, stdout=subprocess.PIPE, text=True)
    time.sleep(0.3)

    p_e = subprocess.Popen(cmd_e, stdout=subprocess.PIPE, text=True)

    out_g = out_e = None
    try:
        out_g = p_g.communicate()[0]
        out_e = p_e.communicate()[0]
    finally:
        if p_e.poll() is None:
            p_e.kill()
        if p_g.poll() is None:
            p_g.kill()

    if p_g.returncode != 0:
        raise RuntimeError(f"Garbler exited with {p_g.returncode}\n{out_g}")
    if p_e.returncode != 0:
        raise RuntimeError(f"Evaluator exited with {p_e.returncode}\n{out_e}")

    eval_time = None
    decrypted = None
    gates = None

    for line in out_e.splitlines():
        m1 = re.search(r"Evaluation time: ([0-9]+) microseconds", line)
        m2 = re.search(r"Decrypted ([0-9]+) ciphers", line)
        m3 = re.search(r"Successfully evaluated ([0-9]+) gates", line)
        if m1:
            eval_time = int(m1.group(1))
        if m2:
            decrypted = int(m2.group(1))
        if m3:
            gates = int(m3.group(1))

    if eval_time is None or decrypted is None or gates is None:
        raise RuntimeError("Failed to parse evaluator output:\n" + out_e)

    return eval_time, decrypted, gates


times_with = []
times_without = []
dec_with = []
dec_without = []
gates_with = []
gates_without = []

for _ in range(runs):
    g_in = rand_bits()
    e_in = rand_bits()

    t, d, g = run_once(False, g_in, e_in)
    times_without.append(t)
    dec_without.append(d)
    gates_without.append(g)

    # g_in = rand_bits()
    # e_in = rand_bits()
    t, d, g = run_once(True, g_in, e_in)
    times_with.append(t)
    dec_with.append(d)
    gates_with.append(g)


# Plot evaluation times
plt.figure()
plt.plot(times_without, label="No pandp")
plt.plot(times_with, label="With pandp")
plt.title("Evaluation time")
plt.xlabel("Run")
plt.ylabel("Microseconds")
plt.legend()
plt.savefig(f"plots/{circuit_name}_evaluation_time.png")
plt.close()

# Plot ciphers decrypted and gates evaluated
plt.figure()
plt.plot(dec_without, label="Ciphers decrypted (No pandp)")
plt.plot(dec_with, label="Ciphers decrypted (With pandp)")
# plt.plot(gates_without, label="Gates evaluated")
plt.title("Ciphers decrypted & Gates evaluated")
plt.xlabel("Run")
plt.ylabel("Count")
plt.legend()
plt.savefig(f"plots/{circuit_name}_ciphers_decrypted.png")
plt.close()