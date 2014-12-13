#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>
#include	<sys/ioctl.h>
#include	<net/if.h>
#include	<net/route.h>
#include    <dirent.h>
#include	<sys/types.h>
#include	<sys/socket.h>
#include	<fcntl.h>
//#include 	"nvram.h"
#include 	"command.h"
#include 	"tool.h"

#define MAX(a,b) (a>b?a:b)

struct sockaddr_in server_addr;


/*
 * register module to server 
 */
int register2Server()
{
	int ret = -1;
	int dataType;
	char buf[DATASIZE];
	int buflen;
	int counter=0;

	printf("register2Server\n");
	while(1){
		if(counter > MAXCOUNTER){
			g_state = STATE_IDLE;			
			return -1;
		}

		switch(g_state){
			case STATE_IDLE:
				ret = sendData(srv_fd, REQ_HELLO, p_module, sizeof(moduleInfo));
				if(ret < 0){
					/* FIXME */
					printf("%s:sendData error",__FUNCTION__);
					counter++;
					break;
				}
				ret = recvData(srv_fd, &dataType, buf, &buflen, 1);
				if(ret<0 || dataType != REQ_HELLO){
					counter++;		
					break;
				}
				g_state = STATE_HELLO;
				counter=0;
				break;

			case STATE_HELLO:
				ret = sendData(srv_fd, REQ_FIRTWARE_UPDATE, p_module->fwVersion, 
								sizeof(p_module->fwVersion));
				if(ret < 0){
					/* FIXME */
					printf("%s:sendData error\n",__FUNCTION__);
					counter++;
					break;
				}
				ret = recvData(srv_fd, &dataType, buf, &buflen, 1);
				if(ret<0 || dataType != REQ_FIRTWARE_UPDATE){
					printf("REQ_FIRTWARE_UPDATE response error\n");
					counter++;
					break;
				}
				if( *(int*)buf == 1 ){
					recvFirmware(srv_fd);	

				       //this function will reboot the module!!				
					updateFirmware();
					exit(0);
				}
				g_state = STATE_FIRMWARE_CHECKED;
				counter=0;
				break;

			case STATE_FIRMWARE_CHECKED:
				ret = sendData(srv_fd, REQ_CONFIG, NULL, 0);
				if(ret < 0){
					/* FIXME */
					printf("%s:sendData error",__FUNCTION__);
					counter++;
					break;
				}
				ret = recvData(srv_fd, &dataType, buf, &buflen, 1);
				if(ret<0 || dataType != REQ_CONFIG){
					printf("REQ_FIRTWARE_UPDATE response error\n");
					counter++;
					break;
				}
				if( buflen!=0 ){
//					doConfiguration(buf, buflen);
					break;
				}else{				
					g_state = STATE_CONFIG;
					counter=0;
					break;
				}

			case STATE_CONFIG:
				ret = sendData(srv_fd, REQ_RUN, NULL, 0);
				if(ret < 0){
					/* FIXME */
					printf("%s:sendData error",__FUNCTION__);
					counter++;
					break;
				}
				ret = recvData(srv_fd, &dataType, buf, &buflen, 1);
				if(ret<0 || dataType != REQ_RUN){
					counter++;
					break;
				}
				g_state = STATE_RUN;
				counter=0;
				break;

			case STATE_RUN:
				close(srv_fd);
				srv_fd = -1;
				return 0;
		}
	}
	
}





int main(int argc, char *argv[])
{
	int ret = -1;
	int sock;
	struct sockaddr_in local_addr; 
	int server_len = sizeof(struct sockaddr_in);
	int count = -1;
	fd_set readfd; 
	char buffer[1024];
	struct timeval timeout;
	timeout.tv_sec = 2;
	timeout.tv_usec = 0;

	sock = socket(AF_INET, SOCK_DGRAM, 0); 
	if (sock < 0)
	{
		perror("sock error");
		return -1;
	}

	memset((void*) &local_addr, 0, sizeof(struct sockaddr_in));
	local_addr.sin_family = AF_INET;
	local_addr.sin_addr.s_addr = htons(INADDR_ANY );
	local_addr.sin_port = htons(UPORT);

	ret = bind(sock, (struct sockaddr*)&local_addr, sizeof(local_addr));
	if (ret < 0)
	{
		perror("bind error");
		return -1;
	}


	while (1)
	{
		timeout.tv_sec = 10;
		timeout.tv_usec = 0;
	
		FD_ZERO(&readfd);
		FD_SET(sock, &readfd);

		ret = select(sock + 1, &readfd, NULL, NULL, &timeout); 
		switch (ret)
		{
		case -1: 
			perror("select error:");
			break;
		case 0: 
			printf("select timeout\n");
			break;
		default:
			if (FD_ISSET(sock,&readfd)){

				count = recvfrom(sock, buffer, 1024, 0,
						(struct sockaddr*)&server_addr, &server_len); 
				
				if (strstr(buffer, IP_FOUND))
				{
					printf("\nClient connection information:\n\t IP: %s, port: %d\n",
							(char *)inet_ntoa(server_addr.sin_addr),
							ntohs(server_addr.sin_port));

					//将数据发送给客户端
					memcpy(buffer, IP_FOUND_ACK, strlen(IP_FOUND_ACK) + 1);
					count = sendto(sock, buffer, strlen(buffer), 0,
							(struct sockaddr*) &server_addr, server_len);
				}
			}
			memcpy(p_addr, &from_addr, sizeof(from_addr));
			return 0;
		}
	}
	return -1;
}