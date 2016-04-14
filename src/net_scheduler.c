#include "net_scheduler.h"
#include "net_list.h"

#if defined(LINUX_ENV)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#define SCHED_PRINTF printf
#define MALLOC malloc
#define FREE free
#define MEMSET	memset
#define closesocket	close
#elif defined(PLATFORM_RT_THREAD)
#include <rtthread.h>
#include <lwip/sockets.h>
#define SCHED_PRINTF rt_kprintf
#define MALLOC UT_MALLOC
#define FREE UT_FREE
#define MEMSET UT_MEMSET
#endif

typedef struct _DelayTaskT
{
    SchedProcT proc;
    void *clientData;
    SchedProcT cleanUp;
    unsigned int timeoutTick;
    unsigned int msec;
    unsigned int flag;

    // DelayTasks are linked together in a doubly-linked list:
    ListNodeT listEntry;
} DelayTaskT;

typedef struct _HandlerDescriptorT
{
    int sock;
    SchedProcT handlerProc;
    void *clientData;
    SchedProcT cleanUp;

    // Descriptors are linked together in a doubly-linked list:
    ListNodeT listEntry;
} HandlerDescriptorT;

struct _SchedulerT
{
    ListNodeT delayQHead;
    ListNodeT handlerQHead;

    int lastHandledSock;
    int maxNumSocks;
    fd_set readSet;
    int enableIPC; // bool var
    unsigned short ipcPort; // host byte order
};

typedef struct _SchedIpcDataT
{
    unsigned int magicId;
    SchedulerT *scheduler;
    unsigned int msec;
    SchedProcT task;
    void *clientData;
    SchedProcT cleanUp;
} SchedIpcDataT;

enum SchedIpcCmdE
{
    SCHED_CMD_DELAY,
    SCHED_CMD_UNDELAY,
    SCHED_CMD_HANDLE_READ,
    SCHED_CMD_UNHANDLE_READ,
};

typedef struct _SchedIpcMsgT
{
    unsigned int magicId;
    enum SchedIpcCmdE type;
    int ret; // useless for now
    void *param0;
    void *param1;
    void *param2;
    void *param3;
    void *param4;
    void *param5;
    void *param6;
} SchedIpcMsgT;


#define SCHEDULER_TICK_MAX 0xffffffff // Maxium number of UINT32
#define SCHEDULER_IPC_MSG_MAGIC_ID 0x01DADA10

static unsigned int PlatformGetTick(void);
static DelayTaskT* FindDelayTask(SchedulerT *scheduler, int token);
static void AddDelayTask(SchedulerT *scheduler, DelayTaskT *task);
static int HandleTimeout(SchedulerT *scheduler);
static HandlerDescriptorT* LookupHandler(SchedulerT *scheduler, int sock);
static int AddIpcHandler(SchedulerT *scheduler);
static void IpcHandler(void *data);


// This function should return msec tick
static unsigned int PlatformGetTick(void)
{
#if defined(WIN32)
    return (unsigned int)GetTickCount();
#endif // WIN32

#if defined(LINUX_ENV)
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_sec*1000 + tv.tv_usec/1000;
#endif // LINUX_ENV

#if defined(PLATFORM_RT_THREAD)
    return (unsigned int)rt_tick_get()*10;
#endif

}

/**
 * @brief Create a scheduler
 *
 * @param [out] pScheduler get a scheduler
 * @param [in] param parameters for scheduler
 * @return status code
 */
int scheduler_open(SchedulerT **pScheduler, SchedulerParamT *param)
{
    SchedulerT *scheduler;

    scheduler = (SchedulerT *)MALLOC(sizeof(SchedulerT));
    if (scheduler == NULL)
    {
        return ERR_SCHEDULER_UNKNOWN;
    }

    list_init(&scheduler->delayQHead);
    list_init(&scheduler->handlerQHead);
    scheduler->maxNumSocks = 0;
    scheduler->lastHandledSock = -1;
    FD_ZERO(&scheduler->readSet);
    scheduler->enableIPC = 0;
    scheduler->ipcPort = 0;

    if (param != NULL)
    {
        scheduler->enableIPC = param->enableIPC;
        scheduler->ipcPort = param->ipcPort;
    }
    if (scheduler->enableIPC)
    {
        // use IPC Interface, add a socket for IPC handling
        AddIpcHandler(scheduler);
    }

    *pScheduler = scheduler;
    return ERR_SCHEDULER_OK;

}

/**
 * @brief Destroy a scheduler
 *
 * @param [in, out] pScheduler [in] a scheduler get from scheduler_open(), [out] should be set to NULL if succeed
 * @return status code
 */
int scheduler_close(SchedulerT **pScheduler)
{
    ListNodeT *entry;
    HandlerDescriptorT *handler;
    DelayTaskT *task;
    SchedulerT *scheduler = *pScheduler;

    entry = scheduler->handlerQHead.next;
    while (entry != &scheduler->handlerQHead)
    {
        handler = list_entry(entry, HandlerDescriptorT, listEntry);
        entry = entry->next;
        scheduler_unhandle_read(scheduler, handler->sock);
    }

    entry = scheduler->delayQHead.next;
    while (entry != &scheduler->delayQHead)
    {
        task = list_entry(entry, DelayTaskT, listEntry);
        entry = entry->next;
        scheduler_undelay_task(scheduler, (int)task);
    }

    FREE(scheduler);
    *pScheduler = NULL;

    return ERR_SCHEDULER_OK;

}

/**
 * @brief Add a socket read event Handler into the scheduler
 * When a socket read event occurs, the Handler function will be called
 *
 * @param [in] scheduler the scheduler get from scheduler_open()
 * @param [in] sock the socket descriptor
 * @param [in] handlerProc the Handler function
 * @param [in] clientData specific data passed to Handler function and Cleanup function
 * @param [in] cleanUp the Cleanup function for doing cleanup job
 * @return status code
 */
int scheduler_handle_read(SchedulerT *scheduler, int sock, \
                          SchedProcT handlerProc, void *clientData, SchedProcT cleanUp)
{
    HandlerDescriptorT *hd;

    if (sock < 0) return ERR_SCHEDULER_UNKNOWN;

    hd = LookupHandler(scheduler, sock);
    if (hd == NULL)
    {
        hd = (HandlerDescriptorT *)MALLOC(sizeof(HandlerDescriptorT));
        if (hd == NULL)
        {
            return ERR_SCHEDULER_UNKNOWN;
        }
        hd->sock = sock;

        // Link this descriptor into a doubly-linked list:
        list_insert_before(&scheduler->handlerQHead, &hd->listEntry);

        FD_SET(sock, &scheduler->readSet);
        if (sock+1 > scheduler->maxNumSocks)
        {
            scheduler->maxNumSocks = sock + 1;
        }
    }
    hd->handlerProc = handlerProc;
    hd->clientData = clientData;
    hd->cleanUp = cleanUp;

    return ERR_SCHEDULER_OK;

}

/**
 * @brief Delete a socket read event Handler from the scheduler
 *
 * @param [in] scheduler the scheduler get from scheduler_open()
 * @param [in] sock the socket descriptor we want to delete
 * @return status code
 */
int scheduler_unhandle_read(SchedulerT *scheduler, int sock)
{
    HandlerDescriptorT *hd;

    hd = LookupHandler(scheduler, sock);
    if (hd == NULL)
    {
        return ERR_SCHEDULER_DESCRIPTOR_NOT_FOUND;
    }

    list_remove(&hd->listEntry);

    // remove from socket set
    FD_CLR(sock, &scheduler->readSet);
    if (sock+1 == scheduler->maxNumSocks)
    {
        scheduler->maxNumSocks--;
    }

    if (hd->cleanUp != NULL)
    {
        hd->cleanUp(hd->clientData);
    }
    FREE(hd);

    return ERR_SCHEDULER_OK;

}

/**
 * @brief Add a Task into the scheduler that will be executed in specified delay time
 *
 * @param [in] scheduler the scheduler get from scheduler_open()
 * @param [in] msec delay time in microsecond
 * @param [in] proc the delayed Task function
 * @param [in] clientData specific data passed to the Task function and Cleanup function
 * @param [in] cleanUp the Cleanup function for doing cleanup job
 * @return a unique token number identifying the delayed Task
 */
int scheduler_delay_task(SchedulerT *scheduler, unsigned int msec, unsigned int flag, SchedProcT proc, void *clientData, SchedProcT cleanUp)
{
    DelayTaskT *task;

    if (msec > SCHEDULER_TICK_MAX/2)
    {
        return ERR_SCHEDULER_UNKNOWN;
    }

    // Make up a DelayTask
    task = (DelayTaskT *)MALLOC(sizeof(DelayTaskT));
    if (task == NULL)
    {
        return ERR_SCHEDULER_UNKNOWN;
    }
    
    task->proc = proc;
    task->clientData = clientData;
    task->cleanUp = cleanUp;
    task->timeoutTick = PlatformGetTick() + msec;
    task->msec = msec;
    task->flag = flag;

    // Add task to the queue
    AddDelayTask(scheduler, task);
    return (int)task;
}

/**
 * @brief Delete a delayed Task from the scheduler
 *
 * @param [in] scheduler the scheduler get from scheduler_open()
 * @param [in] token the token number identifying the delayed Task
 * @return remain time
 */
int scheduler_undelay_task(SchedulerT *scheduler, int token)
{
    DelayTaskT *task;
    unsigned int remainTick;

    task = FindDelayTask(scheduler, token);
    if (task != NULL)
    {
        remainTick = PlatformGetTick();
        if (remainTick >= task->timeoutTick)
        {
            remainTick = 0;
        }
        else
        {
            remainTick = task->timeoutTick - remainTick;
        }
        
        list_remove(&task->listEntry);
        if (task->cleanUp != NULL)
        {
            (*task->cleanUp)(task->clientData);
        }
        FREE(task);
        return (int)remainTick;
    }

    return ERR_SCHEDULER_TASK_NOT_FOUND;
}

/**
 * @brief Same as scheduler_delay_task() interface but being used in IPC case
 *
 * @param [in] scheduler the scheduler get from scheduler_open()
 * @param [in] msec delay time in microsecond
 * @param [in] proc the delayed Task function
 * @param [in] clientData specific data passed to the Task function and Cleanup function
 * @param [in] cleanUp the Cleanup function for doing cleanup job
 * @return status code
 */
int scheduler_delay_task_remote(SchedulerT *scheduler, unsigned int msec, unsigned int flag, SchedProcT proc, void *clientData, SchedProcT cleanUp)
{
    int ret;
    SchedIpcMsgT ipcMsg;
    int sock;
    struct sockaddr_in dest;

    if (scheduler && !scheduler->enableIPC)   // this scheduler doesn't enable IPC Interface
    {
        return ERR_SCHEDULER_UNKNOWN;
    }

    ipcMsg.magicId = SCHEDULER_IPC_MSG_MAGIC_ID;
    ipcMsg.type = SCHED_CMD_DELAY;
    ipcMsg.param0 = (void*)scheduler;
    ipcMsg.param1 = (void*)msec;
    ipcMsg.param2 = (void*)flag;
    ipcMsg.param3 = (void*)proc;
    ipcMsg.param4 = (void*)clientData;
    ipcMsg.param5 = (void*)cleanUp;

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) return ERR_SCHEDULER_SOCKET;

    MEMSET(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    dest.sin_port = htons(scheduler->ipcPort);

    ret = sendto(sock, &ipcMsg, sizeof(ipcMsg), 0, (struct  sockaddr  *)&dest, sizeof(dest));
    if (ret < 0)
    {
        SCHED_PRINTF("[Scheduler] scheduler_delay_task_remote()->sendto() failed! %d\n", ret);
        closesocket(sock);
        return ERR_SCHEDULER_SOCKET;
    }

    closesocket(sock);
    return ERR_SCHEDULER_OK;
}

/**
 * @brief Single step of the scheduler
 * Currently we execute the scheduler in loop statements manually, probably change this
 * behaviour later
 *
 * @param [in] scheduler the scheduler get from scheduler_open()
 * @param [in] defaultMsec default idle time interval
 * @return status code
 */
int scheduler_single_step(SchedulerT *scheduler, unsigned int defaultMsec)
{
    int ret;
    ListNodeT *entry;
    HandlerDescriptorT *hd;
    DelayTaskT *task;
    struct timeval timeToDelay;
    unsigned int currentTick;
    fd_set readSet;

    // Very large "tv_sec" values cause select() to fail.
    // Don't make it any larger than 1 million seconds (11.5 days)
    const unsigned int MAX_MSEC = 1000000000;
    if (defaultMsec > MAX_MSEC)
    {
        defaultMsec = MAX_MSEC;
        SCHED_PRINTF("[Scheduler] timeToDelay larger than 1 million seconds!\n");
    }

    if (list_isempty(&scheduler->delayQHead))
    {
        // Empty DelayTask queue, use default timeout value for select()
        timeToDelay.tv_sec = defaultMsec/1000;
        timeToDelay.tv_usec = defaultMsec%1000 *1000;
    }
    else
    {
        entry = scheduler->delayQHead.next;
        task = list_entry(entry, DelayTaskT, listEntry);
        currentTick = PlatformGetTick();
        // It supposes that task delay time should be less than the half duration of tick max
        if (currentTick - task->timeoutTick < SCHEDULER_TICK_MAX/2)
        {
            // DelayTask have come due
            timeToDelay.tv_sec = 0;
            timeToDelay.tv_usec = 0;
        }
        else
        {
            // DealyTask haven't come due, caculate timeout value
            timeToDelay.tv_sec = (task->timeoutTick - currentTick)/1000;
            timeToDelay.tv_usec = (task->timeoutTick - currentTick)%1000 * 1000;
        }
    }

    readSet = scheduler->readSet;
    ret = select(scheduler->maxNumSocks, &readSet, NULL, NULL, &timeToDelay);
    if (ret < 0)
    {
        SCHED_PRINTF("[Scheduler] Socket select() error...\n");
        return -1;
    }

    // Call the handler function for one readable socket;
    entry = scheduler->handlerQHead.next;
    // To ensure forward progress through the handlers, begin past the last
    // socket number that we handled:
    if (scheduler->lastHandledSock >= 0)
    {
        hd = LookupHandler(scheduler, scheduler->lastHandledSock);
        if (hd != NULL)
        {
            entry = hd->listEntry.next;
        }
        else
        {
            scheduler->lastHandledSock = -1;
        }
    }

    while (entry != &scheduler->handlerQHead)
    {
        hd = list_entry(entry, HandlerDescriptorT, listEntry);
        if (FD_ISSET(hd->sock, &readSet) && hd->handlerProc != NULL)
        {
            scheduler->lastHandledSock = hd->sock;
            // Note: we set "lastHandledSock" before calling the handler
            (*hd->handlerProc)(hd->clientData);
            break;
        }
        entry = entry->next;
    }

    if (entry == &scheduler->handlerQHead && scheduler->lastHandledSock >= 0)
    {
        // We didn't call a handler, but we didn't get to check all of them,
        // so try again from the beginning:
        entry = scheduler->handlerQHead.next;
        while (entry != &scheduler->handlerQHead )
        {
            hd = list_entry(entry, HandlerDescriptorT, listEntry);
            if (FD_ISSET(hd->sock, &readSet) && hd->handlerProc != NULL)
            {
                scheduler->lastHandledSock = hd->sock;
                // Note: we set "lastHandledSock" before calling the handler
                (*hd->handlerProc)(hd->clientData);
                break;
            }
            entry = entry->next;
        }
    }

    if (entry == &scheduler->handlerQHead)
    {
        // We didn't call a handler
        scheduler->lastHandledSock = -1;
        if (ret > 0)
        {
            // It should be impossible for the scheduler to run here
            SCHED_PRINTF("[Scheduler] Scheduler have problem!\n");
        }
    }

    // Also handle any DelayTask that may have come due.  (Note that we do this *after* calling a socket
    // handler, in case the DelayTask handler modifies the set of readable socket.)
    HandleTimeout(scheduler);

    return ERR_SCHEDULER_OK;

}

static HandlerDescriptorT* LookupHandler(SchedulerT *scheduler, int sock)
{
    ListNodeT *entry;
    HandlerDescriptorT *handler;

    entry = scheduler->handlerQHead.next;
    while (entry != &scheduler->handlerQHead)
    {
        handler = list_entry(entry, HandlerDescriptorT, listEntry);
        if (handler->sock == sock)
        {
            return handler;
        }
        entry = entry->next;
    }

    return NULL;

}

static DelayTaskT* FindDelayTask(SchedulerT *scheduler, int token)
{
    ListNodeT *entry;
    DelayTaskT *task;

    entry = scheduler->delayQHead.next;
    while (entry != &scheduler->delayQHead)
    {
        task = list_entry(entry, DelayTaskT, listEntry);
        if ((int)task == token)
        {
            return task;
        }
        entry = entry->next;
    }

    return NULL;
}

static void AddDelayTask(SchedulerT *scheduler, DelayTaskT *task)
{
    ListNodeT *entry;
    DelayTaskT *curTask;
    
    // Add task to the Delay List
    entry = scheduler->delayQHead.next;
    while (entry != &scheduler->delayQHead)
    {
        curTask = list_entry(entry, DelayTaskT, listEntry);
        if (curTask->timeoutTick - task->timeoutTick != 0 && \
            curTask->timeoutTick - task->timeoutTick < SCHEDULER_TICK_MAX/2)
        {
            list_insert_before(entry, &task->listEntry);
            break;
        }
        entry = entry->next;
    }

    // Insert into tail
    if (entry == &scheduler->delayQHead)
    {
        list_insert_before(entry, &task->listEntry);
    }
}

static int HandleTimeout(SchedulerT *scheduler)
{
    ListNodeT *entry;
    DelayTaskT *task;
    unsigned currentTick;

    if (list_isempty(&scheduler->delayQHead))
    {
        return ERR_SCHEDULER_DELAYQ_EMPTY;
    }

    entry = scheduler->delayQHead.next;
    while (entry != &scheduler->delayQHead)
    {
        task = list_entry(entry, DelayTaskT, listEntry);
        entry = entry->next;
        currentTick = PlatformGetTick();
        // It supposes that task delay time should be less than the half duration of tick max
        if (currentTick - task->timeoutTick < SCHEDULER_TICK_MAX/2)
        {
            // This DelayTask is due to be handled:
            if (task->flag == DELAYTASK_FLAG_ONESHOT)
            {
                list_remove(&task->listEntry); // do this first, in case handler accesses queue
                if (task->proc) (*task->proc)(task->clientData);
                if (task->cleanUp) (*task->cleanUp)(task->clientData);
                FREE(task);                
            }
            else
            {
                list_remove(&task->listEntry); // do this first, in case handler accesses queue
                if (task->proc) (*task->proc)(task->clientData);
                task->timeoutTick += task->msec;
                AddDelayTask(scheduler, task);
            }
        }
        else
        {
            break;
        }
    }

    return ERR_SCHEDULER_OK;
}

static int AddIpcHandler(SchedulerT *scheduler)
{
    int ret;
    int sock;
    struct sockaddr_in addr;

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) return ERR_SCHEDULER_SOCKET;

    MEMSET(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(scheduler->ipcPort);

    ret = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    if (ret < 0)
    {
        SCHED_PRINTF("[Scheduler] IpcHandler()->bind() error! %d\n", ret);
        closesocket(sock);
        return ERR_SCHEDULER_SOCKET;
    }

    scheduler_handle_read(scheduler, sock, IpcHandler, (void *)sock, NULL);
    return ERR_SCHEDULER_OK;
}

static void IpcHandler(void *data)
{
    int ret;
    struct sockaddr_in fromAddr;
    socklen_t fromAddrLen = sizeof(fromAddr);
    int sock = (int)data;
    SchedIpcMsgT ipcMsg;

    if (sock < 0) return;
    ret = recvfrom(sock, &ipcMsg, sizeof(ipcMsg), 0, (struct sockaddr *)&fromAddr, &fromAddrLen);
    if (ret < 0 || ret != (int)sizeof(ipcMsg))   // Error happened, just return
    {
        SCHED_PRINTF("[Scheduler] IpcHandler()->recvfrom() %d!\n", ret);
        return;
    }

    if (ipcMsg.magicId != SCHEDULER_IPC_MSG_MAGIC_ID)   // Error data, just return
    {
        SCHED_PRINTF("[Scheduler] IPC data error!\n");
        return;
    }

    switch (ipcMsg.type)
    {
        case SCHED_CMD_DELAY:
        {
            SchedulerT *scheduler = (SchedulerT *)ipcMsg.param0;
            unsigned int msec = (unsigned int)ipcMsg.param1;
            unsigned int flag = (unsigned int)ipcMsg.param2;
            SchedProcT proc = (SchedProcT)ipcMsg.param3;
            void *clientData = (void*)ipcMsg.param4;
            SchedProcT cleanUp = (SchedProcT)ipcMsg.param5;
            ipcMsg.ret = scheduler_delay_task(scheduler, msec, flag, proc, clientData, cleanUp);
            break;
        }
        case SCHED_CMD_UNDELAY:
        {
            SchedulerT *scheduler = (SchedulerT *)ipcMsg.param0;
            int token = (int)ipcMsg.param1;
            ipcMsg.ret = scheduler_undelay_task(scheduler, token);
            break;
        }
        case SCHED_CMD_HANDLE_READ:
        {
            SchedulerT *scheduler = (SchedulerT *)ipcMsg.param0;
            int sock = (int)ipcMsg.param1;
            SchedProcT handlerProc = (SchedProcT)ipcMsg.param2;
            void *clientData = (void*)ipcMsg.param3;
            SchedProcT cleanUp = (SchedProcT)ipcMsg.param4;
            ipcMsg.ret = scheduler_handle_read(scheduler, sock, handlerProc, clientData, cleanUp);
            break;
        }
        case SCHED_CMD_UNHANDLE_READ:
        {
            SchedulerT *scheduler = (SchedulerT *)ipcMsg.param0;
            int sock = (int)ipcMsg.param1;
            ipcMsg.ret = scheduler_unhandle_read(scheduler, sock);
            break;
        }
        default:
            break;
    }

}


