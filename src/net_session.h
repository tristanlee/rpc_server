#ifndef __NET_SESSION_H__
#define __NET_SESSION_H__

#define PACKET_HEADER_LEN   2
#define SESSION_BUFFER_SIZE 1024

typedef struct _SessionT {
	int sock;
	int sid;
	ListNodeT listEntry;
	void *ourServer;
	unsigned int reqBufPos;
	unsigned int packetLen;
	char requestBuf[SESSION_BUFFER_SIZE];
    char *response;
} SessionT;


#ifdef __cplusplus
extern "C" {
#endif


int session_open(SessionT **pClient, void *ourServer, int sock);
int session_close(SessionT **pClient);




#ifdef __cplusplus
}
#endif

#endif //__NET_SESSION_H__

