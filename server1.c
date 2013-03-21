#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUF_SIZE 1024*1024

typedef struct {
	int index;
	char buf[3000];
} ElemType;

int main(int argc,char *argv[])
{
	FILE *fp;	//파일포인터
	int serv_sock;
	size_t nbyte=0;
	char buf[BUF_SIZE];
	size_t fsize=0;
	int val=0;
	int count=0;
	struct sockaddr_in serv_adr,clnt_adr;
	socklen_t clnt_adr_sz;

	ElemType elem;
	clnt_adr_sz=sizeof(clnt_adr);
	serv_sock=socket(PF_INET,SOCK_DGRAM,0);
	
	memset(&serv_adr,0,sizeof(serv_adr));
	serv_adr.sin_family=AF_INET;
	serv_adr.sin_addr.s_addr=htonl(INADDR_ANY);
	serv_adr.sin_port=htons(atoi(argv[1]));

	bind(serv_sock,(struct sockaddr*)&serv_adr,sizeof(serv_adr));

	fp=fopen("/home/lee/바탕화면/sock/LENA0002.mp3","wb");
	
	while(1)
	{
		nbyte=recvfrom(serv_sock,(ElemType*)&elem,sizeof(ElemType),0,(struct sockaddr*)&clnt_adr,&clnt_adr_sz);
		printf("패킷인덱스 : %d",elem.index);
		fwrite(elem.buf,1,3000,fp);
		//fsize-=nbyte;
		//printf("nbyte : %d , fsize : %d\n",nbyte,fsize);
		count++;
		printf("%d\n",count);
	}
	
	fclose(fp);
	close(serv_sock);
}





