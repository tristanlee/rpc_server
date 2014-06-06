#ifndef __CONFIG_H__
#define __CONFIG_H__

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
#define DPRINTF printf
#define MALLOC malloc
#define FREE free
#define MEMSET	memset
#define STRLEN	strlen
#define STRDUP	strdup
#define STRCMP	strcmp
#define closesocket	close
#elif defined(WIN32)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <WinSock2.h>
#pragma comment (lib,"Ws2_32.lib")
#define DPRINTF printf
#define MALLOC malloc
#define FREE free
#define MEMSET	memset
#define STRLEN	strlen
#define STRDUP	strdup
#define STRCMP	strcmp
#elif defined(PLATFORM_RT_THREAD)
#include <rtthread.h>
#include <lwip/sockets.h>
#define SCHED_PRINTF rt_kprintf
#define MALLOC UT_MALLOC
#define FREE UT_FREE
#define MEMSET UT_MEMSET
#endif

#define SERVER_PORT 6000
#define SESSION_MAX_NUM	4


enum
{
	ERR_OK = 0,
	ERR_UNKNOWN = -1,
	ERR_MALLOC = -2,
	ERR_SOCKET = -3,

};

#endif //__CONFIG_H__
