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

**What it proves:** Core record/replay pipeline works. stdin read is captured and injected.

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
Starting tracee...
Enter some text: hello
Tracee read 6 bytes.
```

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
```

Notice: you did NOT type anything. The `hello` came from the trace file.

### Step 5 — Visualize
```bash
cd ~/EchoRun/final/visualizer
./echovis visualise ../recorder/trace.bin ../recorder/trace.idx --tui
./echovis visualise ../recorder/trace.bin ../recorder/trace.idx --output test1_timeline.svg
```

Open `test1_timeline.svg` in browser. Hover over blue ticks to see syscall name + retval.

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
getrandom returned 4 bytes: XX XX XX XX
hostname: Harshith-TUF
Enter a word: harshith
You typed: harshith
Done.
```
Note the exact random bytes printed — they will be the same on replay.

### Step 3 — Replay
```bash
cd ~/EchoRun/final/replayer
./echoplay ../recorder/trace.bin ../recorder/trace.idx ../recorder/tracee2
```

Expected: same output as recording — same random bytes, same hostname, same word. No keyboard input needed.

### Step 4 — Visualize
```bash
cd ~/EchoRun/final/visualizer
./echovis visualise ../recorder/trace.bin ../recorder/trace.idx --tui
./echovis visualise ../recorder/trace.bin ../recorder/trace.idx --output test2_timeline.svg
```

---

## TEST 3 — File I/O (tracee3.c)

**What it proves:** Replayer handles file read/write chains. During replay no real file I/O happens — data comes from trace.

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
Read 21 bytes from file: EchoRun file IO test
OS info: NAME="Ubuntu"
File IO test done.
```

### Step 3 — Replay
```bash
cd ~/EchoRun/final/replayer
./echoplay ../recorder/trace.bin ../recorder/trace.idx ../recorder/tracee3
```

Expected: identical output. Even if `/tmp/echorun_test.txt` is deleted between record and replay, the replayer injects the saved data.

### Step 4 — Visualize
```bash
cd ~/EchoRun/final/visualizer
./echovis visualise ../recorder/trace.bin ../recorder/trace.idx --tui
./echovis visualise ../recorder/trace.bin ../recorder/trace.idx --output test3_timeline.svg
```

---

## TEST 4 — Trace Diff Tool

**What it proves:** Diff tool finds the exact seq_idx where two traces diverge.

### Step 1 — Record tracee twice with different input
```bash
cd ~/EchoRun/final/recorder

./echorun ./tracee        # type: hello
cp trace.bin trace_a.bin && cp trace.idx trace_a.idx

./echorun ./tracee        # type: world   (different input)
cp trace.bin trace_b.bin && cp trace.idx trace_b.idx
```

### Step 2 — Run diff
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
The data payloads differ (hello vs world) even though retval is the same — caught by FNV-1a hash comparison.

---

## TEST 5 — Time-Travel REPL

**What it proves:** Step/goto/peek/poke commands work for interactive debugging.

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

### Step 3 — Make sure trace_a from TEST 4 exists, then run
```bash
cd ~/EchoRun/final/recorder
./echorun ./tracee          # type: hello

cd ~/EchoRun/final/replayer
./echoplay ../recorder/trace.bin ../recorder/trace.idx ../recorder/tracee
```

You will get the REPL prompt:
```
[EchoRun REPL] Commands: continue | step | goto <seq> | peek <addr> | poke <addr> <val> | quit
echoplay>
```

### Step 4 — Try these commands
```
echoplay> step
echoplay> step
echoplay> goto 4
echoplay> continue
```

### Step 5 — Reset back to free-running mode after demo
Change `running = 2` back to `running = 1` and `make clean && make all`.

---

## TEST 6 — Record Any Real Binary (/bin/ls)

**What it proves:** EchoRun is not hardcoded to our test programs — it works on any Linux binary.

### Step 1 — Record ls
```bash
cd ~/EchoRun/final/recorder
./echorun /bin/ls
```
ls will print your directory listing and exit. Trace is saved.

### Step 2 — Replay ls
```bash
cd ~/EchoRun/final/replayer
./echoplay ../recorder/trace.bin ../recorder/trace.idx /bin/ls
```

Expected: same directory listing as during recording — even if files were added/removed between record and replay.

### Step 3 — Visualize
```bash
cd ~/EchoRun/final/visualizer
./echovis visualise ../recorder/trace.bin ../recorder/trace.idx --tui
./echovis visualise ../recorder/trace.bin ../recorder/trace.idx --output test6_ls_timeline.svg
```

---

## TEST 7 — Intentional Divergence Detection

**What it proves:** Divergence detection catches mismatches and reports exact position. SVG shows red marker.

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
[DIVERGENCE] seq_idx=N expected_syscall=X actual_syscall=Y
[ECHOPLAY] Replay DIVERGED at seq=N expected=X actual=Y
```

### Step 3 — Visualize with divergence marker
Use the seq/expected/actual values from the output above:
```bash
cd ~/EchoRun/final/visualizer
./echovis visualise ../recorder/trace.bin ../recorder/trace.idx \
    --output test7_divergence.svg \
    --divergence N X Y
```
Replace N, X, Y with actual values from step 2.

Open `test7_divergence.svg` in browser — red dashed vertical line marks exact divergence point.

---

## Summary Table

| Test | Program | Feature Tested |
|------|---------|----------------|
| TEST 1 | tracee.c | Basic record + replay + visualize |
| TEST 2 | tracee2.c | getrandom + multiple file reads |
| TEST 3 | tracee3.c | File I/O chain |
| TEST 4 | tracee.c x2 | Trace diff tool |
| TEST 5 | tracee.c | Time-travel REPL (step/goto/peek) |
| TEST 6 | /bin/ls | Any real Linux binary |
| TEST 7 | tracee.c + /bin/ls | Divergence detection + SVG overlay |
