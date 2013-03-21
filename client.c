#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <signal.h>

#define DEFAULT_PORT 9190
#define DEFAULT_ADDR "127.0.0.1"
#define DEFAULT_BLKSZ (64*1024)
#define DEFAULT_BUFSZ (8*1024*1024)
#define DEFAULT_PKTSZ (3000)
#define NOWAIT 0
#define WAIT 1

#define pktsz 3000
#define BUF_SIZE 	1024*1024

typedef struct {
	int index;
	char buf[DEFAULT_PKTSZ];
} ElemType;

typedef struct thread_argument{
	char multicast_addr[20];
	int multicast_port;
	int blockSize;
	int packetSize;
}thread_arg;

typedef struct buffer{
	int start;
	int end;
	int size;
	int state;
	ElemType *elems;
}CircularBuffer;

void cbInit(CircularBuffer *cb, int size) {
    cb->size  = size + 1; /* include empty elem */
    cb->start = 0;
    cb->end   = 0;
    cb->elems = (ElemType *)calloc(cb->size, sizeof(ElemType));
}
 
void cbFree(CircularBuffer *cb) {
    free(cb->elems); /* OK if null */ }
 
int cbIsFull(CircularBuffer *cb) {
    return (cb->end + 1) % cb->size == cb->start; }
 
int cbIsEmpty(CircularBuffer *cb) {
    return cb->end == cb->start; }
 
/* Write an element, overwriting oldest element if buffer is full. App can
   choose to avoid the overwrite by checking cbIsFull(). */
void cbWrite(CircularBuffer *cb, ElemType *elem) {
    //패킷생성
    printf("들어갔다\n");
    ElemType *pkt;
    pkt = (ElemType *)malloc(sizeof(ElemType));  

    //패킷으로 데이터 복사
    memcpy(pkt->buf,elem->buf,pktsz);  
    pkt->index =elem->index;

    //버퍼에 삽입
    cb->elems[cb->end] = *pkt;
    cb->end = (cb->end + 1) % cb->size;
    if (cb->end == cb->start)
        cb->start = (cb->start + 1) % cb->size; /* full, overwrite */
}
 
/* Read oldest element. App must ensure !cbIsEmpty() first. */
int cbRead(CircularBuffer *cb, ElemType *elem) {
	int size = 1;
    printf("이제 빼온다\n");
    //패킷생성
    ElemType *pkt;
    pkt = (ElemType *)malloc(sizeof(ElemType));

    *pkt = cb->elems[cb->start];
     elem->index = pkt->index;
     memcpy(elem->buf,pkt->buf,pktsz);
     
    cb->start = (cb->start + 1) % cb->size;
	return size;
}

/*
typedef struct buffer{
	int head;
	int tail;
	int sz;
	int state;
	char *buffer;
}bufType;
*/

thread_arg t_argument;
CircularBuffer ringBuf;


pthread_mutex_t buf_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t buf_no_empty = PTHREAD_COND_INITIALIZER;
/*
int availableBuf()
{
	if(ringBuf.head<=ringBuf.tail)
		return ((ringBuf.sz-1)-(ringBuf.tail-ringBuf.head));
	else
		return (ringBuf.head-ringBuf.tail-1);
}

int usageBuf()
{
	return (ringBuf.sz-1-availableBuf());
}

void putBuf(char *data,int size)
{
	char *buf;
	int bufsz;
	int newTail,oldTail;

	bufsz=ringBuf.sz;
	buf=ringBuf.buffer;

	oldTail=ringBuf.tail;
	newTail=(oldTail+size)%bufsz;

	if(newTail<oldTail)
	{
		register int cpsz = bufsz -1 -oldTail;
		memcpy(&buf[oldTail+1],data,cpsz);
		memcpy(&buf[0],&data[cpsz],newTail+1);
	}
	else if(newTail>oldTail)
	{
		memcpy(&buf[oldTail+1],data,size);
	}
	
	ringBuf.tail=newTail;
	printf("ringbuf tail : %d\n",ringBuf.tail);
}

int getBuf(char *data,int pos,int size)
{
	char *buf;
	int bufsz;
	int newHead,oldHead;

	bufsz=ringBuf.sz;
	buf=ringBuf.buffer;

	oldHead = ringBuf.head;
	newHead = (oldHead+size)%bufsz;

	if(newHead<oldHead)
	{
		register int cpsz=bufsz-oldHead-1;
		memcpy(&data[pos],&buf[oldHead+1],cpsz);
		memcpy(&data[pos+cpsz],&buf[0],newHead+1);
	}

	printf("oldHead : %d\n",oldHead);
	if(newHead>oldHead)
	{
		memcpy(data,&buf[oldHead+1],size);
	}
	
	ringBuf.head=newHead;
	return size;
}
*/



void* dataSrc(void* args)
{
	/*쓰레드*/
	thread_arg *dargs =args;
	
	/*파일포인터 변수*/
	FILE *fp;
	size_t fsize=0;
	size_t nsize=0;
	
	/*그외 변수*/
	char *buf;
	int loop;
	int pktsize;
	char cbuf[BUF_SIZE];
	int i;

	/*초기화 세팅*/
	fp=fopen("/home/lee/바탕화면/sock/LENA.mp3","rb");
	fseek(fp,0,SEEK_END);
	fsize=ftell(fp);
	fseek(fp,0,SEEK_SET);
	
	pktsize = dargs->blockSize;
	

	/*패킷선언*/
	ElemType elem;

	
	loop=fsize/pktsize;
	/*Producer*/
	for(i=0;i<loop;i++)
	{
		/*패킷데이터부여*/
		elem.index= i;
		fread(elem.buf,1,pktsize,fp);
		/*뮤텍스*/
		pthread_mutex_lock(&buf_mutex);
		while(cbIsFull(&ringBuf))
		{
			ringBuf.state = WAIT;
			pthread_cond_wait(&buf_no_empty,&buf_mutex);
		}
		ringBuf.state = NOWAIT;
		cbWrite(&ringBuf,&elem);
		//putBuf(buf,pktsize);
		pthread_mutex_unlock(&buf_mutex);
	}
	
	fclose(fp);
}


void* netSend(void *args)
{

	/*쓰레드*/
	thread_arg *dargs =args;

	/*consumer Thread 변수*/
	//int pktsz,blksz;
	//pktsz=dargs->packetSize;
	//blksz=dargs->blockSize;
	
	/*패킷선언*/
	ElemType elem;

	int nbytes = 0;	
	
	/*socket 관련 변수*/
	int fd;
	int addrlen = 0;
	struct sockaddr_in addr;
	

	fd=socket(AF_INET,SOCK_DGRAM,0);
	memset(&addr,0,sizeof(addr));

	addr.sin_family=AF_INET;
	addr.sin_addr.s_addr = inet_addr(dargs->multicast_addr);
	addr.sin_port = htons(dargs->multicast_port);

	addrlen = sizeof(addr);
	while(1)
	{
	pthread_mutex_lock(&buf_mutex);
	if((ringBuf.state==WAIT) && !cbIsFull(&ringBuf) )
	{
		pthread_cond_signal(&buf_no_empty);
	}
	pthread_mutex_unlock(&buf_mutex);

	pthread_mutex_lock(&buf_mutex);
	if(!cbIsEmpty(&ringBuf))
	{
		nbytes = cbRead(&ringBuf,&elem);
		//nbytes = getBuf(buf,0,pktsz);
	}
	pthread_mutex_unlock(&buf_mutex);

	if(nbytes!=0)
	{
		nbytes = sendto(fd,(ElemType*)&elem,sizeof(ElemType),0,(struct sockaddr*)&addr,addrlen);
	}
	nbytes=0;
	usleep(1000);
	}

}

int main(int argc,char *argv[])
{
	/*초기화*/
	cbInit(&ringBuf,3000);
	


	strcpy(t_argument.multicast_addr,DEFAULT_ADDR);
	t_argument.multicast_port = DEFAULT_PORT;
	t_argument.blockSize = DEFAULT_BLKSZ;
	t_argument.packetSize = DEFAULT_PKTSZ;
	
	/*쓰레드*/
	pthread_t dataSrc_tid;
	pthread_t netSend_tid;



	if(pthread_create(&dataSrc_tid,NULL,dataSrc,&t_argument)!=0)
	{
		perror("Data Source Thread creation failed.");
		exit(1);
	}

	if(pthread_create(&netSend_tid,NULL,netSend,&t_argument)!=0)
	{
		perror("Data Source Thread creation failed.");
		exit(1);
	}
	
	pthread_join(dataSrc_tid,NULL);
	pthread_join(netSend_tid,NULL);
	
	
	/*파일전송*/
	/*
	fp=fopen("/home/lee/바탕화면/sock/LENA.mp3","rb");
	fseek(fp,0,SEEK_END);
	fsize=ftell(fp);
	fseek(fp,0,SEEK_SET);
	printf("File Size : %d \n",fsize);
	sendto(sock,&fsize,sizeof(fsize),0,(struct sockaddr*)&serv_adr,sizeof(serv_adr));
	*/
}	
