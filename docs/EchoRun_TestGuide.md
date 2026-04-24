# EchoRun — Complete Test Guide

All commands assume you are inside `~/EchoRun/final/`.

---

## Prerequisites

You need a Linux machine (Ubuntu 20.04+ recommended) with GCC and Make installed.

```bash
sudo apt update && sudo apt install gcc make
```

---

## Step 0 — Fix the Recorder Makefile

The recorder's `Makefile` shipped with incorrect targets. Replace it before building.

Open `~/EchoRun/final/recorder/Makefile` and replace the entire contents with:

```makefile
CC     = gcc
CFLAGS = -Wall -Wextra -g

all: echorun dump_trace tracee tracee2 tracee3

echorun: echorun.c syscall_loop.c ND_syscall_handler.c echotrace_bin.c \
         sys_cat_info.c trace_reader.c
	$(CC) $(CFLAGS) -o $@ $^

dump_trace: dump_trace.c trace_reader.c echotrace_bin.c
	$(CC) $(CFLAGS) -o $@ $^

tracee: tracee.c
	$(CC) $(CFLAGS) -o $@ $^

tracee2: tracee2.c
	$(CC) $(CFLAGS) -o $@ $^

tracee3: tracee3.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f echorun dump_trace tracee tracee2 tracee3

.PHONY: all clean
```

---

## Step 1 — Build All Three Modules

```bash
cd ~/EchoRun/final/recorder   && make clean && make all
cd ~/EchoRun/final/replayer   && make clean && make all
cd ~/EchoRun/final/visualizer && make clean && make all
```

Expected: zero errors, zero warnings for all three.

You will now have: `echorun`, `dump_trace`, `tracee`, `tracee2`, `tracee3` in recorder/,
`echoplay` in replayer/, and `echovis` in visualizer/.

---

## A Note on ASLR and Confidence Scores

Linux randomises memory addresses by default (ASLR). This causes `mmap` and `brk`
calls — which are classified NON_DET — to return different addresses between the
record run and the replay run. The replayer correctly flags these as unmatched,
which lowers the confidence score.

To get a **100/100 confidence score**, disable ASLR for both echorun and echoplay
using `setarch -R`:

```bash
setarch -R ./echorun ./tracee       # recording
setarch -R ./echoplay ...           # replaying
```

All tests below show both the standard command and the ASLR-disabled variant.
The replay itself works correctly either way — only the confidence score differs.

---

## TEST 1 — Basic Record + Replay (tracee)

**What it proves:** Core record/replay pipeline works. stdin read is captured and
injected. Replay confidence score prints after replay.

### Step 1 — Record

```bash
cd ~/EchoRun/final/recorder
setarch -R ./echorun ./tracee
```

When prompted `Enter some text:` — type `hello` and press Enter.

Expected output:

```
[ECHORUN] Measuring native baseline (stdin suppressed)...
Starting tracee...
Enter some text: hello
Tracee read 6 bytes.

[ECHORUN] Recording overhead report:
  Native (no ptrace) : X.XXX ms
  Recorded           : X.XXX ms
  Overhead           : X.XXx
  (Note: native run used /dev/null for stdin — overhead includes all user interaction time)
```

### Step 2 — Inspect the trace

```bash
./dump_trace
```

Expected output (exact seq_idx values may vary):

```
Trace loaded successfully.
Total Events Recorded: 5
--------------------------------------------------
Seq Idx: 0  | Event Type: 3 | Syscall No: 4  | Retval: 0   | Data Len: 0
Seq Idx: 4  | Event Type: 1 | Syscall No: 0  | Retval: 832 | Data Len: 832
   -> Intercepted Data: .ELF...
Seq Idx: 13 | Event Type: 1 | Syscall No: 0  | Retval: 6   | Data Len: 6
   -> Intercepted Data: hello.
Seq Idx: 14 | Event Type: 3 | Syscall No: 6  | Retval: 0   | Data Len: 0
Seq Idx: 15 | Event Type: 3 | Syscall No: 0  | Retval: 0   | Data Len: 0
```

Reading this:
- Event Type 1 = SYSCALL\_EVENT (recorded non-deterministic syscall)
- Event Type 3 = PROC\_EVENT (exec / exit)
- Syscall 0 = `read` — the `.ELF` read is the dynamic linker; the second is your input

### Step 3 — Replay

```bash
cd ~/EchoRun/final/replayer
setarch -R ./echoplay ../recorder/trace.bin ../recorder/trace.idx ../recorder/tracee
```

Expected output:

```
[ECHOPLAY] Trace loaded. Events recorded: 5
[ECHOPLAY] Tracee PID=XXXX launched
[REPLAY] Entry: suppressed syscall=0 seq=4
[REPLAY] Exit: injected retval=832
Starting tracee...
[REPLAY] Entry: suppressed syscall=0 seq=13
[REPLAY] Exit: injected retval=6
Enter some text: 
Tracee read 6 bytes.
[REPLAY] Tracee exited with code 0

[ECHOPLAY] Replay confidence: 100/100  (EXCELLENT — replay is highly deterministic)
  NON_DET syscalls seen    : 2
  Successfully injected    : 2
  Unmatched (let through)  : 0
  Signals replayed         : 0
[ECHOPLAY] Replay completed successfully.
```

Notice: you did **not** type anything. `hello` was injected from the trace file.

> **Without `setarch -R`:** The replay still works and output is identical, but
> the confidence score will be lower (e.g. 0/100) because ELF loader `mmap`/`brk`
> calls are counted as unmatched. This is expected — not a bug.

### Step 4 — Visualize

```bash
cd ~/EchoRun/final/visualizer
./echovis visualise ../recorder/trace.bin ../recorder/trace.idx --tui
./echovis visualise ../recorder/trace.bin ../recorder/trace.idx --output test1_timeline.svg
```

The `--tui` flag prints a terminal block-char timeline with event breakdown stats.
Open `test1_timeline.svg` in any browser — hover over ticks to see syscall name,
seq\_idx, and return value.

---

## TEST 2 — Multiple NON_DET Syscalls (tracee2)

**What it proves:** Replayer handles `getrandom` + multiple file reads + stdin in one run.

### Step 1 — Record

```bash
cd ~/EchoRun/final/recorder
setarch -R ./echorun ./tracee2
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
  ...
```

Note the exact random bytes — they will be identical on replay.

### Step 2 — Replay

```bash
cd ~/EchoRun/final/replayer
setarch -R ./echoplay ../recorder/trace.bin ../recorder/trace.idx ../recorder/tracee2
```

Expected: identical output — same random bytes, same hostname, same word. No
keyboard input required. Confidence score prints at the end.

### Step 3 — Visualize

```bash
cd ~/EchoRun/final/visualizer
./echovis visualise ../recorder/trace.bin ../recorder/trace.idx --tui
./echovis visualise ../recorder/trace.bin ../recorder/trace.idx --output test2_timeline.svg
```

---

## TEST 3 — File I/O (tracee3)

**What it proves:** Replayer handles file read/write chains. During replay no real
file I/O happens — data comes entirely from the trace.

### Step 1 — Record

```bash
cd ~/EchoRun/final/recorder
setarch -R ./echorun ./tracee3
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

### Step 2 — Replay

```bash
cd ~/EchoRun/final/replayer
setarch -R ./echoplay ../recorder/trace.bin ../recorder/trace.idx ../recorder/tracee3
```

Expected: identical output. Even if `/tmp/echorun_test.txt` is deleted between
record and replay, the replayer injects the saved bytes from the trace file.

### Step 3 — Visualize

```bash
cd ~/EchoRun/final/visualizer
./echovis visualise ../recorder/trace.bin ../recorder/trace.idx --tui
./echovis visualise ../recorder/trace.bin ../recorder/trace.idx --output test3_timeline.svg
```

---

## TEST 4 — Trace Diff Tool

**What it proves:** The diff tool finds the exact seq\_idx where two traces diverge,
names the syscalls, and auto-renders an SVG with a divergence marker in one command.

### Step 1 — Record tracee twice with different input

```bash
cd ~/EchoRun/final/recorder

setarch -R ./echorun ./tracee        # type: hello
cp trace.bin trace_a.bin && cp trace.idx trace_a.idx

setarch -R ./echorun ./tracee        # type: world
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

The return values are the same length but the payloads differ (`hello` vs `world`)
— caught by the FNV-1a hash comparison on the recorded buffers.

### Step 3 — Diff with auto-rendered SVG

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

Open `diff_result.svg` — a red dashed vertical line marks the divergence point
automatically. No manual `--divergence` flags needed.

---

## TEST 5 — Time-Travel REPL

**What it proves:** `step` / `goto` / `peek` / `poke` / `checkpoints` commands work.

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

### Step 3 — Record then replay in step mode

```bash
cd ~/EchoRun/final/recorder
setarch -R ./echorun ./tracee          # type: hello

cd ~/EchoRun/final/replayer
setarch -R ./echoplay ../recorder/trace.bin ../recorder/trace.idx ../recorder/tracee
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

`checkpoints` lists all auto-saved restore points with their seq\_idx and number of
memory regions snapshotted. `goto <seq>` jumps to the nearest checkpoint before
that position and seeks the trace cursor there.

### Step 5 — Reset to free-running mode

Change `running = 2` back to `running = 1` and rebuild:

```bash
make clean && make all
```

---

## TEST 6 — Record Any Real Binary (/bin/ls)

**What it proves:** EchoRun works on any single-threaded Linux binary, not just
the test programs.

### Step 1 — Record

```bash
cd ~/EchoRun/final/recorder
setarch -R ./echorun /bin/ls
```

`ls` prints the directory listing and exits. The overhead report prints after.

### Step 2 — Replay

```bash
cd ~/EchoRun/final/replayer
setarch -R ./echoplay ../recorder/trace.bin ../recorder/trace.idx /bin/ls
```

Expected: same directory listing as during recording — even if files were added
or removed between record and replay. Confidence score prints at the end.

### Step 3 — Visualize

```bash
cd ~/EchoRun/final/visualizer
./echovis visualise ../recorder/trace.bin ../recorder/trace.idx --tui
./echovis visualise ../recorder/trace.bin ../recorder/trace.idx --output test6_ls_timeline.svg
```

The startup phase will show significantly more `mmap` calls than the tracee tests
due to library loading by `ls`.

---

## TEST 7 — Intentional Divergence Detection

**What it proves:** Divergence detection catches mismatches, reports the exact
position with syscall names, and the confidence score drops to reflect the failure.

### Step 1 — Record tracee

```bash
cd ~/EchoRun/final/recorder
setarch -R ./echorun ./tracee          # type: hello
```

### Step 2 — Replay with the wrong binary

```bash
cd ~/EchoRun/final/replayer
setarch -R ./echoplay ../recorder/trace.bin ../recorder/trace.idx /bin/ls
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

The divergence message shows syscall names (`read()`, `openat()`) not just numbers.

### Step 3 — Visualize with divergence marker

Replace `N` below with the actual seq\_idx printed in step 2:

```bash
cd ~/EchoRun/final/visualizer
./echovis visualise ../recorder/trace.bin ../recorder/trace.idx \
    --output test7_divergence.svg \
    --divergence N 0 257
```

Open `test7_divergence.svg` — a red dashed vertical line marks the exact
divergence point on the timeline.

---

## TEST 8 — Execution Summary

**What it proves:** EchoRun can generate a human-readable narrative of what a
program did from its trace file alone — without re-running the program.

### Step 1 — Record tracee2

```bash
cd ~/EchoRun/final/recorder
setarch -R ./echorun ./tracee2         # type: harshith
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

Try it on the `/bin/ls` trace too — the startup phase will show many more `mmap`
calls from library loading.

---

## Full Pipeline at a Glance

```
Your program (e.g. tracee)
        |
        | setarch -R ./echorun ./tracee
        v
[RECORDER — echorun]
  forks tracee under ptrace
  intercepts every NON_DET syscall
  copies return values + buffer contents out of tracee memory
  writes trace.bin + trace.idx
        |
        |-----> ./dump_trace                  (inspect raw events)
        |-----> ./echovis visualise --tui     (terminal timeline)
        |-----> ./echovis visualise --output  (SVG timeline)
        |-----> ./echovis diff                (compare two traces)
        |-----> ./echovis summarise           (execution narrative)
        |
        | setarch -R ./echoplay trace.bin trace.idx ./tracee
        v
[REPLAYER — echoplay]
  forks tracee fresh under ptrace
  sets orig_rax = -1  (suppress real syscall)
  injects saved retval into rax
  injects saved buffer bytes into tracee memory
  detects divergence if syscall does not match trace
  REPL: step / goto / peek / poke / checkpoints
        |
        v
  identical output as original run
  confidence score 0–100 printed at exit
```

---

## Summary Table

| Test | Program        | Feature Tested                                          |
|------|----------------|---------------------------------------------------------|
| 1    | tracee         | Record + replay + overhead benchmark + confidence score |
| 2    | tracee2        | getrandom + multiple file reads + confidence score      |
| 3    | tracee3        | File I/O chain (write then read)                        |
| 4    | tracee × 2     | Trace diff + auto-SVG with `--output`                   |
| 5    | tracee         | Time-travel REPL (step / goto / peek / checkpoints)     |
| 6    | /bin/ls        | Any real Linux binary                                   |
| 7    | tracee + /bin/ls | Divergence detection with syscall names               |
| 8    | tracee2        | Execution summary narrative (`echovis summarise`)       |