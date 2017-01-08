#ifndef CS550_MQ_H
#define CS550_MQ_H

#include <unistd.h>
#include <stddef.h> 
#include <pthread.h>


#define MAX 128*1014
#define CS550_NOMSG	0
#define MSGQ_LEN 5
#define MAX_PROCESSES 10
#define CS550_CBREGISTERED 1

struct Msg {
    size_t size;
    size_t Recvcount;
    int present;
    char buf[MAX];
};

typedef struct Receiver
{
	size_t RecvID;
	size_t PID[MAX_PROCESSES];
	
}Receiverinfo;

struct Info {
    pthread_mutex_t mutex;
    pthread_cond_t condvar1;
    pthread_cond_t condvar2;
    int n, in, out, process_count;
    struct Msg msgs[MSGQ_LEN];
    Receiverinfo receivers;
};

struct cs550_MQ {
    struct Info *infop;
    const char *path;
    size_t len;
    int msg_in, msg_out;
};

int cs550_MQOpen(struct cs550_MQ *, const char *path, int queue_length);
int cs550_MQClose(struct cs550_MQ *);
int cs550_MQSend(struct cs550_MQ *, const char *msg, size_t size);
ssize_t cs550_MQRecv(struct cs550_MQ *, char *msg, size_t size);
ssize_t cs550_MQPeek(struct cs550_MQ *);
int cs550_MQNotify(struct cs550_MQ *, void (*cb)(const char *msg, size_t sz));



#endif
