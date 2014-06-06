
#include "config.h"
#include "net_list.h"
#include "net_scheduler.h"
#include "net_session.h"
#include "net_server.h"

ServerT *net_server = NULL;
static void server_connection_handler(ServerT *server);

ServerT* server_get(void)
{
    return net_server;
}

int server_init(void)
{
#if defined(WIN32)
    WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) exit(-1);
#endif
    return 0;
}

int server_open(ServerT **pServer, unsigned short port)
{
    int sock, ret;
    ServerT *server;
    struct sockaddr_in addr;
    SchedulerParamT param;

    server = MALLOC(sizeof(ServerT));
    if (server == NULL)
    {
        return ERR_MALLOC;
    }

    if (port == 0)
    {
        // Use default port number;
        port = SERVER_PORT;
    }

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0)
    {
        return -1;
    }

    MEMSET(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    ret = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    if (ret < 0)
    {
        closesocket(sock);
        return -1;
    }

    ret = listen(sock, 10);
    if (ret < 0)
    {
        closesocket(sock);
        return -1;
    }

    server->sock = sock;
    server->udpSock = -1;
    server->port = port;
    server->clientNum = 0;
    list_init(&(server->clientList));
    param.enableIPC = 1;
    param.ipcPort = SCHEDULER_DEFAULT_IPC_PORT;
    scheduler_open(&server->scheduler, &param);

    scheduler_handle_read(server->scheduler, sock, (SchedProcT)server_connection_handler, server, NULL);

    *pServer = server;
    return 0;
}

int server_start(ServerT *server)
{
    while (1)
    {
        scheduler_single_step(server->scheduler, 200);
    }

    return 0;
}

SessionT* server_find_session(ServerT *server, int sid)
{
    ListNodeT *entry;
    SessionT *client;

    entry = server->clientList.next;
    while (entry != &server->clientList)
    {
        client = list_entry(entry, SessionT, listEntry);
        if (client->sid == sid)
        {
            return client;
        }
        entry = entry->next;
    }

    return NULL;
}

static void server_connection_handler(ServerT *server)
{
    int clientSock, ret;
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    SessionT *client;

    clientSock = accept(server->sock, (struct sockaddr *)&clientAddr, &clientAddrLen);
    if (clientSock < 0)
    {
        //DPRINTF("accept() failed\n");
        return;
    }

    ret = session_open(&client, server, clientSock);
    if (ret != ERR_OK)
    {
        DPRINTF("session_open() failed! %d\n", ret);
        closesocket(clientSock);
        return;
    }
}



