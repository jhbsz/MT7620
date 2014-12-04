#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>
#include	<sys/ioctl.h>
#include	<net/if.h>
#include	<net/route.h>
#include    	<dirent.h>
#include	"module.h"

/*
 * arguments: 
 * description: 
 * return: 
 */
int openServerSocket(int srv_port, char* serverIp)
{
	int fd = -1;
	struct sockaddr_in peer_addr;
	int i = 3;
	int ct = 3;
	int ret = -1;
	
	peer_addr.sin_family = PF_INET;
	peer_addr.sin_port = htons(srv_port); 
	peer_addr.sin_addr.s_addr =  inet_addr(serverIp);  
	
	deb_print("before while loop\n");
	
	for(i=0; i<3; i++){
		deb_print("in while loop\n");
		fd = socket(PF_INET,SOCK_STREAM,0);
		if(fd < 0){
			deb_print("fd=%d\n",fd);
			perror("can't open socket\n");
			exit(1);
		}
		deb_print("i=%d\n", i);
		ret = connect( fd, (struct sockaddr *)&peer_addr, sizeof(peer_addr));
		if(ret < 0){
			sleep(1);
			close(fd);
		}else
			return fd;
	}
	return fd;
}


int sendData(int fd, int id, int dataType, void* buf, int buflen)
{
	int ret  = -1;
	msg *p_responseBuf;
	p_responseBuf = (msg*)malloc(sizeof(msg));
	bzero( p_responseBuf, sizeof(msg));
	
	p_responseBuf->id = id;
	p_responseBuf->cmd = dataType;
	if(buflen > 0){
		p_responseBuf->bufsize = buflen;
		memcpy( p_responseBuf->buf, buf, buflen);
	}
	deb_print("send data Type:%d,length:%d\n", dataType, buflen);
	ret = write(fd, p_responseBuf, sizeof(msg));
	if(ret< 0){
		perror("socket write error\n");
		free(p_responseBuf);
		return -1;
	}
	free(p_responseBuf);
	return 0;
}

int recvData(int fd, int *dataType, void* buf, int* buflen, int time)
{
	int ret =-1;
	msg msgbuf;
	fd_set rdfds;
	struct timeval *p_tv;

	FD_ZERO(&rdfds);
	FD_SET(fd, &rdfds);

	if(time <= 0 ){
		p_tv = NULL;
	}else{
		p_tv = (struct timeval*)malloc(sizeof(struct timeval));
		p_tv->tv_sec = time;
		p_tv->tv_usec = 0;
	}
	ret = select(fd+1, &rdfds, NULL, NULL, p_tv);
	if(ret < 0){
		perror("select error!\n");
	}else if(ret == 0 ){
//		deb_print("select timeout\n");
	}else{
		deb_print("read data\n");
		ret = read(fd, &msgbuf, sizeof(msg));
		if(ret<0){
			perror("Socket read error\n");
			return -1;
		}
		*dataType = msgbuf.cmd;
		deb_print("recv data: %d\n",*dataType);
		*buflen = msgbuf.bufsize;
		memcpy(buf, msgbuf.buf, msgbuf.bufsize);
		return ret;
	}
	
	if(time > 0)
		free(p_tv);

	return ret;

}

#if 0
void client_print( client* p)
{
	printf("      id: %d\n", p->id);
	printf("      fd: %d\n", p->fd);
	
	printf("    ssid: %s\n", p->ssid);
	printf("     mac: %s\n", p->mac);
	printf(" channel: %d\n", p->channel);
	
	if(p->have5g){
		printf("5G\n");
		printf("    ssid: %s\n", p->ssid_5g);
		printf("     mac: %s\n", p->mac_5g);
		printf(" channel: %d\n", p->channel_5g);
	}
}
#endif

/*
 * arguments: ifname  - interface name
 * description: test the existence of interface through /proc/net/dev
 * return: -1 = fopen error, 1 = not found, 0 = found
 */
extern int getIfLive(char *ifname)
{
        FILE *fp;
        char buf[256], *p;
        int i;

        if (NULL == (fp = fopen("/proc/net/dev", "r"))) {
                perror("getIfLive: open /proc/net/dev error");
                return -1;
        }

        fgets(buf, 256, fp);
        fgets(buf, 256, fp);
        while (NULL != fgets(buf, 256, fp)) {
                i = 0;
                while (isspace(buf[i++]))
                        ;
                p = buf + i - 1;
                while (':' != buf[i++])
                        ;
                buf[i-1] = '\0';
                if (!strcmp(p, ifname)) {
                        fclose(fp);
                        return 0;
                }
        }
        fclose(fp);
        perror("getIfLive: device not found");
        return 1;
}

/*
 * arguments: ifname  - interface name
 *            if_addr - a 18-byte buffer to store mac address
 * description: fetch mac address according to given interface name
 */
extern int getIfMac(char *ifname, char *if_hw)
{
        struct ifreq ifr;
        char *ptr;
        int skfd;

        if((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
                perror("getIfMac: open socket error");
                return -1;
        }

        strncpy(ifr.ifr_name, ifname, IF_NAMESIZE);
        if(ioctl(skfd, SIOCGIFHWADDR, &ifr) < 0) {
                close(skfd);
                //error(E_L, E_LOG, T("getIfMac: ioctl SIOCGIFHWADDR error for %s"), ifname);
                return -1;
        }

        ptr = (char *)&ifr.ifr_addr.sa_data;
        sprintf(if_hw, "%02X:%02X:%02X:%02X:%02X:%02X",
                        (ptr[0] & 0377), (ptr[1] & 0377), (ptr[2] & 0377),
                        (ptr[3] & 0377), (ptr[4] & 0377), (ptr[5] & 0377));

        close(skfd);
        return 0;
}

/*
 * arguments: ifname  - interface name
 *            if_addr - a 16-byte buffer to store ip address
 * description: fetch ip address, netmask associated to given interface name
 */
int getIfIp(char *ifname, char *if_addr)
{
	struct ifreq ifr;
	int skfd = 0;

	if((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("getIfIp: open socket error");
		return -1;
	}

	strncpy(ifr.ifr_name, ifname, IF_NAMESIZE);
	if (ioctl(skfd, SIOCGIFADDR, &ifr) < 0) {
		close(skfd);
		//perror("getIfIp: ioctl SIOCGIFADDR error ");
		return -1;
	}
	strcpy(if_addr, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));

	close(skfd);
	return 0;
}


extern int initInternet(void)
{
	system("internet.sh");
}




