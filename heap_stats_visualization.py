#  Visualize the heap statistics of the FreeRTOS running on rp2040
# It will receive information from UART and visualize it using matplotlib based on timsstamp
# Displaying the heap statistics in real-time
# Real time Heap Usage Percentage
# Interanl Fragmentation
# External Fragmentation Information
    
import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from datetime import datetime
import queue
from threading import Thread

#setting information
serial_port = 'COM10' #set this to the port where the rp2040 is connected
baud_rate = 115200
heap_size_total = 128 * 1024  # Total heap size in bytes
update_interval = 10  # Update interval for the plot in milliseconds
tick_rate = 100000 #set to configTICK_RATE_HZ in FreeRTOSConfig.h

# Initialize serial port
ser = serial.Serial(serial_port, baud_rate, timeout=None)

# Setup Matplotlib figure and subplots
fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(10, 15))
fig.suptitle('Real-time Heap Statistics')

# Initialize lines for each plot
line1, = ax1.plot([], [], 'r-', label='Heap Usage (%)')
line2, = ax2.plot([], [], 'y-', label='Accumulated Internal Fragmentation (bytes)')
line3, = ax3.plot([], [], 'g-', label='Largest Free Block Size (bytes)')
ax4 = ax3.twinx()  # Twin axis for different scale data
line4, = ax4.plot([], [], 'b-', label='Number of Free Blocks')

ax1.legend(loc='best')
ax2.legend(loc='best')
ax3.legend(loc='best')
ax4.legend(loc='lower left')  # Adjust legend position

ax1.set_xlabel('Number of Heap Operations (alloc & free)')
ax1.set_ylabel('Usage (%)')
ax1.set_title('Real Time Heap Usage')
ax1.grid(True)
ax1.set_ylim(0, 100)
ax1.xaxis.set_major_locator(plt.MaxNLocator(integer=True))

ax2.set_xlabel('Number of Succesful Heap Allocation')
ax2.set_ylabel('Internal Fragmentation (bytes)')
ax2.set_title('Internal Fragmentation Statistics')
ax2.grid(True)
ax2.xaxis.set_major_locator(plt.MaxNLocator(integer=True))
ax2.yaxis.set_major_locator(plt.MaxNLocator(integer=True))

# Data lists for plotting
xs1 = [] # Number of successful operations
xs2 = [] # sum sucessful allocation
heap_usages = []
internal_frags = []
max_free_block_sizes = []
max_max_free_block = 0
num_free_blocks = []

ax3.set_xlabel('Number of Operations (alloc & free)')
ax3.set_ylabel('Largest Free Block Size (bytes)')
ax3.set_title('External Fragmentation Statistics')
ax3.grid(True)
ax3.xaxis.set_major_locator(plt.MaxNLocator(integer=True))
ax3.yaxis.set_major_locator(plt.MaxNLocator(integer=True))

ax4.set_ylabel('Number of Free Blocks')
ax4.yaxis.set_major_locator(plt.MaxNLocator(integer=True))



# Static text display setup
info_text = plt.figtext(0.1, 0.02, '', fontsize=11, horizontalalignment='left', verticalalignment='bottom')
#Data Queue
data_queue = queue.Queue()

def read_serial_data():
    while True:
        line = ser.readline().decode('utf-8').strip()
        if line:
            data_queue.put(line)
            
#start the thread reading the uart serial data
serial_thread = Thread(target=read_serial_data)
serial_thread.daemon = True
serial_thread.start()


def print_received_data():
    try:
        while True:
            line = ser.readline().decode('utf-8').strip()
            if line:
                print(f"Received Line: {line}")
                data = line.split(',')

                # Parse data
                ticks = int(data[0])
                free_space = int(data[1])
                min_free_block_size = int(data[2])
                max_free_block_size = int(data[3])

                # Print parsed data
                print(f"Ticks: {ticks}")
                print(f"Free Space: {free_space} bytes")
                print(f"Minimum Free Block Size: {min_free_block_size} bytes")
                print(f"Maximum Free Block Size: {max_free_block_size} bytes")
                print("------------")
    except KeyboardInterrupt:
        print("Stopped by user.")
    finally:
        ser.close()
        print("Serial port closed.")

def animate(i):
    global max_max_free_block
    while not data_queue.empty():
        line = data_queue.get()
        if line:
            data = line.split(',')
            ticks = int(data[0])
            free_space = int(data[1])
            min_free_block_size = int(data[2])
            max_free_block_size = int(data[3])
            alloc_num = int(data[4])
            alloc_time = int(data[5])
            free_num = int(data[6])
            free_time = int(data[7])
            num_tasks = int(data[8])
            internal_frag = int(data[9])
            num_success_alloc = int(data[10])
            num_success_free = int(data[11])
            minimum_ever_free = int(data[12])
            num_free_block = int(data[13])
            

            # Update the heap usage data
            #calculate the current time based on the tick rate
            #current_time = ticks / tick_rate;
            num_operations = num_success_alloc + num_success_free
            
            xs1.append(num_operations)
            xs2.append(num_success_alloc)      
            heap_usage = ((heap_size_total - free_space) * 100) / heap_size_total
            heap_usages.append(heap_usage)
            internal_frags.append(internal_frag)
            max_free_block_sizes.append(max_free_block_size)
            #update max_max_free_block
            if(max_free_block_size > max_max_free_block):
                max_max_free_block = max_free_block_size
            num_free_blocks.append(num_free_block)
            
            # update the usage plot
            line1.set_data(xs1, heap_usages)
            if len(xs1) > 1:
                ax1.set_xlim(xs1[0], num_operations)

            ax1.fill_between(xs1, 0, heap_usages, color='red', alpha=0.05)  # Filling under the line
            ax1.autoscale_view()
            
            #update the Internal Fragmentation time plot
            line2.set_data(xs2, internal_frags)
            if len(xs2) > 1:
                ax2.set_xlim(xs2[0], num_success_alloc)
            ax2.set_ylim(0, internal_frag + 2000)
            ax2.autoscale_view()
            
            #update the free time plot
            line3.set_data(xs1, max_free_block_sizes)
            if len(xs1) > 1:
                ax3.set_xlim(xs1[0], num_operations)
            ax3.set_ylim(0, max_max_free_block+2000)
            ax3.fill_between(xs1, 0, max_free_block_sizes, color='green', alpha=0.02)  # Filling under the line
            ax3.autoscale_view()
            
            line4.set_data(xs1, num_free_blocks)
            ax4.set_ylim(0,  max(num_free_blocks) + 2)
            ax4.autoscale_view()
            
            # Update text data
            info_text.set_text( f"Number of heap operations: {num_operations} (Alloc : {num_success_alloc}, Free: {num_success_free})                    Number of Heap Tasks: {num_tasks}\n" 
                                f"Figure1 : Heap Usage: {heap_usage:.2f}%                          Free Space: {free_space} bytes\n"
                                f"Figure2 : Internal Fragmentation: {internal_frag} bytes\n"
                                f"Figure 3: Max Block Size: {max_free_block_size} bytes                Number of Free Blocks: {num_free_block}\n\n"
                                f"Latency: Allocation Time: {alloc_time} us/ {alloc_num} alloc ops       Freeing Time: {free_time} us/ {free_num} free ops \n"
                                f"Minimum Ever Free Block: {minimum_ever_free} bytes                        Min Block Size {min_free_block_size}\n")


            plt.xticks(rotation=45, ha='right')
            plt.subplots_adjust(hspace=0.5, bottom=0.25, top=0.90, right=0.95)   

#print_received_data()

# Start Matplotlib Animation
ani = animation.FuncAnimation(fig, animate, interval=update_interval)
plt.show()
 
