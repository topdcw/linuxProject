/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * main.cc
 * Copyright (C) 2017 topdcw <topdcw@topdcw-Lenovo-IdeaPad-Y400>
 * 
 */

#include <iostream>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUF_SIZE 256
//1，打开串口函数
int OpenDev(char*Dev){
	int fd=open(Dev,O_RDWR | O_NOCTTY);

	if(fd == -1){
		perror ("串口打开失败！");
	}
	return fd;
}

//2，设置串口通信速率

int speed_arr[] = {B38400,B19200,B9600,B4800,B2400,B1200,B300,
				   B38400,B19200,B9600,B4800,B2400,B1200,B300};
int name_arr[] = {38400,19200,9600,4800,2400,1200,300,
				   38400,19200,9600,4800,2400,1200,300};
void set_speed(int fd,int speed){
	int i;
	int status;
	struct termios Opt;//串口的设置主要设置这个结构体的的各成员值
	tcgetattr(fd,&Opt);//获得当前串口的有关参数
	for(i=0;i<14;i++){
		if(speed==name_arr[i]){
			tcflush(fd,TCIOFLUSH);//先刷清缓存
			cfsetispeed(&Opt,speed_arr[i]);
			cfsetospeed(&Opt,speed_arr[i]);
			status =tcsetattr(fd,TCSANOW,&Opt);//使串口参数修改立即生效
			if(status!=0){
				perror ("speed set error!");
				return;
			}
			tcflush(fd,TCIOFLUSH);
		}
	}	
}

//3，取消数据回显和回车问题

void cancelHuixian(int fd){

	struct termios options;
	if(tcgetattr(fd,&options)!=0){
		perror ("getattr error!");
		return;
	}
	options.c_lflag &= ~(ICANON |ECHO | ECHOE | ISIG);  /*取消回显*/
	options.c_cc[VTIME] = 0; //更新间隔
	options.c_cc[VMIN] = 8; //read当读取8个字节才返回
	tcflush(fd, TCIFLUSH);
	if (tcsetattr(fd, TCSANOW, &options) != 0){
		perror("huixian error");
		return;
	}
}

//4，向串口写入数据
 void sendInfo(int fd,char*sendbuff,int num){
	int nread = write(fd, sendbuff ,num);
	printf ("send successfully!\n");
 }

int main()
{

	printf ("\n这是一个串口读写程序！\n");
	//a,串口相关配置及初始化
	int fd;
	int nread;
	char sendbuff[BUF_SIZE];//供scoket接收数据和向串口写数据使用
	char recvbuff[BUF_SIZE];//供串口接收数据和socket发送数据使用	
	char *dev="/dev/ttyUSB0";
	fd=OpenDev (dev);
	if(fd==-1){
		printf ("serial port open error!");
		return 0;
	}
	set_speed (fd,9600);
	cancelHuixian(fd);//取消回显
	//b,socket相关配置及初始化
	int sock;
	int strLen;
	if((sock= socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1){ //IPPROTO_UDP表示UDP协议
		printf ("socket err!\n");
		return 1;
	}
		//创建本地sockaddr_in结构体变量
	struct sockaddr_in local_addr;
	memset(&local_addr, 0, sizeof(local_addr));  //每个字节都用0填充
	local_addr.sin_family = AF_INET;  //使用IPv4地址
	local_addr.sin_addr.s_addr = inet_addr("172.24.61.3");  //具体的IP地址
	local_addr.sin_port = htons(1234);  //端口

		//创建sockaddr_in结构体变量,表示服务器地址信息
	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));  //每个字节都用0填充
	serv_addr.sin_family = AF_INET;  //使用IPv4地址
	serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");  //具体的IP地址
	serv_addr.sin_port = htons(2000);  //端口

		//将套接字和IP、端口绑定
	if(bind(sock, (struct sockaddr*)&local_addr, sizeof(local_addr))==-1){
		printf ("bind err!\n");
		return 1;
	}
		//接收客户端请求
    struct sockaddr_in clnt_addr;//客户端地址信息
	socklen_t nSize = sizeof(clnt_addr);
	    
	//c,select函数的使用
	fd_set rfds;
	fd_set wfds;
    struct timeval timeout={3,0}; //select等待3秒，3秒轮询，要非阻塞就置0
	int maxfdp=sock>fd?sock+1:fd+1;//最大描述符+1
	while(1){
		FD_ZERO(&rfds); //每次循环都要清空集合，否则不能检测描述符变化
		FD_ZERO(&wfds);
        FD_SET(sock,&rfds); //添加描述符  
        FD_SET(fd,&rfds); //同上 
		FD_SET(sock,&wfds); 
        FD_SET(fd,&wfds); 
		if(select(maxfdp,&rfds,&wfds,NULL,&timeout)>0){
			//从udp读入并发送给串口
			if(FD_ISSET(sock,&rfds)){//测试sock是否可读
				strLen= recvfrom(sock, sendbuff, BUF_SIZE, 0,(struct sockaddr*)&clnt_addr, &nSize);
				sendbuff[strLen]=0;
				printf("\nMessage form server: %s\n", sendbuff);
				if(FD_ISSET(fd,&wfds)){//测试fd是否可写
					printf("send message to port!\n");
					sendInfo(fd,sendbuff,strLen);
				}
			}
			//从串口读入并发送
			if(FD_ISSET(fd,&rfds)){//测试fd是否可读
				nread=read(fd,recvbuff,256);
				recvbuff[nread]='\0';
				printf("\nMessage form port:%s\n",recvbuff);
				if(FD_ISSET(sock,&wfds)){//测试sock是否可写
					printf("send message to server!\n");
					sendto(sock, recvbuff, nread, 0, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
				}
			}	
		}
	}

	
	std::cout << "Hello world!" << std::endl;
	return 0;
}

