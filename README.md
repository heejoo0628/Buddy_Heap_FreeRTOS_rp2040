# Buddy_Heap_FreeRTOS_RP2040

The core of the project is implementation of the buddy heap allocation system that can be deployed on FreeRTOS running on Raspberry Pi Pico Board.
The two different buddy algorithms are implemented rewriting significant logic from original First Fit implementation (heap_4.c) of FreeRTOS.
They can be found in the heap folder.

The src_main_FreeRTIS contains application program that works with Heap Manager (visualization.py)
