#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>

#define SERVER_PORT "8080"
#define HTTP_SERVER_VERSION "HTTP/1.1"
#define BACKLOG 5
#define MAX_BUFFER_SIZE 8192

#define EMPTY_SPACE " "

void handle_client_request(int client_fd);

void send_internal_server_error(int client_fd) {
	char* error_message = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
	send(client_fd, error_message, strlen(error_message), 0);
	close(client_fd);
}

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
} request_details;

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

	handle_client_request(client_fd);

	freeaddrinfo(res);
	return 0;
}

void handle_client_request(int client_fd) 
{
	char* buffer = malloc(((MAX_BUFFER_SIZE + 1) * sizeof(char)));

	if (buffer == NULL) {
		send_internal_server_error(client_fd);
		return;
	}

	int recv_bytes = 0;
	int total_bytes = 0;

	while ((recv_bytes = recv(client_fd, buffer, (MAX_BUFFER_SIZE - total_bytes), 0)) > 0)
	{
		total_bytes += recv_bytes;

		//it does not block the recv function in an infinited loop
		shutdown(client_fd, SHUT_RD);
	}

	buffer[total_bytes] = '\0';
	char* ptr_buffer = buffer;

	if (recv_bytes == -1)
	{
		send_internal_server_error(client_fd);
		return ;
	}

	char* raw_request_line = strtok_r(buffer, "\r\n", &buffer);

	if (raw_request_line == NULL) {
		send_internal_server_error(client_fd);
		return;
	}

	request_line request_line = {
		.method = strtok_r(raw_request_line, EMPTY_SPACE, &raw_request_line),
		.path = strtok_r(raw_request_line, EMPTY_SPACE, &raw_request_line),
		.version = strtok_r(raw_request_line, EMPTY_SPACE, &raw_request_line)
	};

	if (request_line.version == NULL || strcmp(HTTP_SERVER_VERSION, request_line.version) != 0) {
		char* error_message = "HTTP/1.1 505 HTTP Version Not Supported\r\n\r\n";
		send(client_fd, error_message, strlen(error_message), 0);
		close(client_fd);
		return;
	}

	char* raw_body = strstr(buffer, "\r\n\r\n");

	if (raw_body == NULL) {
		send_internal_server_error(client_fd);
		return;
	}

	size_t body_index = raw_body - buffer;

	buffer[body_index] = '\0';

	request_details request_details = {
		.request_line = request_line,
		//skip '\n\ character
		.headers = buffer + 1,
		//skip '\r\n\r\n' characters
		.body = &buffer[body_index + 4]
	};

	char* message = "Hello from the server";
	char* response = calloc(1024, sizeof(char));

	sprintf(response, "HTTP/1.1 200 OK\r\n"
		"Content-Type: text/plain; charset=UTF-8\r\n"
		"Content-Length: %zu\r\n"
		"\r\n"
		"%s", strlen(message), message
	);

	send(client_fd, response, strlen(response), 0);
	close(client_fd);

	free(response);
	free(ptr_buffer);
	ptr_buffer = NULL;
}