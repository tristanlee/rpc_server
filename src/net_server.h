#ifndef __NET_SERVER_H__
#define __NET_SERVER_H__

#include "net_scheduler.h"
#include "net_list.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _ServerT
{
    int sock;
    int udpSock;
    unsigned short port; // host order byte
    SchedulerT *scheduler;
    ListNodeT clientList;
    unsigned int clientNum;
} ServerT;

ServerT* server_get(void);
int server_init(void);
int server_open(ServerT **pServer, unsigned short port);
int server_start(ServerT *server);

#ifdef __cplusplus
}
#endif

#endif //__NET_SERVER_H__
