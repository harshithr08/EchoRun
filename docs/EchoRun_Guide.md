# EchoRun — Project Guide

---

## Part 1 — Mozilla rr vs EchoRun: What They Have, What We Took

### What Mozilla rr Is

rr is a production-grade Linux debugger built by Mozilla engineers. It lets you record any program running on a real machine, then replay that recording perfectly — down to the exact instruction — as many times as you want. It integrates with GDB so you can step backward through time, set reverse breakpoints, and inspect state at any moment in the past.

It has been battle-tested on Firefox (millions of lines of C++) and is used daily by professional engineers to debug crashes that only happen once.

---

### What rr Has (Full Feature List)

**Core Engine**
- ptrace-based syscall interception for all Linux syscalls
- Hardware performance counter (Intel PMU) integration via `perf_event_open` — records exact instruction count at every event, not just syscall position
- Deterministic multi-threaded replay — this is rr's crown jewel. It uses the PMU branch counter to serialize thread scheduling so two threads always interleave in exactly the same order on replay
- ASLR disabling via `personality(ADDR_NO_RANDOMIZE)` so memory addresses are identical between record and replay
- Near-zero overhead recording (~1.2x slowdown claimed in their paper)

**Trace Format**
- Compact binary trace with per-event instruction counts
- Supports traces gigabytes in size
- Handles `fork`, `exec`, `clone` — full process tree recording

**Replay**
- Suppresses all non-deterministic syscalls and injects saved results
- Injects signals at instruction-precise positions using PMU overflow interrupts
- Full memory injection via `PTRACE_POKEDATA`
- Handles memory-mapped files, shared memory, and pipes across processes

**Time-Travel Debugging**
- `continue` / `step` / `reverse-continue` / `reverse-step` — all four directions
- `checkpoint` and `restart` — restore any saved point instantly
- `goto` by event count or instruction count
- `watch` — reverse watchpoints (find the last write to an address)

**GDB Integration**
- rr exposes a GDB stub — you attach GDB to `rr replay` and get full GDB with time-travel
- All GDB commands work: breakpoints, watchpoints, `print`, `backtrace`, memory inspection

**IPC and Multi-Process**
- Records multiple cooperating processes (`rr record --pid A,B`)
- Replays causal ordering across processes connected by pipes or sockets
- Handles shared memory between processes

**Signal Handling**
- Instruction-precise signal delivery using PMU counters
- Records signal mask, signal handler addresses, and delivery timing

---

### What EchoRun Has (Our Project)

**Core Engine**
- ptrace-based syscall interception ✅
- Syscall classification table (NON_DET / DET / SIDE_EFFECT) ✅
- Binary trace format (.echotrace + .idx index file) ✅
- ASLR: not explicitly disabled (not implemented) ❌
- Multi-threaded replay: explicitly out of scope ❌
- PMU integration: scaffolded in echotrace_bin.c but always returns 0 (partial) ⚠️
- Near-zero overhead: not claimed, not measured ❌

**Trace Format**
- Compact binary with header + per-event records ✅
- Index file for random access by seq_idx ✅
- Handles exit, fork, exec as PROC_EVENTs ✅
- Payload capture for read/recv/getrandom ✅
- Circular buffer / flight-recorder mode ✅
- Multi-process / cross-process traces ❌

**Replay**
- Syscall suppression via orig_rax = -1 ✅
- Return value injection via PTRACE_SETREGS ✅
- Memory injection via PTRACE_POKEDATA ✅
- Signal replay at sequence position ✅ (approximate, not instruction-precise)
- Divergence detection + divergence_report_t ✅
- ASLR not disabled so addresses may differ between runs ⚠️

**Time-Travel**
- `continue` ✅
- `step` ✅
- `goto <seq_idx>` ✅
- `peek` / `poke` ✅
- `reverse-continue` / `reverse-step`: stretch goal, not implemented ❌
- Checkpoints: implemented (register + writable memory snapshot) ✅
- Auto-checkpointing every 500 events ✅

**GDB Integration**
- None ❌

**IPC and Multi-Process**
- Pipe-only IPC: stretch goal, not implemented ❌
- Shared memory / sockets: explicitly out of scope ❌

**Visualization (our addition — rr does not have this)**
- SVG timeline renderer ✅
- TUI block-char terminal renderer ✅
- Trace diff tool (compare two traces, find first divergence) ✅

---

### Summary Table

| Feature | Mozilla rr | EchoRun |
|---|---|---|
| Syscall interception (single-threaded) | ✅ | ✅ |
| Binary trace format | ✅ | ✅ |
| Return value + memory injection | ✅ | ✅ |
| Divergence detection | ✅ | ✅ |
| continue / step / goto | ✅ | ✅ |
| reverse-continue / reverse-step | ✅ | ❌ stretch |
| GDB integration | ✅ | ❌ |
| Multi-threaded determinism (PMU) | ✅ | ❌ out of scope |
| Instruction-precise signals (PMU) | ✅ | ❌ approximate |
| Multi-process / IPC | ✅ | ❌ out of scope |
| ASLR disabling | ✅ | ❌ |
| SVG / TUI timeline visualizer | ❌ | ✅ |
| Trace diff tool | ❌ | ✅ |

**One-line summary:** We took the core idea of rr — ptrace + syscall interception + deterministic replay — and implemented it for single-threaded programs. We skipped the hard parts (multi-threading, PMU counters, GDB integration) and added our own visualization that rr doesn't have.

---

## Part 2 — How to Run the Full Pipeline (Step by Step)

### Prerequisites

You need a Linux machine (Ubuntu 20.04+ recommended). Install build tools:

```bash
sudo apt update
sudo apt install gcc make git
```

---

### Step 0 — Setup: Get All Files in Place

Download `EchoRun_complete.zip` and extract it:

```bash
unzip EchoRun_complete.zip
```

You will have three folders:

```
final/
├── recorder/       ← Harsha's module
├── replayer/       ← Your module
└── visualizer/     ← Prashanth's module
```

Build all three:

```bash
cd final/recorder   && make all
cd ../replayer      && make all
cd ../visualizer    && make all
```

You will now have binaries: `echorun`, `echoplay`, `echovis`, `tracee`, `dump_trace`.

---

### Step 1 — Record a Program

`tracee` is a simple test program that asks you to type something and reads it. This is our test subject.

```bash
cd final/recorder
./echorun ./tracee
```

The terminal will show:

```
Starting tracee...
Enter some text: 
```

Type something (e.g. `hello`) and press Enter. The program prints how many bytes it read and exits.

What just happened:
- `echorun` forked `tracee` as a child process
- It attached to it with ptrace
- Every time `tracee` made a syscall, ptrace paused it
- For non-deterministic syscalls (like `read`), it copied the data out of tracee's memory and saved it
- When tracee exited, everything was flushed to disk

You now have two new files:

```
trace.bin   ← the binary trace (events + payloads)
trace.idx   ← the index file (seq_idx → byte offset for fast seeking)
```

---

### Step 2 — Inspect the Trace

Before replaying, verify the trace looks correct:

```bash
./dump_trace
```

Expected output (approximately):

```
Trace loaded successfully.
Total Events Recorded: 8
--------------------------------------------------
Seq Idx: 0  | Event Type: 3 | Syscall No: 4  | Retval: 0  | Data Len: 0
Seq Idx: 3  | Event Type: 1 | Syscall No: 5  | Retval: 0  | Data Len: 0
Seq Idx: 5  | Event Type: 1 | Syscall No: 0  | Retval: 832 | Data Len: 832
   -> Intercepted Data: .ELF...
Seq Idx: 16 | Event Type: 1 | Syscall No: 0  | Retval: 6  | Data Len: 6
   -> Intercepted Data: hello.
Seq Idx: 17 | Event Type: 3 | Syscall No: 6  | Retval: 0  | Data Len: 0
Seq Idx: 18 | Event Type: 3 | Syscall No: 0  | Retval: 0  | Data Len: 0
```

Reading this:
- Event Type 1 = SYSCALL_EVENT (a recorded non-deterministic syscall)
- Event Type 3 = PROC_EVENT (fork / exec / exit)
- Syscall 0 = `read`, Syscall 5 = `fstat`
- The `read` at Seq 16 captured your 6 bytes (`hello\n`)

---

### Step 3 — Replay

Now run the same `tracee` binary again, but this time driven by the trace instead of live input:

```bash
cd ../replayer
./echoplay ../recorder/trace.bin ../recorder/trace.idx ../recorder/tracee
```

**The terminal will not pause and ask for input.** Instead it prints immediately:

```
[ECHOPLAY] Trace loaded. Events recorded: 8
[ECHOPLAY] Tracee PID=XXXX launched
[REPLAY] Entry: syscall=0 seq=16
[REPLAY] Exit: injected retval=6
[ECHOPLAY] Replay completed successfully.
```

And `tracee`'s stdout will show the same output as the original run.

What just happened:
- `echoplay` forked `tracee` fresh, with ptrace attached
- Every time `tracee` hit a syscall, echoplay suppressed it (set `orig_rax = -1` so the kernel skipped it)
- At the exit stop it injected the saved return value into `rax`
- For `read`, it also wrote the saved bytes (`hello\n`) directly into `tracee`'s memory buffer
- `tracee` never talked to the keyboard — it got the same data as before, from the trace file

---

### Step 4 — Use the Time-Travel REPL

To pause at each event and inspect state, open `echoplay.c` and change line:

```c
.running = 1,   // free-running
```
to:
```c
.running = 2,   // step mode — pauses at every event
```

Rebuild: `make all`

Then run:

```bash
./echoplay ../recorder/trace.bin ../recorder/trace.idx ../recorder/tracee
```

You will get an interactive prompt:

```
[EchoRun REPL] Commands: continue | step | goto <seq> | peek <addr> | poke <addr> <val> | quit
echoplay>
```

Commands:
- `step` — advance one recorded event, pause again
- `continue` — run to the end without pausing
- `goto 16` — jump directly to seq_idx 16 (the read syscall)
- `peek 0x7ffe12345678` — read 8 bytes from that address in the tracee's memory
- `poke 0x7ffe12345678 42` — write value 42 into that address
- `quit` — exit

---

### Step 5 — Visualize the Trace

#### SVG Timeline

```bash
cd ../visualizer
./echovis visualise ../recorder/trace.bin ../recorder/trace.idx --output timeline.svg
```

Open `timeline.svg` in any browser. You will see:
- Blue ticks on the SYSCALL lane for every recorded syscall
- Green ticks on the PROC lane for exec/exit events
- Orange ticks on the SIGNAL lane if any signals were recorded
- Hover over any tick to see the syscall name, seq_idx, and return value

#### TUI Terminal View

```bash
./echovis visualise ../recorder/trace.bin ../recorder/trace.idx --tui
```

Prints directly to terminal using block characters and ANSI colors. Fits to your terminal width automatically.

#### With a Divergence Marker

If a replay diverged at seq_idx 16, expected syscall 0, got syscall 1:

```bash
./echovis visualise ../recorder/trace.bin ../recorder/trace.idx \
    --output timeline.svg \
    --divergence 16 0 1
```

A red dashed vertical line appears at seq 16.

---

### Step 6 — Diff Two Traces

Record the same program twice (type different input the second time):

```bash
cd recorder
./echorun ./tracee          # type "hello", saves trace.bin / trace.idx
cp trace.bin trace_a.bin && cp trace.idx trace_a.idx

./echorun ./tracee          # type "world", saves new trace.bin / trace.idx
cp trace.bin trace_b.bin && cp trace.idx trace_b.idx

cd ../visualizer
./echovis diff ../recorder/trace_a.bin ../recorder/trace_a.idx \
               ../recorder/trace_b.bin ../recorder/trace_b.idx
```

Output:

```
[DIFF] First divergence at seq_idx=16
  Expected : read() -> 6
  Got      : read() -> 6
```

If the data payloads differ (different bytes typed), the FNV-1a hash comparison will catch it even if the return value is the same length.

---

### The Full Pipeline at a Glance

```
Your program (tracee)
        |
        | ./echorun ./tracee
        v
[RECORDER — echorun]
  forks tracee
  ptrace intercepts every syscall
  NON_DET syscalls → copies data out of tracee memory
  writes to trace.bin + trace.idx
        |
        v
  trace.bin + trace.idx
        |
        |-----> ./dump_trace         (inspect raw events)
        |-----> ./echovis visualise  (SVG or TUI timeline)
        |-----> ./echovis diff       (compare two traces)
        |
        | ./echoplay trace.bin trace.idx ./tracee
        v
[REPLAYER — echoplay]
  forks tracee fresh
  ptrace intercepts every syscall
  sets orig_rax = -1  (suppress real syscall)
  injects saved retval into rax
  injects saved bytes into tracee memory
  detects divergence if syscall doesn't match trace
  REPL: step / goto / peek / poke
        |
        v
  identical output as original run
  divergence_report_t → fed to visualizer if replay diverged
```
