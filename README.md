# Linux Kernel Elevator Scheduler

A Linux kernel-space implementation of a multi-floor passenger elevator scheduling system. This project demonstrates Linux kernel compilation, custom system call integration via stub functions, dynamically loadable kernel modules (LKMs), process synchronization using kernel threads and mutexes, and real-time kernel monitoring via `/proc`.

---

## Features

- **Custom System Call Interface:** Adds 3 custom system calls to control the elevator and enqueue passenger requests from user space.
- **Dynamic Kernel Module:** Implements the core elevator algorithm as a loadable kernel module (`elevator.ko`), linking seamlessly with kernel syscall stubs.
- **Kernel Thread Scheduling:** Uses `kthread` and time delays (`ssleep` / `msleep`) to handle elevator movements and loading/unloading asynchronously.
- **Thread Safety & Concurrency:** Synchronizes access to floor queues and elevator states using kernel mutexes (`struct mutex`).
- **Proc Filesystem Interface:** Exposes real-time elevator telemetry and floor queue status through `/proc/elevator`.

---

## System Architecture & Specifications

### Building Configuration
- **Floors:** 1 to 10 (Floor 1 is the lobby).
- **Capacity Limits:**
  - Maximum Weight: 15 weight units
  - Maximum Passengers: 10 passenger units

### Passenger Types
| Type ID | Name         | Passenger Units | Weight Units |
| :---:   | :---         | :---:           | :---:        |
| `0`     | Adult        | 1               | 1            |
| `1`     | Child        | 1               | 0.5          |
| `2`     | Room Service | 2               | 2            |
| `3`     | Bellhop      | 2               | 3            |

### Operational Timing
- **Floor-to-Floor Movement:** 1.0 second delay
- **Loading / Unloading:** 0.5 second delay

### Movement States
- `OFFLINE`: Elevator module installed, but scheduler inactive.
- `IDLE`: Stopped on a floor; no active or pending requests.
- `LOADING`: Stopped on a floor to load or unload passengers.
- `UP`: Moving upward to service a request.
- `DOWN`: Moving downward to service a request.

---

## System Call Specification

The project integrates three system calls into the Linux syscall table:

### 1. `int start_elevator(void)`
- **Description:** Initializes and activates the elevator scheduler.
- **Return Values:** 
  - `0`: Elevator successfully activated.
  - `1`: Elevator is already running.
  - `-ERROR`: Failed to initialize (e.g., memory allocation error).

### 2. `int issue_request(int type, int start_floor, int destination_floor)`
- **Description:** Enqueues a passenger request from `start_floor` to `destination_floor`.
- **Return Values:**
  - `0`: Request successfully created.
  - `1`: Invalid parameters (invalid floor numbers or passenger type).

### 3. `int stop_elevator(void)`
- **Description:** Deactivates the elevator. The elevator stops accepting *new* requests but continues running until all current passengers are safely dropped off.
- **Return Values:**
  - `0`: Deactivation initiated.
  - `1`: Elevator is already stopping or offline.

---

## Repository Structure

```text
.
├── kernel/                   # Core kernel modifications & system call stubs
│   ├── start_elevator.c      # Syscall wrapper and function pointer stub
│   ├── issue_request.c       # Syscall wrapper and function pointer stub
│   ├── stop_elevator.c        # Syscall wrapper and function pointer stub
│   └── Makefile              # Kernel obj-y build configuration
├── elevator/                 # Kernel module implementation
│   ├── elevator.c            # Core elevator logic, scheduling kthread, procfs handler
│   └── Makefile              # Module kbuild configuration
├── tests/                    # User-space testing programs
│   ├── producer.c            # Generates random passenger requests
│   └── consumer.c            # User CLI to issue start/stop commands
└── README.md
```

---

## Building & Installation

### Prerequisites
- Linux Kernel source tree (v4.x recommended, e.g., Linux 4.15)
- GCC, `make`, and essential kernel build tools (`build-essential`)

### Step 1: Recompile the Linux Kernel
1. Clone the updated syscall wrapper source files into your kernel source tree (e.g., `linux-4.15/kernel/`).
2. Add system call table entries in `arch/x86/entry/syscalls/syscall_64.tbl`.
3. Add function prototypes to `include/linux/syscalls.h`.
4. Recompile and install your kernel:
   ```bash
   make -j$(nproc)
   sudo make modules_install
   sudo make install
   sudo reboot
   ```

### Step 2: Build the Elevator Kernel Module
Navigate to the `elevator/` directory and compile the loadable kernel module:
```bash
cd elevator
make
```

### Step 3: Insert the Module
Insert the compiled module into the running kernel:
```bash
sudo insmod elevator.ko
```

---

## 💻 Usage & Verification

### 1. Monitor Elevator Telemetry
To inspect the live status of the elevator and floor queues, print the contents of `/proc/elevator`:
```bash
cat /proc/elevator
```

### 2. Control the Elevator (`consumer`)
Compile user-space utilities and control elevator service:
```bash
# Start elevator
gcc consumer.c -o consumer
./consumer --start

# Stop elevator
./consumer --stop
```

### 3. Generate Workload (`producer`)
Issue simulated passenger requests:
```bash
gcc producer.c -o producer
./producer <number_of_requests>
```

---

## Cleanup & Removal

To stop service, remove the module, and clean build artifacts:

```bash
# Deactivate elevator via consumer
./consumer --stop

# Unload kernel module
sudo rmmod elevator

# Clean user binaries and module build files
cd elevator && make clean
```
