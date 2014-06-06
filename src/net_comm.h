#ifndef __NET_COMM_H__
#define __NET_COMM_H__

#include <stdlib.h>
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

cJSON *recv_request_response(int sock);
void send_response(int sock, cJSON *res);

#ifdef __cplusplus
}
#endif

#endif //__NET_COMM_H__

