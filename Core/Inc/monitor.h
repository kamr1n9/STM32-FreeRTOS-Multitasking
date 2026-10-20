#ifndef CPU_MONITOR_H
#define CPU_MONITOR_H

#include <stdint.h>

void vCPUMonitorTask(void const * argument);
void vConfigureTimerForRunTimeStats(void);
uint32_t ulGetRunTimeCounterValue(void);

#endif
