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

#include "config.h"
#include "net_service.h"
#include "net_server.h"
#include "net_comm.h"
#include "cJSON.h"

cJSON *service_test1(cJSON *data)
{
    printf("service_test1\n");

    return NULL;
}

cJSON *service_test2(cJSON *data)
{
    printf("service_test2\n");

    return NULL;
}

int test_function2(void *param)
{
    int ret = -1;
    ServerT *server;

    service_init();
    service_register("test1", &service_test1, NULL);
    service_register("test2", &service_test2, NULL);

    ret = server_init();
    if (ret < 0)
    {
        printf("server err...\n");
        return ret;
    }
    ret = server_open(&server, 0);
    if (ret < 0)
    {
        printf("server err...\n");
        return ret;
    }

    printf("server start...\n");
    server_start(server);

    server_close(&server);
    return ret;
}




int main(void)
{
    //char *buf = "{ \"abc\":2, {   ";
    //const char *end;
    //const char *cur;
    //cJSON *root;
    //root = cJSON_ParseWithOpts(buf, &end, 0);
    //cur = cJSON_GetErrorPtr();

    test_function2(NULL);
    return 0;
}


