#!/usr/bin/python3

import sys, os, time

if len(sys.argv) < 2:
    print("Usage: {} PROGRAM [ARGS...]".format(sys.argv[0]), file=sys.stderr)
    exit(1)

program = sys.argv[1]

if not os.path.isfile(program):
    print("Program '{}' does not exist".format(program), file=sys.stderr)
    exit(1)

print("Starting program {}...".format(program))

while True:
    time.sleep(1)
    print("Program is running...", flush=True)