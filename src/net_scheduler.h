#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__


#ifdef __cplusplus
extern "C" {
#endif

typedef void (*SchedProcT)(void* clientData);

typedef struct _SchedulerT SchedulerT;
typedef struct _SchedulerParamT
{
    int enableIPC; // bool var
    unsigned short ipcPort; // host order byte
} SchedulerParamT;

// Scheduler Interfaces:
int scheduler_open(SchedulerT **pScheduler, SchedulerParamT *param);
int scheduler_close(SchedulerT **pScheduler);
int scheduler_single_step(SchedulerT *scheduler, unsigned int defaultMsec);

// Delay Task Interfaces:
int scheduler_delay_task(SchedulerT *scheduler, unsigned int msec, SchedProcT proc, void *clientData, SchedProcT cleanUp);
int scheduler_undelay_task(SchedulerT *scheduler, int token);
int scheduler_delay_task_remote(SchedulerT *scheduler, unsigned int msec, SchedProcT proc, void *clientData, SchedProcT cleanUp);

// Socket Event Handler Interfaces:
int scheduler_handle_read(SchedulerT *scheduler, int sock, \
                          SchedProcT handlerProc, void *clientData, SchedProcT cleanUp);
int scheduler_unhandle_read(SchedulerT *scheduler, int sock);

#define SCHEDULER_DEFAULT_IPC_PORT	7777
// Error code
#define ERR_SCHEDULER_OK		(0)
#define ERR_SCHEDULER_DELAYQ_EMPTY		(101)
#define ERR_SCHEDULER_UNKNOWN		(-100)
#define ERR_SCHEDULER_TASK_NOT_FOUND		(-101)
#define ERR_SCHEDULER_DESCRIPTOR_NOT_FOUND		(-102)
#define ERR_SCHEDULER_SOCKET	(-103)

#ifdef __cplusplus
}
#endif

#endif // __SCHEDULER_H__

