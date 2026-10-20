/*
 * monitor.c
 *
 *  Created on: Jan 10, 2026
 *      Author: Kamran
 */


#include "monitor.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <stdio.h>

#define MAX_TASKS 16

typedef struct
{
    char name[configMAX_TASK_NAME_LEN];
    uint32_t lastRunTime;
    uint8_t cpuPercent;
} TaskCPUInfo_t;

static TaskCPUInfo_t taskCPUInfo[MAX_TASKS];
static uint32_t lastTotalTime = 0;
static uint8_t totalCPULoad = 0;

/*---------------------------------------------------------------------------*/
/* Timer for runtime stats (TIM2)                                            */
/*---------------------------------------------------------------------------*/
void vConfigureTimerForRunTimeStats(void)
{
    __HAL_RCC_TIM2_CLK_ENABLE();

    TIM2->PSC = 839;         // 84MHz APB1 / 840 = 100kHz (10us tick)
    TIM2->ARR = 0xFFFFFFFF;  // 32-bit max
    TIM2->CNT = 0;
    TIM2->CR1 = TIM_CR1_CEN; // Enable timer
}

uint32_t ulGetRunTimeCounterValue(void)
{
    return TIM2->CNT;
}

/*---------------------------------------------------------------------------*/
/* CPU Load Calculation                                                      */
/*---------------------------------------------------------------------------*/
static void CalculateCPULoad(void)
{
    TaskStatus_t *taskStatusArray;
    UBaseType_t numTasks;
    uint32_t totalRunTime;
    uint32_t deltaTotal;

    numTasks = uxTaskGetNumberOfTasks();

    if (numTasks > MAX_TASKS)
    {
        numTasks = MAX_TASKS;
    }

    taskStatusArray = pvPortMalloc(numTasks * sizeof(TaskStatus_t));
    if (taskStatusArray == NULL)
    {
        return;
    }

    numTasks = uxTaskGetSystemState(taskStatusArray, numTasks, &totalRunTime);
    deltaTotal = totalRunTime - lastTotalTime;

    if (deltaTotal > 0)
    {
        uint32_t idleTime = 0;

        for (UBaseType_t i = 0; i < numTasks; i++)
        {
            uint32_t deltaTask = 0;

            // Find previous runtime for this task
            for (uint8_t j = 0; j < MAX_TASKS; j++)
            {
                if (strcmp(taskCPUInfo[j].name, taskStatusArray[i].pcTaskName) == 0)
                {
                    deltaTask = taskStatusArray[i].ulRunTimeCounter - taskCPUInfo[j].lastRunTime;
                    taskCPUInfo[j].lastRunTime = taskStatusArray[i].ulRunTimeCounter;
                    taskCPUInfo[j].cpuPercent = (uint8_t)((deltaTask * 100) / deltaTotal);
                    break;
                }
                // New task - add to array
                if (taskCPUInfo[j].name[0] == '\0')
                {
                    strncpy(taskCPUInfo[j].name, taskStatusArray[i].pcTaskName, configMAX_TASK_NAME_LEN - 1);
                    taskCPUInfo[j].lastRunTime = taskStatusArray[i].ulRunTimeCounter;
                    taskCPUInfo[j].cpuPercent = 0;
                    break;
                }
            }

            // Track idle time
            if (strcmp(taskStatusArray[i].pcTaskName, "IDLE") == 0)
            {
                for (uint8_t j = 0; j < MAX_TASKS; j++)
                {
                    if (strcmp(taskCPUInfo[j].name, "IDLE") == 0)
                    {
                        idleTime = deltaTask;
                        break;
                    }
                }
            }
        }

        totalCPULoad = 100 - (uint8_t)((idleTime * 100) / deltaTotal);
    }

    lastTotalTime = totalRunTime;
    vPortFree(taskStatusArray);
}

/*---------------------------------------------------------------------------*/
/* Print CPU Stats                                                           */
/*---------------------------------------------------------------------------*/
static void PrintCPUStats(void)
{
    TaskStatus_t *taskStatusArray;
    UBaseType_t numTasks;
    uint32_t totalRunTime;

    numTasks = uxTaskGetNumberOfTasks();
    taskStatusArray = pvPortMalloc(numTasks * sizeof(TaskStatus_t));

    if (taskStatusArray == NULL)
    {
        printf("Memory allocation failed\r\n");
        return;
    }

    numTasks = uxTaskGetSystemState(taskStatusArray, numTasks, &totalRunTime);

    printf("\r\n");
    printf("==============================================\r\n");
    printf("        CPU USAGE REPORT\r\n");
    printf("==============================================\r\n");
    printf("Total CPU Load: %u%%\r\n", totalCPULoad);
    printf("----------------------------------------------\r\n");
    printf("%-12s %5s %6s %8s %6s\r\n", "Task", "State", "Prio", "Stack", "CPU%");
    printf("----------------------------------------------\r\n");

    for (UBaseType_t i = 0; i < numTasks; i++)
    {
        char stateChar;
        uint8_t cpuPercent = 0;

        // Get task state
        switch (taskStatusArray[i].eCurrentState)
        {
            case eRunning:   stateChar = 'X'; break;
            case eReady:     stateChar = 'R'; break;
            case eBlocked:   stateChar = 'B'; break;
            case eSuspended: stateChar = 'S'; break;
            case eDeleted:   stateChar = 'D'; break;
            default:         stateChar = '?'; break;
        }

        // Find CPU percentage for this task
        for (uint8_t j = 0; j < MAX_TASKS; j++)
        {
            if (strcmp(taskCPUInfo[j].name, taskStatusArray[i].pcTaskName) == 0)
            {
                cpuPercent = taskCPUInfo[j].cpuPercent;
                break;
            }
        }

        printf("%-12s %5c %6lu %8u %5u%%\r\n",
               taskStatusArray[i].pcTaskName,
               stateChar,
               taskStatusArray[i].uxCurrentPriority,
               (unsigned int)taskStatusArray[i].usStackHighWaterMark,
               cpuPercent);
    }

    printf("----------------------------------------------\r\n");
    printf("State: X=Running, R=Ready, B=Blocked, S=Suspended\r\n");
    printf("Stack: High water mark (words remaining)\r\n");
    printf("==============================================\r\n\r\n");

    vPortFree(taskStatusArray);
}

/*---------------------------------------------------------------------------*/
/* CPU Monitor Task                                                          */
/*---------------------------------------------------------------------------*/

void vCPUMonitorTask(void const * argument)
{
    // Clear task info array
    memset(taskCPUInfo, 0, sizeof(taskCPUInfo));

    // Let system stabilize
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Initialize baseline
    CalculateCPULoad();

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(2000));  // Update every 2 seconds

        CalculateCPULoad();
        PrintCPUStats();
    }
}
