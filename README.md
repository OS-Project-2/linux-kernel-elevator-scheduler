# Project 2: Elevator Kernel Module
# COP4610: Operating Systems

# Group Members
- Kenny La

# Division of Labor
- Kenny La - all parts

# File Listing
- part1/empty.c
- part1/part1.c
- part1/empty.trace
- part1/part1.trace
- part2/src/my_timer.c
- part3/syscalls.c
- part3/src/elevator.c

# How to Compile and Run

# Part 1
```bash
cd part1
make
strace -o empty.trace ./empty
strace -o part1.trace ./part1
```

# Part 2
```bash
cd part2
make
sudo insmod src/my_timer.ko
cat /proc/timer
sudo rmmod my_timer
```

# Part 3
```bash
cd part3
make
sudo insmod src/elevator.ko
cat /proc/elevator
sudo rmmod elevator
```
