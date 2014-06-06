
#include "config.h"
#include "net_list.h"
#include "net_service.h"

ListNodeT service_list;

static struct
{
    int retCode;
    char *desc;
} ret_code_table[] =
{
    {-SERVICE_RET_OK, "Call Suceeded"},
    {-SERVICE_RET_UNKNOWN, "Unknown Error"},
    {-SERVICE_RET_INVALID, "Call Invalid"},
    {-SERVICE_RET_NOT_FOUND, "Call Not Found"}
};

static ServiceT *service_find(char *name);
static cJSON *service_generate_response(int retCode);

int service_init(void)
{
    list_init(&service_list);
    return 0;
}

int service_register(char *name, ServiceProcT proc, void *data)
{
    if (!name || !proc) return -1;

    ServiceT *service = (ServiceT *)MALLOC(sizeof(ServiceT));
    if (!service) return -1;

    service->name = STRDUP(name);
    service->proc = proc;
    service->data = data;
    list_insert_before(&service_list, &service->listEntry);

    return 0;
}



int service_deregister(char *name)
{
    ServiceT *service;
    ListNodeT *entry;

    entry = service_list.next;
    while (entry != &service_list)
    {
        service = list_entry(entry, ServiceT, listEntry);
        if (!STRCMP(name, service->name))
        {
            list_remove(entry);
            FREE(service->name);
            FREE(service);
            return 0;
        }
        entry = entry->next;
    }

    return -1; // not found
}

cJSON *service_invoke(cJSON *root)
{
    cJSON *res = NULL;
    cJSON *call, *function, *params;
    ServiceT *service;

    if (!root) return NULL;

    call = cJSON_GetObjectItem(root, "call");
    if (!call)
    {
        DPRINTF("Invalid request -1 !\n");
        return service_generate_response(SERVICE_RET_INVALID);
    }
    function = cJSON_GetObjectItem(call, "function");
    if (!function)
    {
        DPRINTF("Invalid request call -1 !\n");
        return service_generate_response(SERVICE_RET_INVALID);
    }
    service = service_find(function->valuestring);
    if (!service)
    {
        DPRINTF("Invalid request call -2 !\n");
        return service_generate_response(SERVICE_RET_NOT_FOUND);
    }

    params = cJSON_GetObjectItem(call, "params");
    if (service->proc)
    {
        res = service->proc(params);
    }

    if (!res) return service_generate_response(SERVICE_RET_UNKNOWN);

    return res;

}

static cJSON *service_generate_response(int retCode)
{
    cJSON *root = NULL;
    cJSON *ret = NULL;

    if (retCode >= SERVICE_RET_MAX) return NULL;

    do
    {
        root = cJSON_CreateObject();
        if (!root) break;
        ret = cJSON_CreateObject();
        if (!ret) break;

        cJSON_AddItemToObject(root, "ret", ret);
        cJSON_AddNumberToObject(ret,"code", ret_code_table[retCode].retCode);
        cJSON_AddStringToObject(ret, "desc", ret_code_table[retCode].desc);
        return root;
    }
    while (0);

    if (root) cJSON_Delete(root);
    if (ret) cJSON_Delete(ret);
    return NULL;
}

static ServiceT *service_find(char *name)
{
    ServiceT *service;
    ListNodeT *entry;

    entry = service_list.next;
    while (entry != &service_list)
    {
        service = list_entry(entry, ServiceT, listEntry);
        if (!STRCMP(name, service->name))
        {
            return service;
        }
        entry = entry->next;
    }

    return NULL;
}

