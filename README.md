# Real-Time Scheduling Policies Evaluation in Linux

## Course

E604 Real-Time Systems — University of Thessaly

## Abstract

This project evaluates real-time scheduling policies in Linux by measuring deadline misses, response time, and latency for periodic tasks. Different priority assignment strategies and CPU pinning effects are also examined.

## System Model

Four periodic tasks are used:

| Task | Period | Execution | Deadline |
| ---- | ------ | --------- | -------- |
| T1   | 50 ms  | 10 ms     | 50 ms    |
| T2   | 100 ms | 20 ms     | 100 ms   |
| T3   | 200 ms | 40 ms     | 200 ms   |
| T4   | 400 ms | 80 ms     | 400 ms   |

## Scheduling Policies

* SCHED_OTHER (CFS)
* SCHED_FIFO
* SCHED_RR
* SCHED_DEADLINE (EDF)

## Priority Strategies

* Equal priority
* Rate Monotonic Scheduling (RMS)
* Dynamic priority adjustment

## Metrics

* Deadline misses
* Average response time
* Average latency

## Experimental Setup

* POSIX threads (`pthread`)
* `CLOCK_MONOTONIC` timing
* Experiments with and without CPU pinning (core 0)

## Compilation & Execution

```bash
gcc -o scheduler main.c -lpthread -lm
sudo ./scheduler
```

## Notes

* Root privileges are required for real-time policies
* SCHED_DEADLINE requires kernel support

## References

* Liu & Layland (1973)
* Linux `sched(7)` manual
* Buttazzo, *Hard Real-Time Computing Systems*


