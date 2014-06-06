
#include "config.h"
#include "cJSON.h"



cJSON *recv_request_response(int sock)
{
    int ret;
    char len[2];
    unsigned short dataLen, offset, left;
    cJSON *json;
    char *buf;

    recv(sock, &len[0], 1, 0);
    recv(sock, &len[1], 1, 0);
    dataLen = *(unsigned short *)len;
    dataLen = ntohs(dataLen);
    DPRINTF("data len: %d\n", dataLen);
    if (!dataLen) return NULL;
    buf = MALLOC(dataLen+1);
    if (!buf) return NULL;

    left = dataLen;
    offset = 0;
    do
    {
        ret = recv(sock, buf+offset, 1024, 0);
        if (ret < 0)
        {
            DPRINTF("recv() error!\n");
            break;
        }
        else if (ret == 0)
        {
            break;
        }

        offset += (unsigned short)ret;
        left -= (unsigned short)ret;
    }
    while (left);
    buf[dataLen] = 0; // end string
    DPRINTF("%s\n", buf);

    json = cJSON_Parse(buf);
    FREE(buf);
    return json;
}

void send_response(int sock, cJSON *res)
{
    cJSON *root, *ret;
    unsigned short dataLen, netLen;
    char *out;

    if (!res)
    {
        root = cJSON_CreateObject();
        cJSON_AddItemToObject(root, "ret", ret=cJSON_CreateObject());
        cJSON_AddNumberToObject(ret,"code", -1);
        cJSON_AddStringToObject(ret, "desc", "Invalid call");
        out = cJSON_Print(root);
        dataLen = STRLEN(out);
        netLen = htons(dataLen);
        cJSON_Delete(root);

        send(sock, &netLen, 2, 0);
        send(sock, out, dataLen, 0);
        FREE(out);
        return;
    }

    out = cJSON_Print(res);
    dataLen = STRLEN(out);
    netLen = htons(dataLen);
    cJSON_Delete(res);
    send(sock, &netLen, 2, 0);
    send(sock, out, dataLen, 0);
    FREE(out);

}

