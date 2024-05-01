#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <queue.h>
#include "pico/stdlib.h"
#include "semphr.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "timers.h"

#define BUTTON_1_PIN 15
#define BUTTON_2_PIN 14
//array of new task handles
#define MAX_NEW_TASKS 35
TaskHandle_t new_task_handles[MAX_NEW_TASKS];
uint32_t num_tasks;

TaskHandle_t ButtonTaskHandle_1;
TaskHandle_t ButtonTaskHandle_2;

TimerHandle_t xDebounceTimer1, xDebounceTimer2;
uint32_t alloc_time, free_time;

uint64_t get_system_time(){
    uint32_t high, low;
    do{
        high = timer_hw->timerawl;
        low = timer_hw->timerawl;
    }while(high != timer_hw->timerawl);
    return ((uint64_t)high << 32) | low;
}
//interrupt handler
void debounceTimerCallback1(TimerHandle_t xTimer){
    if(gpio_get(BUTTON_1_PIN) == 1){
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(ButtonTaskHandle_1, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

void debounceTimerCallback2(TimerHandle_t xTimer){
    if(gpio_get(BUTTON_2_PIN) == 1){
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(ButtonTaskHandle_2, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}


void button_isr(uint gpio, uint32_t events){
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    //printf("inside the isr");
    if(gpio == BUTTON_1_PIN && events == GPIO_IRQ_EDGE_RISE)
    {
        //printf("Button 1 pressed\n");
        //vTaskNotifyGiveFromISR(ButtonTaskHandle_1, &xHigherPriorityTaskWoken);
        xTimerStartFromISR(xDebounceTimer1, NULL);
        
    }
    else if(gpio == BUTTON_2_PIN && events == GPIO_IRQ_EDGE_RISE){
        //printf("Button 2 pressed\n");
        //vTaskNotifyGiveFromISR(ButtonTaskHandle_2, &xHigherPriorityTaskWoken);
        xTimerStartFromISR(xDebounceTimer2, NULL);
    }
    //portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void init_debounce_timers(){
    const TickType_t xTimerPeriod = pdMS_TO_TICKS(50);
    xDebounceTimer1 = xTimerCreate("DehounceTimer1", xTimerPeriod, pdFALSE, NULL, debounceTimerCallback1);
    xDebounceTimer2 = xTimerCreate("DehounceTimer2", xTimerPeriod, pdFALSE, NULL, debounceTimerCallback2);

    if(xDebounceTimer1 == NULL || xDebounceTimer2 == NULL){
        printf("Timer creation failed\n");
    }
    else{
        printf("Timer created\n");
    }
}


void init_button_interrupt(){
    gpio_init(BUTTON_1_PIN);
    gpio_set_dir(BUTTON_1_PIN, GPIO_IN);
    gpio_pull_down(BUTTON_1_PIN); //use as pull down

    gpio_init(BUTTON_2_PIN);
    gpio_set_dir(BUTTON_2_PIN, GPIO_IN);
    gpio_pull_down(BUTTON_2_PIN); //use as pull down

    init_debounce_timers();

    gpio_set_irq_enabled_with_callback(BUTTON_1_PIN, GPIO_IRQ_EDGE_RISE, true, &button_isr);
    gpio_set_irq_enabled_with_callback(BUTTON_2_PIN, GPIO_IRQ_EDGE_RISE, true, &button_isr);
}

void heap_stats_task(void *pvParameters){
    HeapStats_t xHeapStats;
    vPortGetHeapStats(&xHeapStats);

    // Print heap statistics via USB UART
    printf("Heap Free Size: %u bytes\n", xHeapStats.xAvailableHeapSpaceInBytes);
    printf("Heap Minimum Ever Free Size: %u bytes\n", xHeapStats.xMinimumEverFreeBytesRemaining);
    printf("Number of successful allocations: %u\n", xHeapStats.xNumberOfSuccessfulAllocations);
    printf("Number of successful frees: %u\n", xHeapStats.xNumberOfSuccessfulFrees);
    printf("Size of smallest free block: %u bytes\n", xHeapStats.xSizeOfSmallestFreeBlockInBytes);
    printf("Size of largest free block: %u bytes\n", xHeapStats.xSizeOfLargestFreeBlockInBytes);

    vTaskDelete(NULL);
}

uint32_t num_alloc_counted = 0;
uint32_t num_free_counted = 0;



void * PortMalloc_tm(size_t xWantedSize){\
    uint32_t start_time, end_time;
    start_time = time_us_32();
    void * pvReturn = pvPortMalloc(xWantedSize);
    end_time = time_us_32();
    if(pvReturn == NULL){
        printf("Malloc failed\n");
        return NULL;
    }
    alloc_time += end_time - start_time;
    num_alloc_counted++;
    
    return pvReturn;
}

void PortFree_tm(void *pv){
    uint32_t start_time, end_time;
    start_time = time_us_32();
    vPortFree(pv);
    end_time = time_us_32();
    free_time += end_time - start_time;
    num_free_counted++;
}

void send_stats_to_uart(){
    HeapStats_t HeapStats;
    vPortGetHeapStats(&HeapStats);
    int time_stamp = 0;

    //encode the information in csv format
    printf("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", time_stamp, HeapStats.xAvailableHeapSpaceInBytes, 
        HeapStats.xSizeOfSmallestFreeBlockInBytes, HeapStats.xSizeOfLargestFreeBlockInBytes,
        num_alloc_counted,alloc_time,num_free_counted,free_time,num_tasks,HeapStats.xTotalInternalFragmentationInBytes,
        HeapStats.xNumberOfSuccessfulAllocations,HeapStats.xNumberOfSuccessfulFrees,HeapStats.xMinimumEverFreeBytesRemaining,
        HeapStats.xNumberOfFreeBlocks);

    //delay for the uart to sena and recieve
    sleep_ms(100);
}

//Static Allocation Task
void heap_task(void *pvParameters){
    const size_t alloc_size = 400;
    uint32_t start_time, end_time;

    start_time = time_us_32();
    void *allocated_memory = pvPortMalloc(alloc_size);
    end_time = time_us_32();
    alloc_time += end_time - start_time;
    num_alloc_counted++;

    while(1){
        uint32_t ulNotificationValue = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if(ulNotificationValue > 0){
            if(allocated_memory != NULL){
                start_time = time_us_32();
                vPortFree(allocated_memory);
                end_time = time_us_32();
                free_time += end_time - start_time;
                num_free_counted++;
                allocated_memory = NULL;
            }
            vTaskDelete(NULL);
        }    
    }
}

//Dynamic Allocation Task
void heap_task2(void *pvParameters){
    
    void *a1 = PortMalloc_tm(1000);
    send_stats_to_uart();
    void *a2 =  PortMalloc_tm(200);
    send_stats_to_uart();
    void *a3 = PortMalloc_tm(400);
    send_stats_to_uart();
    void *a4 = PortMalloc_tm(800);
    send_stats_to_uart();
    void *a5 = PortMalloc_tm(1600);
    send_stats_to_uart();

    //freeing in random order
    PortFree_tm(a3);
    send_stats_to_uart();
    PortFree_tm(a1);
    send_stats_to_uart();
    PortFree_tm(a5);
    send_stats_to_uart();
    PortFree_tm(a2);
    send_stats_to_uart();
    PortFree_tm(a4);
    send_stats_to_uart();

    void *a6, *a7, *a8, *a9, *a10;

    //alloc random sized blocks 10 times with diff pointers assigned
    a1 = PortMalloc_tm(1000);
    send_stats_to_uart();
    a2 =  PortMalloc_tm(200);
    send_stats_to_uart();
    a3 = PortMalloc_tm(2000);
    send_stats_to_uart();
    a4 = PortMalloc_tm(800);
    send_stats_to_uart();
    a5 = PortMalloc_tm(1600);
    send_stats_to_uart();
    a6 = PortMalloc_tm(3200);
    send_stats_to_uart();
    a7 = PortMalloc_tm(50);
    send_stats_to_uart();
    a8 = PortMalloc_tm(100);
    send_stats_to_uart();
    a9 = PortMalloc_tm(400);
    send_stats_to_uart();
    a10 = PortMalloc_tm(800);
    send_stats_to_uart();
    

    //free all the blocks in random order
    PortFree_tm(a3);
    send_stats_to_uart();
    PortFree_tm(a1);
    send_stats_to_uart();
    PortFree_tm(a5);
    send_stats_to_uart();
    PortFree_tm(a2);
    send_stats_to_uart();


    while(1){
        uint32_t ulNotificationValue = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if(ulNotificationValue > 0){
            PortFree_tm(a4);
            send_stats_to_uart();
            PortFree_tm(a6);
            send_stats_to_uart();
            PortFree_tm(a7);
            send_stats_to_uart();
            PortFree_tm(a8);
            send_stats_to_uart();
            PortFree_tm(a9);
            send_stats_to_uart();
            PortFree_tm(a10);
            send_stats_to_uart();
            vTaskDelete(NULL);
        }
    }
}


void heap_stats_to_uart(void * pvParameters){
    TickType_t time_stamp = xTaskGetTickCount();
    HeapStats_t HeapStats;
    vPortGetHeapStats(&HeapStats);

    //encode the information in csv format
    printf("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", time_stamp, HeapStats.xAvailableHeapSpaceInBytes, 
        HeapStats.xSizeOfSmallestFreeBlockInBytes, HeapStats.xSizeOfLargestFreeBlockInBytes,
        num_alloc_counted,alloc_time,num_free_counted,free_time,num_tasks,HeapStats.xTotalInternalFragmentationInBytes,
        HeapStats.xNumberOfSuccessfulAllocations,HeapStats.xNumberOfSuccessfulFrees,HeapStats.xMinimumEverFreeBytesRemaining,
        HeapStats.xNumberOfFreeBlocks);

    vTaskDelete(NULL);
}

void ButtonTask_1(void *pvParameters){
    while(1){
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY); //wait for the interrupt handler to give notify
        //printf("New Heap task adding\n");
        if(num_tasks < MAX_NEW_TASKS){
            //char * task_name = (char *)pvPortMalloc(20);
            //sprintf(task_name, "Heap Task%d", num_tasks);
            xTaskCreate(heap_task2, "Heap Task", 1000, NULL, tskIDLE_PRIORITY+2, &new_task_handles[num_tasks]); //hiher priority
            //xTaskCreate(heap_stats_to_uart, "Heap Stats Task", 256, NULL, tskIDLE_PRIORITY+3, NULL); //lower priority
            num_tasks++;
        }
    }
}

void ButtonTask_2(void *pvParameters){
    while(1){
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY); //wait for the interrupt handler to give notify
        //printf("Killing the Heap Task\n");
        if(num_tasks > 0){
            //this should free and delete the task
            //printf("Deleting task index %d\n", num_tasks-1);
            xTaskNotifyGive(new_task_handles[num_tasks-1]);
            //xTaskCreate(heap_stats_to_uart, "Heap Stats Task", 256, NULL, tskIDLE_PRIORITY+2, NULL); //lower priority
            num_tasks--;
        }
    }
}


void led_task()
{   
    const uint LED_PIN = PICO_DEFAULT_LED_PIN;
    uint uIValueToSend = 0;

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    while (true) {
        gpio_put(LED_PIN, 1);
        uIValueToSend = 1;
        //xQueueSend(xQueue, &uIValueToSend, 0U);


        vTaskDelay(10000);
        gpio_put(LED_PIN, 0);
        uIValueToSend = 0;
        //xQueueSend(xQueue, &uIValueToSend, 0U);
        vTaskDelay(10000);
    }
}

void usb_task(void *pvParameters)
{
    uint UIReceivedValue = 0;

    while(1){
        //xQueueReceive(xQueue, &UIReceivedValue, portMAX_DELAY);

        if(UIReceivedValue == 1){
            //printf("LED is ON\n");
        }
        else{
            //printf("LED is OFF\n");
        }
    }
}



//Task to collect and print runtime statistics
void vRuntimeStatsTask(void *pvParameters){
    const TickType_t xDelay = pdMS_TO_TICKS(10000);
    char *statsBuffer;

    while(1){
        statsBuffer = (char *)pvPortMalloc(1024);
        if(statsBuffer != NULL){
            vTaskGetRunTimeStats(statsBuffer);
            printf("Runtime Stats: \n%s\n", statsBuffer);
            vPortFree(statsBuffer);
        }
        vTaskDelay(xDelay);
    }

}

int main()
{
    stdio_init_all();

    //xQueue = xQueueCreate(1, sizeof(uint));
    num_tasks = 0;

    xTaskCreate(led_task, "Task 1", 256, NULL, 1, NULL);
    //xTaskCreate(usb_task, "Task 2", 256, NULL, 1, NULL);
    //xTaskCreate(vRuntimeStatsTask, "stats", 2048, NULL, tskIDLE_PRIORITY+2, NULL);
    init_button_interrupt();
    xTaskCreate(heap_stats_task, "Heap Stats Task", 256, NULL, tskIDLE_PRIORITY+2, NULL); //inital heap stats task
    xTaskCreate(ButtonTask_1, "Button Task1", 256, NULL, tskIDLE_PRIORITY+4, &ButtonTaskHandle_1);
    xTaskCreate(ButtonTask_2, "Button Task2", 256, NULL, tskIDLE_PRIORITY+4, &ButtonTaskHandle_2);
    vTaskStartScheduler();

    while(1){};
}