#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>

#define SERVER_PORT "8080"
#define BACKLOG 5
#define MAX_BUFFER_SIZE 8192

int main(void)
{
	int sockfd;
	struct addrinfo hints = { 
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
		.ai_flags = AI_PASSIVE
	};
	struct addrinfo* res;

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

	char* buffer = calloc(MAX_BUFFER_SIZE, sizeof(char));
	int received_bytes = 0;
	received_bytes = recv(client_fd, buffer, (MAX_BUFFER_SIZE - 2), 0);

	if (received_bytes == -1) 
	{
		perror("recv");
		close(client_fd);
	}

	char* message = "Hello from the server";
	char* response = calloc(1024, sizeof(char));

	sprintf(response,"HTTP/1.1 200 OK\r\n"
		"Content-Type: text/html; charset=UTF-8\r\n"
		"Content-Length: %d\r\n"
		"\r\n"
		"%s\r", (int) strlen(message), message
	);

	send(client_fd, response, strlen(response), 0);
	close(client_fd);
	printf("Closed the connection!\n");

	free(buffer);
	free(response);
	return 0;
}