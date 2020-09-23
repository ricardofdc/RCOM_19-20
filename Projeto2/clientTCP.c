/*      (C)2000 FEUP  */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <string.h>


int initSocket(int server_port, char *server_address){

	int	sockfd;
	struct	sockaddr_in server_addr;
	// char	buf[] = "Mensagem de teste na travessia da pilha TCP/IP\n";  
	// int	bytes;
	
	/*server address handling*/
	bzero((char*)&server_addr,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(server_address);	/*32 bit Internet address network byte ordered*/
	server_addr.sin_port = htons(server_port);		        /*server TCP port must be network byte ordered */
    
	/*open an TCP socket*/
	if ((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0) {
		perror("socket()");
		exit(0);
	}
	/*connect to the server*/
	if(connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
		perror("connect()");
		exit(0);
	}
    	/*send a string to the server*/
	// bytes = write(sockfd, buf, strlen(buf));
	// printf("Bytes escritos %d\n", bytes);

	return sockfd;
}




