# EchoRun

A **ptrace-based record-replay debugger** for Linux, written in C. Inspired by [Mozilla rr](https://rr-project.org/).

EchoRun records a program's execution by intercepting every non-deterministic syscall and capturing its return values and memory payloads. It then replays that execution deterministically — suppressing live kernel calls and injecting the saved data — producing identical observable behaviour every time.

---

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│  echorun  (recorder)                                    │
│  └─ ptrace(PTRACE_SYSCALL) loop                         │
│     ├─ classify: NON_DET / DET / SIDE_EFFECT            │
│     ├─ capture return value + memory payload at exit    │
│     └─ write → trace.bin + trace.idx                   │
└────────────────────────┬────────────────────────────────┘
                         │ trace files
┌────────────────────────▼────────────────────────────────┐
│  echoplay  (replayer)                                   │
│  └─ ptrace loop                                         │
│     ├─ suppress NON_DET calls (orig_rax = −1)           │
│     ├─ inject saved retval via PTRACE_SETREGS           │
│     ├─ restore memory payloads via PTRACE_POKEDATA      │
│     └─ REPL: continue / step / goto / peek / poke      │
└────────────────────────┬────────────────────────────────┘
                         │ trace files
┌────────────────────────▼────────────────────────────────┐
│  echovis  (visualizer)                                  │
│  ├─ visualise → SVG timeline (colour-coded lanes)       │
│  ├─ diff     → SVG with divergence marker overlay       │
│  └─ summarise → human-readable execution narrative      │
└─────────────────────────────────────────────────────────┘
```

---

## Features

**Recorder (`echorun`)**
- Attaches to a child process via `ptrace(PTRACE_TRACEME)` and intercepts all syscalls at their exit
- Classifies 30+ Linux syscalls into `NON_DET` (non-deterministic), `DET` (deterministic), and `SIDE_EFFECT` categories
- Captures return values and memory-backed payloads (e.g. buffers written by `read`, `recvfrom`, `uname`) and serialises them into a compact binary trace format (`trace.bin` + `trace.idx`)
- Reports ptrace overhead vs native runtime after recording

**Replayer (`echoplay`)**
- Replays a recorded execution deterministically by suppressing live kernel calls (`orig_rax = −1`) for `NON_DET` syscalls and injecting the saved return value via `PTRACE_SETREGS`
- Restores memory payloads via `PTRACE_POKEDATA`, byte by byte
- Saves process checkpoints (CPU registers + all writable memory segments from `/proc/pid/maps`) during replay
- Exposes a time-travel REPL with:
  - `continue` / `step` — resume or advance one event
  - `goto <seq>` — jump to any recorded syscall by sequence index (restores nearest checkpoint, then replays forward)
  - `peek <addr>` — read a memory word from the tracee
  - `poke <addr> <val>` — write a memory word into the tracee
  - `checkpoints` — list all saved checkpoints
- Per-event divergence detection: reports exact `seq_idx` and syscall name on mismatch; FNV-1a payload hashing catches buffer-level differences across trace pairs

**Visualizer (`echovis`)**
- `visualise` — renders an SVG timeline with colour-coded lanes per event type (syscall / signal / process)
- `diff` — compares two trace files and renders an SVG with a divergence marker at the first mismatch
- `summarise` — reconstructs a human-readable execution narrative from a trace file without re-running the program
- TUI mode (`--tui`) — block-character renderer for terminals

---

## Repository Structure

```
EchoRun/
├── recorder/
│   ├── echorun.c          # Entry point — forks tracee, runs syscall loop, reports overhead
│   ├── syscall_loop.c     # Core ptrace intercept loop
│   ├── ND_syscall_handler.c  # Payload capture for NON_DET syscalls
│   ├── echotrace_bin.c    # Binary trace serialisation (trace.bin + trace.idx)
│   ├── sys_cat_info.c     # Syscall classification table
│   ├── trace_reader.c     # Trace deserialisation (shared)
│   └── Makefile
│
├── replayer/
│   ├── echoplay.c         # Entry point — opens trace, launches replay loop, starts REPL
│   ├── replay_loop.c      # Core replay ptrace loop
│   ├── syscall_inject.c   # Suppresses kernel calls and injects saved retvals
│   ├── signal_inject.c    # Signal replay
│   ├── checkpoint.c       # Process state save/restore
│   ├── time_travel.c      # goto <seq> implementation
│   ├── divergence.c       # Per-event divergence detection + FNV-1a hashing
│   ├── repl.c             # Interactive REPL (continue/step/goto/peek/poke)
│   ├── replay_cursor.c    # Stateful cursor over trace events
│   └── Makefile
│
├── visualizer/
│   ├── echovis.c          # Entry point — visualise / diff / summarise subcommands
│   ├── svg_renderer.c     # SVG timeline generator
│   ├── tui_renderer.c     # Block-character terminal renderer
│   ├── trace_diff.c       # Two-trace comparison
│   ├── trace_parser.c     # Event list builder
│   └── Makefile
│
└── docs/
```

---

## Build

Requires: Linux x86-64, GCC, `make`

```bash
# Recorder
cd recorder && make
# → ./echorun

# Replayer
cd replayer && make
# → ./echoplay

# Visualizer
cd visualizer && make
# → ./echovis
```

---

## Usage

**Record**
```bash
./echorun ./your_program [args]
# Produces: trace.bin, trace.idx
# Prints overhead report comparing native vs recorded runtime
```

**Replay**
```bash
./echoplay trace.bin trace.idx ./your_program [args]
# Drops into REPL on completion
```

**REPL commands**
```
echoplay> continue          # resume replay
echoplay> step              # advance one syscall event
echoplay> goto 42           # jump to seq_idx 42
echoplay> peek 0x7ffd1234   # read memory word at address
echoplay> poke 0x7ffd1234 0 # write memory word at address
echoplay> checkpoints       # list saved checkpoints
```

**Visualize**
```bash
./echovis visualise trace.bin trace.idx --output timeline.svg
./echovis visualise trace.bin trace.idx --tui
./echovis diff a.bin a.idx b.bin b.idx --output diff.svg
./echovis summarise trace.bin trace.idx
```

---

## Known Limitations

- **Multi-process tracing**: `clone`/`fork` are classified as `SIDE_EFFECT` and not followed — child process syscalls are not recorded. Full multi-process support would require `PTRACE_O_TRACEFORK` and per-child trace streams.
- **ASLR during replay**: the replayer re-executes the original binary, which may load at a different base address than during recording. Memory payload injection via `PTRACE_POKEDATA` uses recorded addresses, which can be stale if ASLR shifts the mapping. Mitigation would require recording the full memory map layout and adjusting addresses at replay time, or disabling ASLR via `personality(ADDR_NO_RANDOMIZE)` before exec.

---

## Inspiration

[Mozilla rr](https://rr-project.org/) — a production-grade record-replay debugger that handles ASLR, multi-threading, and hardware performance counters. EchoRun is a from-scratch undergraduate reimplementation exploring the same core ideas at a smaller scale.
