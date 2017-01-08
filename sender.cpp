extern "C" {
#include "MQ.h"
}
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define NO_MSG 25

int main() {

    int rv, pid;
    cs550_MQ mq;

    // rv = unlink("/tmp/mq");
    // MSGQ_LEN is #defined in MQ.h currently set to 5
    
    rv = cs550_MQOpen(&mq, "mq", MSGQ_LEN); assert(rv == 0);
    pid = (int) getpid();
    for (int i = 0; i < NO_MSG; i++) {
        char buf[100];
        sprintf(buf, "Hello %d from process %d!", i,pid);
        printf ("Sending msg %d from process %d!\n", i, pid);
        rv = cs550_MQSend(&mq, buf, strlen(buf));
        sleep(1);
    }

    rv = cs550_MQClose(&mq); assert(rv == 0);
}
