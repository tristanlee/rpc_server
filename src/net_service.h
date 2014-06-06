#ifndef __SERVICE_H__
#define __SERVICE_H__

#include <stdlib.h>
#include "cJSON.h"
#include "net_list.h"

// service return code
enum
{
    SERVICE_RET_OK = 0,
    SERVICE_RET_UNKNOWN,
    SERVICE_RET_INVALID,
    SERVICE_RET_NOT_FOUND,
    SERVICE_RET_MAX
};

typedef cJSON* (*ServiceProcT)(cJSON *params);

typedef struct _ServiceT
{
    char *name;
    ServiceProcT proc;
    void *data;
    ListNodeT listEntry;
} ServiceT;

int service_init(void);
int service_register(char *name, ServiceProcT proc, void *data);
int service_deregister(char *name);
cJSON *service_invoke(cJSON *root);

#endif //__SERVICE_H__
