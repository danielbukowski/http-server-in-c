#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>

#define SERVER_PORT "8080"
#define HTTP_SERVER_VERSION "HTTP/1.1"
#define BACKLOG 5
#define MAX_BUFFER_SIZE 4096

#define EMPTY_SPACE " "

typedef struct request_line 
{
	char* method;
	char* path;
	char* version;
} request_line;

typedef struct request_details 
{
	request_line request_line;
	char* headers;
	char* body;
} request;

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

	struct sockaddr_storage client_addr = { 0 };
	socklen_t addr_size = sizeof client_addr;

	int client_fd;
	printf("Waiting for a connection...\n");

	client_fd = accept(sockfd, (struct sockaddr*) &client_addr, &addr_size);
	printf("Accepted a connection!\n");

	char* buffer = malloc(MAX_BUFFER_SIZE * sizeof(char));
	int received_bytes = 0;

	received_bytes = recv(client_fd, buffer, (MAX_BUFFER_SIZE - 2), 0);

	if (received_bytes == -1) 
	{
		perror("recv");
		close(client_fd);
		return -1;
	}
	buffer[MAX_BUFFER_SIZE - 1] = '\0';

	char* client_request = (char*) malloc((received_bytes + 1) * sizeof(char));
	strncpy(client_request, buffer, received_bytes);

	char* raw_request_line = strtok_r(client_request, "\r\n", &client_request);

	request_line request_line = {
		.method = strtok_r(raw_request_line, EMPTY_SPACE, &raw_request_line),
		.path = strtok_r(raw_request_line, EMPTY_SPACE, &raw_request_line),
		.version = strtok_r(raw_request_line, EMPTY_SPACE, &raw_request_line)
	};

	char* raw_body;
	if ((raw_body = strstr(client_request, "\r\n\r\n")) == NULL) 
	{
		perror("parsing error");
		return -1;
	}

	int body_index = (int) (raw_body - client_request);
	// raw_body + "\r\n\r\n"
	raw_body = raw_body + 4;
	char* raw_headers = calloc((body_index + 1), sizeof(char));
	// client_request + 1 to remove the leading '\n' in the client_request 
	strncpy(raw_headers, client_request + 1, body_index - 1);

	request request = {
		.request_line = request_line,
		.headers = raw_headers,
		.body = raw_body
	};

	char* message = "Hello from the server";
	char* response = calloc(1024, sizeof(char));

	sprintf(response,"HTTP/1.1 200 OK\r\n"
		"Content-Type: text/html; charset=UTF-8\r\n"
		"Content-Length: %zu\r\n"
		"\r\n"
		"%s", strlen(message), message
	);

	send(client_fd, response, strlen(response), 0);
	close(client_fd);
	printf("Closed the connection!\n");

	free(buffer);
	free(raw_headers);
	free(response);

	return 0;
}