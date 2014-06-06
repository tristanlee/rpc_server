#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>
#include "net_scheduler.h"
#include "net_comm.h"
#include "cJSON.h"

#define STRLEN	strlen
#define MALLOC	malloc
#define FREE	free

char gBuf[1024];
int test_function4(void *param)
{
    int ret = -1;
    int sockfd;
    struct sockaddr_in server;
    cJSON *root, *call, *params;
    unsigned short dataLen, netLen;
    char *out;

    sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        printf("socket() error...\n");
        return -1;
    }

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_port = htons(6000);

    printf("start connecting...\n");
    ret = connect(sockfd, (struct sockaddr*)&server, sizeof(server));
    if (ret < 0)
    {
        printf("connect error!\n");
        close(sockfd);
        return -1;
    }
    else
    {
        printf("connect() return %d\n", ret);
    }

    root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "call", call=cJSON_CreateObject());
    cJSON_AddStringToObject(call,"function", "test2");
    cJSON_AddItemToObject(call, "params", params=cJSON_CreateObject());
    cJSON_AddNumberToObject(params,"param1", 1080);
    cJSON_AddNumberToObject(params,"param1", 1080);
    out = cJSON_Print(root);
    dataLen = STRLEN(out);
    netLen = htons(dataLen);
    cJSON_Delete(root);

    ret = send(sockfd, &netLen, 2, 0);
    ret = send(sockfd, out, dataLen, 0);
    FREE(out);

    root = recv_request_response(sockfd);
    cJSON_Delete(root);

    sleep(10);
    close(sockfd);
    return 0;
}

int main(void)
{
    test_function4(NULL);
    return 0;
}


