#include "MQ.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <assert.h>
#include <sys/mman.h>

#define DEBUG 0
#define TRUE 1
#define FALSE 0
#define F   2
#define T    1


int 
cs550_MQOpen(struct cs550_MQ *mqp, const char *path, int qlen) {

    int rv;

    if (access(path, F_OK) != 0) {
        int en = errno;
        assert(en == ENOENT);

        int fd = open(path, O_RDWR|O_CREAT|O_EXCL, 0600);
        if (fd < 0) {
            return -1;
        }

        rv = ftruncate(fd, sizeof(struct Info)); assert(rv == 0);

        void *vp = mmap(NULL, sizeof(struct Info)/*1024*/, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
        assert(vp != MAP_FAILED);

        rv = close(fd); assert(rv == 0);

        mqp->infop = vp;

        {
            pthread_mutexattr_t attr;
            rv = pthread_mutexattr_init(&attr); assert(rv == 0);
            rv = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK); assert(rv == 0);
            rv = pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED); assert(rv == 0);
            rv = pthread_mutex_init(&mqp->infop->mutex, &attr); assert(rv == 0);
            rv = pthread_mutexattr_destroy(&attr); assert(rv == 0);
        }

        {
            pthread_condattr_t attr;
            rv = pthread_condattr_init(&attr); assert(rv == 0);
            rv = pthread_condattr_setpshared(&attr, PTHREAD_PROCESS_SHARED); assert(rv == 0);
            rv = pthread_cond_init(&mqp->infop->condvar1, &attr); assert(rv == 0);
            rv = pthread_cond_init(&mqp->infop->condvar2, &attr); assert(rv == 0);
 
            rv = pthread_condattr_destroy(&attr); assert(rv == 0);
        }

        mqp->infop->n = 0;
        mqp->infop->in = -1;
        mqp->infop->out = 0;
        
    } 
    else 
    {

        int fd = open(path, O_RDWR);
        if (fd < 0) {
            return -1;
        }

        void *vp = mmap(NULL, sizeof(struct Info)/*1024*/, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
        assert(vp != MAP_FAILED);

        rv = close(fd); assert(rv == 0);

        mqp->infop = vp;
    }    
    
    mqp->msg_in = mqp->infop->in;
    mqp->msg_out = mqp->infop->out;
    mqp->path = path;
    mqp->len = qlen;
    mqp->infop->process_count++;

    return 0;
}

int pid_present(struct cs550_MQ *mq, int pid)
{
    int i=0;
    struct  Info *ip = mq->infop;
    
    while(i<10)   {
        if(ip->receivers.PID[i] == pid) return 1;
    i++;
    }
    
    return 0;
}

int
cs550_MQSend(struct cs550_MQ *mq, const char *msg, size_t msg_size) 
{

    int rv;
    int out, in;

    struct Info *ip = mq->infop;

    rv = pthread_mutex_lock(&ip->mutex); assert(rv == 0);

    if (DEBUG) fprintf(stderr, "Send: On queue: %d\n", ip->n);
    

    while ((ip->out == 0) && (ip->in ==mq->len-1) ||
        (ip->out > 0 && ip->in == ip->out -1))
    {
    
        rv = pthread_cond_wait(&ip->condvar1, &ip->mutex); assert(rv == 0);
    }

    if((ip->out > 0) &&  (ip->in == mq->len -1))
    {
        ip->in = 0;
        memcpy(ip->msgs[ip->in].buf, msg, msg_size);
        ip->msgs[ip->in].size = msg_size;
        ip->msgs[ip->in].present = T;
        ip->msgs[ip->in].Recvcount = ip->receivers.RecvID;
    
    }
    else if((ip->out == 0 && ip->in == -1) || (ip->in != ip->out-1))
    {
        ip->in++;
        memcpy(ip->msgs[ip->in].buf, msg, msg_size);
        ip->msgs[ip->in].size = msg_size;
        ip->msgs[ip->in].present = T;
        ip->msgs[ip->in].Recvcount = ip->receivers.RecvID;
    }

    ip->n++;

    if (DEBUG) fprintf(stderr, "Send: On queue: %d\n", ip->n);

    rv = pthread_mutex_unlock(&ip->mutex); assert(rv == 0);

    rv = pthread_cond_broadcast(&ip->condvar2); assert(rv == 0);

    return 0;
}

ssize_t
cs550_MQRecv(struct cs550_MQ *mq, char *msg, size_t buf_size)
{

    int pid, i, pid_flag, updateQ;
    int rv;
    size_t sz;
    struct Info *ip = mq->infop;

    rv = pthread_mutex_lock(&ip->mutex); assert(rv == 0);
    pid = getpid();
    
    // check if PID is present in Process list
    pid_flag = pid_present(mq, pid);
    if(!pid_flag)
    {
        ip->receivers.PID[ip->receivers.RecvID] = pid;  
        ip->receivers.RecvID++;

        // If 'out' greater than 'in' in circular Q then first loop from smaller index to larger index and then from larger to smaller
        if(ip->out > ip->in) 
        {
            for(i=0; i <= ip->in; i++)
                if(ip->msgs[i].present == T)
                    ip->msgs[i].Recvcount = ip->receivers.RecvID;     

            for(i=ip->out; i <= mq->len-1; i++)
                if(ip->msgs[i].present == T)
                    ip->msgs[i].Recvcount = ip->receivers.RecvID;     

        }
        // If 'in' greater than 'out' in circular Q then loop from larger to smaller
        else
        {
            for(i=ip->out; i <= ip->in; i++)
                if(ip->msgs[i].present == T)
                    ip->msgs[i].Recvcount = ip->receivers.RecvID; 
        }
        mq->msg_out = ip->out;
    }


    if (DEBUG) fprintf(stderr, "Recv: On queue: %d\n", ip->n);

    mq->msg_in = ip->in;
    updateQ = (ip->msgs[mq->msg_out].Recvcount == 1);
    
    while (mq->msg_out == 0 && mq->msg_in == -1)
    {
        rv = pthread_cond_wait(&ip->condvar2, &ip->mutex); assert(rv == 0);
        mq->msg_in = ip->in;
    }


    
    if(mq->msg_out == mq->msg_in)
    {
        memcpy(msg, ip->msgs[mq->msg_out].buf, ip->msgs[mq->msg_out].size);
        sz = ip->msgs[mq->msg_out].size;
        ip->msgs[mq->msg_out].Recvcount--;
        ip->msgs[mq->msg_out].present = F;            
    }
    else
    {
        if(mq->msg_out == mq->len-1)
        {
            memcpy(msg, ip->msgs[mq->msg_out].buf, ip->msgs[mq->msg_out].size);
            sz = ip->msgs[mq->msg_out].size; 
            ip->msgs[mq->msg_out].Recvcount--;
            ip->msgs[mq->msg_out].present = F;
            mq->msg_out = 0;   
        }
        else
        {
            
            memcpy(msg, ip->msgs[mq->msg_out].buf, ip->msgs[mq->msg_out].size);
            sz = ip->msgs[mq->msg_out].size;
            ip->msgs[mq->msg_out].present = F;
            ip->msgs[mq->msg_out].Recvcount--;
            mq->msg_out++;
        }
    }
        

    if(updateQ)
    {

        if(ip->out == ip->in)
        {

            mq->msg_in = -1;   
            mq->msg_out = 0;
            ip->out = 0;
            ip->in = -1;            

        }
        else
        {
            if(ip->out == mq->len -1)
                ip->out = 0;
        
            else
                ip->out++;
        } 

    }


    if (DEBUG) fprintf(stderr, "Recv: On queue: %d\n", ip->n);

    rv = pthread_mutex_unlock(&ip->mutex); assert(rv == 0);
    rv = pthread_cond_broadcast(&ip->condvar1); assert(rv == 0);

    return sz;
}


ssize_t
cs550_MQPeek(struct cs550_MQ *mq) 
{

    int rv, msg_size;
    int qempty;
    struct Info *ip = mq->infop;
    rv = pthread_mutex_lock(&ip->mutex); assert(rv == 0);
    
    qempty = (ip->out == 0 && ip->in == -1) ;
    if(!qempty) 
    	msg_size = ip->msgs[ip->out].size;

    else
    	msg_size = CS550_NOMSG;

    rv = pthread_mutex_unlock(&ip->mutex); assert(rv == 0);
   

    return msg_size;
}

int
cs550_MQClose(struct cs550_MQ *mq) 
{
    int rv, i;
    int pid, pid_flag;
    
    struct Info * ip = mq->infop;
    rv = pthread_mutex_lock(&ip->mutex); assert(rv == 0);

    if(ip->process_count == 1)
        unlink(mq->path);
    
    else
    {
        pid = getpid();
        // check if PID is present in Process list
        pid_flag = pid_present(mq, pid);
        
        if(pid_flag)  
        {
            if(mq->msg_out < mq->msg_in)
            {
                for(i=mq->msg_out; i <= mq->msg_in; i++)
                        ip->msgs[i].Recvcount--;                 
                
            }
            else
            {
                for(i=mq->msg_out; i <= mq->len-1; i++)
                        ip->msgs[i].Recvcount--;     
                        
                for(i=0; i <= mq->msg_in; i++)
                        ip->msgs[i].Recvcount--;                          
            }
        
        } 
        ip->process_count--;   
    }

    if (DEBUG) printf("Queue now empty..Exiting\n");
    rv = pthread_mutex_unlock(&ip->mutex); assert(rv == 0);
    
    return 0;
}