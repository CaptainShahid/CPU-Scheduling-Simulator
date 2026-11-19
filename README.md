# Topic 4: CPU Scheduling Simulator

## Requirements
- Linux, macOS, or another POSIX-like environment
- GCC or Clang
- C11 compiler support

## Build
```bash
gcc -std=c11 -Wall -Wextra -O2 scheduler.c -o scheduler
```

## Run
```bash
./scheduler
```

Enter the process count, then arrival time, CPU burst, and priority for each process.
Choose one algorithm or option 6 to run all five. Round Robin asks for a time quantum.

## Algorithms and policies
1. FCFS: non-preemptive; processes are selected by arrival time.
2. SJF: non-preemptive; shortest available CPU burst first.
3. SRTF: preemptive; shortest remaining burst first, reconsidered each time unit.
4. Round Robin: preemptive; FIFO ready queue and configurable quantum.
5. Priority: non-preemptive; smaller numeric priority means higher priority.

Ties are resolved by earlier arrival time and then process input order. Idle CPU intervals
are shown in the Gantt chart.

## Metrics
- CT = completion time
- TAT = CT - arrival time
- WT = TAT - burst time
- RT = first start time - arrival time

## Limitations
This is a teaching simulator using integer time units. SRTF is evaluated at each unit
boundary. Priority scheduling is non-preemptive; aging is not implemented. The simulator
does not model context-switch overhead or I/O blocking.
