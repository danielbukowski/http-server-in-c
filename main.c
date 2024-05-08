#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>

#define SERVER_PORT "8080"
#define BACKLOG 5

int main(void)
{
	int sockfd;
	struct addrinfo hints, *res;

	memset(&hints, 0, sizeof hints);

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if (getaddrinfo(NULL, SERVER_PORT, &hints, &res) < 0) 
	{
		perror("getaddrinfo");
		return -1;
	}

	if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0) 
	{
		perror("socket");
		return -1;
	}

	if (bind(sockfd, res->ai_addr, res->ai_addrlen) < 0) 
	{
		perror("bind");
		return -1;
	}

	if (listen(sockfd, BACKLOG) < 0)
	{
		perror("listen");
		return -1;
	}
	printf("Listening on port %s\n", SERVER_PORT);

	struct sockaddr_storage client_addr;
	socklen_t addr_size = sizeof client_addr;

	int client_fd;
	
	printf("Waiting for a connection...\n");

	client_fd = accept(sockfd, (struct sockaddr*) &client_addr, &addr_size);
	printf("Accepted a connection!\n");

	char* message = "HTTP/1.1 200 OK\r\n\r\n";
	send(client_fd, message, strlen(message), 0);

	close(client_fd);
	printf("Closed the connection!\n");
	return 0;
}