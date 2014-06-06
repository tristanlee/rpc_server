
#include "config.h"
#include "net_list.h"
#include "net_session.h"
#include "net_scheduler.h"
#include "net_server.h"
#include "net_service.h"
#include "cJSON.h"


static int session_cur_id = 0;

static int session_gen_id(void);
static void session_request_handler(SessionT *client);
static int session_send_response(SessionT *session, cJSON *res);
static void session_send(SessionT *session);
static int packet_get_len(char *header, unsigned int *len);


int session_open(SessionT **pClient, void *ourServer, int sock)
{
    SessionT *client;
    ServerT *server = (ServerT *)ourServer;

    if (sock < 0) return ERR_SOCKET;
    if (!server || server->clientNum >= SESSION_MAX_NUM) return ERR_UNKNOWN;
    client = MALLOC(sizeof(SessionT));
    if (!client) return ERR_MALLOC;

    client->ourServer = server;
    client->sock = sock;
    client->reqBufPos = 0;
    client->packetLen = 0;
    client->sid = session_gen_id();
    client->response = NULL;

    list_insert_before(&server->clientList, &client->listEntry);
    server->clientNum++;
    scheduler_handle_read(server->scheduler, sock, (SchedProcT)session_request_handler, client, NULL);

    *pClient = client;
    return ERR_OK;
}

int session_close(SessionT **pClient)
{
    SessionT *client = *pClient;
    ServerT *server = (ServerT *)client->ourServer;

    scheduler_unhandle_read(server->scheduler, client->sock);
    list_remove(&client->listEntry);
    server->clientNum--;
    closesocket(client->sock);
    if (client->response) FREE(client->response);
    FREE(client);
    *pClient = NULL;

    return ERR_OK;
}

static int session_send_response(SessionT *session, cJSON *res)
{
    ServerT *server;
    if (!session || !res) return ERR_UNKNOWN;

    server = (ServerT *)session->ourServer;
    if (session->response) FREE(session->response);
    session->response = cJSON_Print(res);
    return scheduler_delay_task_remote(server->scheduler, 0, (SchedProcT)session_send, session, NULL);
}

static void session_send(SessionT *session)
{
    unsigned short dataLen, netLen;

    if (!session || !session->response) return;

    dataLen = STRLEN(session->response);
    netLen = htons(dataLen);
    send(session->sock, &netLen, 2, 0);
    send(session->sock, session->response, dataLen, 0);
    FREE(session->response);
    session->response = NULL;
    return;
}

static int session_gen_id(void)
{
    session_cur_id = session_cur_id+1;
    return (session_cur_id);
}

static void session_request_handler(SessionT *client)
{
    struct sockaddr_in fromAddr;
    socklen_t fromAddrLen = sizeof(fromAddr);
    unsigned int pos, left, packetLen = 0;
    cJSON *root, *res;
    int ret;

    pos = client->reqBufPos;
    packetLen = client->packetLen;
    if (packetLen == 0)
    {
        // First we need to get the packet header
        left = PACKET_HEADER_LEN - pos;
        ret = recvfrom(client->sock, &client->requestBuf[pos], left, 0, (struct sockaddr *)&fromAddr, &fromAddrLen);
        if (ret <= 0)
        {
            // Close client session;
            session_close(&client);
            return;
        }
        client->reqBufPos += (unsigned int)ret;
        if (client->reqBufPos < PACKET_HEADER_LEN)
        {
            // We don't see the entire packet header, keep awaiting more data
            return;
        }

        ret = packet_get_len(client->requestBuf, &client->packetLen);
        if (ret != ERR_OK || client->packetLen >= SESSION_BUFFER_SIZE)
        {
            // Packet data too long, close client session;
            session_close(&client);
            return;
        }
        client->reqBufPos = 0;
    }
    else
    {
        // Now get packet data
        left = packetLen - pos;
        ret = recvfrom(client->sock, &client->requestBuf[pos], left, 0, (struct sockaddr *)&fromAddr, &fromAddrLen);
        if (ret <= 0)
        {
            // Close client session;
            session_close(&client);
            return;
        }
        client->reqBufPos += (unsigned int)ret;
        if (client->reqBufPos < packetLen)
        {
            // We don't see the entire packet, keep awaiting more data
            return;
        }

        // We've got the whole packet data;
        client->requestBuf[client->packetLen] = 0; // end the data string
        root = cJSON_Parse(client->requestBuf);
        if (!root)
        {
            // Json data error
            session_close(&client);
            return;
        }
        res = service_invoke(root);
        cJSON_Delete(root);
        session_send_response(client, res);
        cJSON_Delete(res);

        client->reqBufPos = 0;
        client->packetLen = 0;
    }
}

static int packet_get_len(char *header, unsigned int *len)
{
    unsigned short dataLen;

    dataLen = *(unsigned short *)header;
    dataLen = ntohs(dataLen);

    *len = dataLen;
    return ERR_OK;
}

