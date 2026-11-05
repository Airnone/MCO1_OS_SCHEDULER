# CSOPESY Process Scheduler Emulator

A CLI-based process scheduler and multiplexer written in C++17, modeled on Linux `screen`, SLURM-style batch scripts, and PowerShell. Built as a Major Course Output for CSOPESY (Operating Systems).

## Features

- **Multi-core CPU simulation** — configurable 1–128 virtual cores, all stepped once per CPU tick by the scheduler thread
- **FCFS & Round Robin scheduling** — switchable via `config.txt` without recompiling
- **6-instruction process model** — PRINT, DECLARE, ADD, SUBTRACT, SLEEP, FOR (nestable up to 3 levels)
- **Screen multiplexer** — create, reattach, and list process screens (`screen -s`, `screen -r`, `screen -ls`)
- **Batch process generation** — auto-spawn processes at a configurable tick interval
- **Reporting** — CPU utilization, per-process progress, and log file export

## Project Structure

```
MCO1_OS_SCHEDULER/
├── config.txt                          # Runtime configuration (read by 'initialize')
├── CMakeLists.txt                      # CMake build file (C++17)
└── src/
    ├── main.cpp                        # Entry point
    ├── cli/
    │   ├── Console.h / .cpp            # Main menu loop, banner, command dispatch
    │   └── CommandParser.h / .cpp      # Tokenizes and validates typed commands
    ├── config/
    │   └── Config.h / .cpp             # Parses config.txt, validates parameter ranges
    ├── process/
    │   ├── Process.h / .cpp            # PCB: state, variable table, execution stack, logs
    │   ├── Instruction.h               # Instruction types and operand definitions
    │   └── InstructionGenerator.h/.cpp # Randomized instruction list generation
    ├── scheduler/
    │   ├── Scheduler.h / .cpp          # Ready queue, dispatch loop, FCFS/RR strategy
    │   ├── Core.h / .cpp               # Simulated CPU core (tick-driven execution)
    │   └── Clock.h                     # Global atomic CPU tick counter
    ├── screen/
    │   └── ScreenManager.h / .cpp      # screen -s/-r/-ls, process-smi, attach/detach
    └── report/
        └── ReportUtil.h / .cpp         # Report formatter, csopesy-log.txt writer
```

## Building

### Prerequisites

- A C++17-compatible compiler (GCC, Clang, or MSVC)
- CMake 3.16+

### Build Steps

```bash
# From the project root:
mkdir build
cd build
cmake ..
cmake --build .
```

This produces `csopesy.exe` (Windows) or `csopesy` (Linux/macOS) inside the `build/` directory.

### Running

Run the executable **from the project root** (where `config.txt` is located):

```bash
# Windows
.\build\csopesy.exe

# Linux/macOS
./build/csopesy
```

## Configuration

All behavior is driven by `config.txt`, read once when the `initialize` command is executed. The file uses space-separated key/value pairs:

```
num-cpu 4
scheduler "rr"
quantum-cycles 5
batch-process-freq 1
min-ins 1000
max-ins 2000
delays-per-exec 0
```

| Parameter | Range | Description |
|-----------|-------|-------------|
| `num-cpu` | 1–128 | Number of simulated CPU cores |
| `scheduler` | `"fcfs"` or `"rr"` | Scheduling algorithm |
| `quantum-cycles` | 1–2³² | Round-robin time slice (ignored under FCFS) |
| `batch-process-freq` | 1–2³² | CPU ticks between each auto-spawned process |
| `min-ins` | 1–2³² | Minimum instructions per process |
| `max-ins` | 1–2³² | Maximum instructions per process |
| `delays-per-exec` | 0–2³² | Busy-wait ticks before each instruction (0 = one instruction per tick) |

> **Note:** Only `config.txt` changes between test cases — no recompilation needed.

## Commands

### Main Menu

| Command | Description |
|---------|-------------|
| `initialize` | Reads `config.txt`, creates the scheduler and cores. Must run first. |
| `screen -s <name>` | Creates a new process, enqueues it, and attaches to its screen |
| `screen -r <name>` | Reattaches to an existing, unfinished process's screen |
| `screen -ls` | Prints CPU utilization, cores used/available, running & finished processes |
| `scheduler-start` | Begins auto-generating dummy processes every `batch-process-freq` ticks |
| `scheduler-stop` | Stops generating new processes; running ones continue to completion |
| `report-util` | Appends the `screen -ls` report to `csopesy-log.txt` |
| `exit` | Terminates the emulator |

### Process Screen (inside `screen -s` or `screen -r`)

| Command | Description |
|---------|-------------|
| `process-smi` | Shows process name, ID, full timestamped print log, current/total instruction lines, and `Finished!` status |
| `exit` | Returns to the main menu (does **not** kill the process) |

## Scheduling Algorithms

### FCFS (First-Come-First-Served)

Each process runs to completion (or until it hits a SLEEP instruction) on whichever core frees up next, in ready-queue order. No preemption — `quantum-cycles` has no effect.

### Round Robin

Each core runs its current process for up to `quantum-cycles` ticks, then preempts it back onto the end of the ready queue (even if unfinished) and pulls the next ready process.

### Process Lifecycle

```
New → Ready → Running → Finished
               ↓   ↑
            Sleeping (SLEEP instruction parks process for X ticks)
               ↓   ↑
             Ready  (preempted by RR quantum)
```

## Instruction Set

All instructions are randomly generated at process creation time — never typed by the user. They operate on process-local `uint16_t` variables clamped to [0, 65535].

| Instruction | Syntax | Semantics |
|-------------|--------|-----------|
| PRINT | `PRINT(msg)` | Logs `msg` to the process buffer (visible via `process-smi`). Default: `"Hello world from <name>!"` |
| DECLARE | `DECLARE(var, value)` | Creates a `uint16_t` variable initialized to `value` |
| ADD | `ADD(var1, var2, var3)` | `var1 = var2 + var3`, clamped to [0, 65535]. Undeclared operands auto-create at 0 |
| SUBTRACT | `SUBTRACT(var1, var2, var3)` | `var1 = var2 - var3`, clamped at 0 on underflow (never wraps) |
| SLEEP | `SLEEP(X)` | Parks the process off the core for `X` CPU ticks, then returns it to Ready |
| FOR | `FOR([instructions], repeats)` | Repeats a nested instruction block `repeats` times; nestable up to 3 levels |

## Report Format

Both `screen -ls` and `report-util` render identical output:

```
CPU utilization: 100%
Cores used: 4
Cores available: 0

--------------------------------------
Running processes:
process05   (01/18/2024 09:15:22AM)   Core: 0   1235 / 5876
process06   (01/18/2024 09:17:22AM)   Core: 1   3 / 5876

Finished processes:
process01   (01/18/2024 09:00:21AM)   Finished   5876 / 5876
process02   (01/18/2024 09:00:22AM)   Finished   5876 / 5876
--------------------------------------
```

Timestamps use the format `MM/DD/YYYY hh:mm:ssAM/PM`.

## Architecture

A single scheduler thread runs the tick loop. Each tick it wakes sleeping processes, spawns batch processes, assigns ready processes from the mutex-protected ready queue to free cores, executes one tick on every busy core, applies Round Robin preemption, and refills freed cores before advancing the global atomic tick counter (independent of wall-clock time). `screen -ls` and `report-util` read a snapshot taken between ticks, so their numbers are always consistent.

```
Config ──→ Scheduler (tick thread) ──→ Core 0
               │                  ──→ Core 1
               │                  ──→ Core N
               │
Console ──→ ScreenManager ──→ process-smi
       ──→ ReportUtil     ──→ csopesy-log.txt
```
