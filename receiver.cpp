extern "C" {
#include "MQ.h"
}
#include <assert.h>
#include <stdio.h>

int main() {

    int rv, i;
    cs550_MQ mq;

    // rv = unlink("/tmp/mq");
    // MSGQ_LEN is #defined in MQ.h currently set to 5
    rv = cs550_MQOpen(&mq, "mq", MSGQ_LEN); assert(rv == 0);

    while(cs550_MQPeek(&mq))
    {
        char msg[1024];
        //printf("Received msg: ");
        rv = cs550_MQRecv(&mq, msg, 1024); assert(rv >= 0);

        msg[rv] = '\0';
        printf("%s\n", msg);
        sleep(1);
    }

    rv = cs550_MQClose(&mq); assert(rv == 0);
}
