# EchoRun — Complete Test Guide

All commands assume you are inside `~/EchoRun/final/`.
Build all three modules before running any test.

---

## Build Everything First

```bash
cd ~/EchoRun/final/recorder   && make all
cd ~/EchoRun/final/replayer   && make all
cd ~/EchoRun/final/visualizer && make all
```

Expected: zero errors, zero warnings for all three.

---

## TEST 1 — Basic Record + Replay (tracee.c)

**What it proves:** Core record/replay pipeline works. stdin read is captured and injected. Overhead benchmark prints automatically after recording. Replay confidence score prints after replay.

### Step 1 — Compile tracee
```bash
cd ~/EchoRun/final/recorder
gcc tracee.c -o tracee
```

### Step 2 — Record
```bash
./echorun ./tracee
```
When prompted `Enter some text:` — type `hello` and press Enter.

Expected output:
```
[ECHORUN] Measuring native baseline (stdin suppressed)...
Starting tracee...
Enter some text: hello
Tracee read 6 bytes.

[ECHORUN] Recording overhead report:
  Native (no ptrace) : 1.234 ms
  Recorded           : 8.901 ms
  Overhead           : 7.21x
  (Note: native run used /dev/null for stdin — overhead includes all user interaction time)
```

The native baseline run is silent (stdin suppressed). The numbers will vary per machine.

### Step 3 — Inspect the trace
```bash
./dump_trace
```
You should see:
- An event with `Syscall No: 0, Retval: 832` (ELF read by dynamic linker)
- An event with `Syscall No: 0, Retval: 6, Data: hello.` (your input)
- PROC_EVENTs for exec and exit

### Step 4 — Replay
```bash
cd ~/EchoRun/final/replayer
./echoplay ../recorder/trace.bin ../recorder/trace.idx ../recorder/tracee
```

Expected output:
```
[ECHOPLAY] Trace loaded. Events recorded: 5
[ECHOPLAY] Tracee PID=XXXX launched
[REPLAY] Entry: suppressed syscall=0 seq=4
[REPLAY] Exit: injected retval=832
[REPLAY] Entry: suppressed syscall=0 seq=13
[REPLAY] Exit: injected retval=6
Starting tracee...
Enter some text:
Tracee read 6 bytes.
[REPLAY] Tracee exited with code 0
[ECHOPLAY] Replay completed successfully.

[ECHOPLAY] Replay confidence: 100/100  (EXCELLENT — replay is highly deterministic)
  NON_DET syscalls seen    : 2
  Successfully injected    : 2
  Unmatched (let through)  : 0
  Signals replayed         : 0
```

Notice: you did NOT type anything. `hello` came from the trace file. Confidence 100/100 means every NON_DET syscall was matched and injected from the trace.

### Step 5 — Visualize
```bash
cd ~/EchoRun/final/visualizer
./echovis visualise ../recorder/trace.bin ../recorder/trace.idx --tui
./echovis visualise ../recorder/trace.bin ../recorder/trace.idx --output test1_timeline.svg
```

The `--tui` flag prints the event breakdown stats and a terminal timeline. Open `test1_timeline.svg` in any browser — hover over blue ticks to see syscall name, seq_idx, and return value.

---

## TEST 2 — Multiple NON_DET Syscalls (tracee2.c)

**What it proves:** Replayer handles getrandom + multiple file reads + stdin in one run.

### Step 1 — Compile tracee2
```bash
cd ~/EchoRun/final/recorder
gcc tracee2.c -o tracee2
```

### Step 2 — Record
```bash
./echorun ./tracee2
```
When prompted `Enter a word:` — type `harshith` and press Enter.

Expected output:
```
[ECHORUN] Measuring native baseline (stdin suppressed)...
getrandom returned 4 bytes: XX XX XX XX
hostname: your-machine-name
Enter a word: harshith
You typed: harshith
Done.

[ECHORUN] Recording overhead report:
  Native (no ptrace) : X.XXX ms
  Recorded           : X.XXX ms
  Overhead           : X.XXx
```

Note the exact random bytes — they will be identical on replay.

### Step 3 — Replay
```bash
cd ~/EchoRun/final/replayer
./echoplay ../recorder/trace.bin ../recorder/trace.idx ../recorder/tracee2
```

Expected: identical output — same random bytes, same hostname, same typed word. No keyboard input required. Confidence score prints at the end.

### Step 4 — Visualize
```bash
cd ~/EchoRun/final/visualizer
./echovis visualise ../recorder/trace.bin ../recorder/trace.idx --tui
./echovis visualise ../recorder/trace.bin ../recorder/trace.idx --output test2_timeline.svg
```

---

## TEST 3 — File I/O (tracee3.c)

**What it proves:** Replayer handles file read/write chains. During replay no real file I/O happens — data comes from the trace.

### Step 1 — Compile tracee3
```bash
cd ~/EchoRun/final/recorder
gcc tracee3.c -o tracee3
```

### Step 2 — Record
```bash
./echorun ./tracee3
```

Expected output:
```
[ECHORUN] Measuring native baseline (stdin suppressed)...
Read 21 bytes from file: EchoRun file IO test
OS info: NAME="Ubuntu"
File IO test done.

[ECHORUN] Recording overhead report:
  ...
```

### Step 3 — Replay
```bash
cd ~/EchoRun/final/replayer
./echoplay ../recorder/trace.bin ../recorder/trace.idx ../recorder/tracee3
```

Expected: identical output. Even if `/tmp/echorun_test.txt` is deleted between record and replay, the replayer injects the saved bytes from the trace.

### Step 4 — Visualize
```bash
cd ~/EchoRun/final/visualizer
./echovis visualise ../recorder/trace.bin ../recorder/trace.idx --tui
./echovis visualise ../recorder/trace.bin ../recorder/trace.idx --output test3_timeline.svg
```

---

## TEST 4 — Trace Diff Tool

**What it proves:** Diff tool finds the exact seq_idx where two traces diverge, names the syscalls, and auto-renders an SVG with the divergence marker in one command.

### Step 1 — Record tracee twice with different input
```bash
cd ~/EchoRun/final/recorder

./echorun ./tracee        # type: hello
cp trace.bin trace_a.bin && cp trace.idx trace_a.idx

./echorun ./tracee        # type: world
cp trace.bin trace_b.bin && cp trace.idx trace_b.idx
```

### Step 2 — Run diff (text output only)
```bash
cd ~/EchoRun/final/visualizer
./echovis diff ../recorder/trace_a.bin ../recorder/trace_a.idx \
               ../recorder/trace_b.bin ../recorder/trace_b.idx
```

Expected output:
```
[DIFF] First divergence at seq_idx=13
  Expected : read() -> 6
  Got      : read() -> 6
```

The data payloads differ (hello vs world) even though the retval is the same length — caught by FNV-1a hash comparison.

### Step 3 — Diff with auto-SVG (one command does both)
```bash
./echovis diff ../recorder/trace_a.bin ../recorder/trace_a.idx \
               ../recorder/trace_b.bin ../recorder/trace_b.idx \
               --output diff_result.svg
```

Expected output:
```
[DIFF] First divergence at seq_idx=13
  Expected : read() -> 6
  Got      : read() -> 6
[VIS] Loaded N events from trace_a.bin
[VIS] Event breakdown:
      SYSCALL : X
      SIGNAL  : 0
      PROC    : X
      Most frequent syscall: read() xN
[VIS] SVG with divergence marker written to diff_result.svg
```

Open `diff_result.svg` — red dashed line at seq 13 is placed automatically, no manual `--divergence` flags needed.

---

## TEST 5 — Time-Travel REPL

**What it proves:** step/goto/peek/poke commands work. `checkpoints` command lists all saved restore points.

### Step 1 — Enable step mode
Open `~/EchoRun/final/replayer/echoplay.c` and change:
```c
.running = 1,   // free-running
```
to:
```c
.running = 2,   // step mode
```

### Step 2 — Rebuild
```bash
cd ~/EchoRun/final/replayer
make clean && make all
```

### Step 3 — Record then run in step mode
```bash
cd ~/EchoRun/final/recorder
./echorun ./tracee          # type: hello

cd ~/EchoRun/final/replayer
./echoplay ../recorder/trace.bin ../recorder/trace.idx ../recorder/tracee
```

You will get the REPL prompt:
```
[EchoRun REPL] Commands: continue | step | goto <seq> | peek <addr> | poke <addr> <val> | checkpoints | quit
echoplay>
```

### Step 4 — Try these commands
```
echoplay> step
echoplay> step
echoplay> checkpoints
echoplay> goto 4
echoplay> continue
```

The `checkpoints` command lists all auto-saved restore points with their seq_idx and number of memory regions snapshotted. Use `goto <seq>` to jump to any of them.

### Step 5 — Reset back to free-running mode after demo
Change `running = 2` back to `running = 1` and rebuild:
```bash
make clean && make all
```

---

## TEST 6 — Record Any Real Binary (/bin/ls)

**What it proves:** EchoRun is not hardcoded to our test programs — it works on any single-threaded Linux binary.

### Step 1 — Record ls
```bash
cd ~/EchoRun/final/recorder
./echorun /bin/ls
```
ls prints your directory listing and exits. Overhead report prints at the end.

### Step 2 — Replay ls
```bash
cd ~/EchoRun/final/replayer
./echoplay ../recorder/trace.bin ../recorder/trace.idx /bin/ls
```

Expected: same directory listing as during recording — even if files were added or removed between record and replay. Confidence score prints at the end.

### Step 3 — Visualize
```bash
cd ~/EchoRun/final/visualizer
./echovis visualise ../recorder/trace.bin ../recorder/trace.idx --tui
./echovis visualise ../recorder/trace.bin ../recorder/trace.idx --output test6_ls_timeline.svg
```

---

## TEST 7 — Intentional Divergence Detection

**What it proves:** Divergence detection catches mismatches, reports exact position with syscall names, and the confidence score drops accordingly.

### Step 1 — Record tracee
```bash
cd ~/EchoRun/final/recorder
./echorun ./tracee          # type: hello
```

### Step 2 — Replay with WRONG binary (ls instead of tracee)
```bash
cd ~/EchoRun/final/replayer
./echoplay ../recorder/trace.bin ../recorder/trace.idx /bin/ls
```

Expected output:
```
[DIVERGENCE] seq_idx=N  expected: read() [0]  got: openat() [257]
[ECHOPLAY] Replay DIVERGED at seq=N expected=0 actual=257

[ECHOPLAY] Replay confidence: 0/100  (POOR — significant divergence risk (ASLR?))
  NON_DET syscalls seen    : 1
  Successfully injected    : 0
  Unmatched (let through)  : 1
  Signals replayed         : 0
```

Note: divergence message now shows **syscall names** (`read()`, `openat()`) not just numbers.

### Step 3 — Visualize with divergence marker
```bash
cd ~/EchoRun/final/visualizer
./echovis visualise ../recorder/trace.bin ../recorder/trace.idx \
    --output test7_divergence.svg \
    --divergence N 0 257
```
Replace `N` with the seq_idx from step 2. Open `test7_divergence.svg` — red dashed vertical line marks the exact divergence point.

---

## TEST 8 — Execution Summary (echovis summarise)

**What it proves:** EchoRun can generate a human-readable narrative of what a program did from its trace — without re-running it.

### Step 1 — Use any existing trace (e.g. tracee2 from TEST 2)
```bash
cd ~/EchoRun/final/recorder
./echorun ./tracee2         # type: harshith
```

### Step 2 — Run summarise
```bash
cd ~/EchoRun/final/visualizer
./echovis summarise ../recorder/trace.bin ../recorder/trace.idx
```

Expected output:
```
[SUMMARY] Program execution narrative (N events)
──────────────────────────────────────────────────────
  1. [Startup] Loaded dynamic libraries and mapped binary
     (X mmap, X fstat/stat calls — typical ELF loader activity)
  2. [Entropy] Called getrandom() 1 time(s) — used hardware RNG
  3. [File I/O] Opened 2 file(s), read 76 bytes total across 2 read() call(s)
  4. [Input] Read from stdin (interactive or pipe input)
  5. [Output] Wrote 89 bytes to stdout/stderr across 4 write() call(s)
  6. [Exit] Exited cleanly with code 0
──────────────────────────────────────────────────────
```

Try it on the `/bin/ls` trace too — the startup phase will show significantly more mmap calls from library loading.

---

## Summary Table

| Test | Program | Feature Tested |
|------|---------|----------------|
| TEST 1 | tracee.c | Record + replay + overhead benchmark + confidence score |
| TEST 2 | tracee2.c | getrandom + multiple file reads + confidence score |
| TEST 3 | tracee3.c | File I/O chain |
| TEST 4 | tracee.c x2 | Trace diff + auto-SVG with `--output` |
| TEST 5 | tracee.c | Time-travel REPL (step / goto / peek / checkpoints) |
| TEST 6 | /bin/ls | Any real Linux binary |
| TEST 7 | tracee.c + /bin/ls | Divergence detection with syscall names + confidence drop |
| TEST 8 | tracee2.c | Execution summary narrative (`echovis summarise`) |