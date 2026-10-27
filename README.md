# Linux Interrupt Latency Monitor

This is a custom Linux kernel module designed to monitor and measure interrupt latency. If you are working on real-time systems, 
device drivers, or just exploring Linux kernel internals, this tool helps you profile how quickly the kernel responds to hardware interrupts.

By running directly in kernel space, it bypasses user-space overhead to provide accurate, low-level timing metrics.

## Core Files
* **`main.c`:** The core C source code for the Loadable Kernel Module (LKM) that handles the interrupt latency monitoring logic.
* **`Makefile`:** The build configuration file used to compile the module against your local Linux kernel headers.
* **`main.ko`:** A pre-compiled kernel object module included in the directory. 
