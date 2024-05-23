#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include "circular_queue.h"

#define SERVER_PORT "8080"
#define HTTP_SERVER_VERSION "HTTP/1.1"
#define BACKLOG 30
#define MAX_BUFFER_SIZE 4096
#define EMPTY_SPACE " "
#define THREAD_POOL_SIZE 2

static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t watch_queue_lock = PTHREAD_MUTEX_INITIALIZER;

static pthread_t thread_pool[THREAD_POOL_SIZE];

void handle_client_request(int client_fd);

void* watch_queue(void* arg);

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

	circular_queue* queue = init_queue();
	for (int i = 0; i < THREAD_POOL_SIZE; i++)
	{
		if (pthread_create(&thread_pool[i], NULL, watch_queue, queue) == -1)
		{
			perror("thread pool");
			return -1;
		}
	}

	int client_fd;
	printf("Waiting for a connection...\n");

	while (true)
	{
		client_fd = accept(sockfd, NULL, 0);

		pthread_mutex_lock(&queue_lock); 
		bool is_added = enqueue(queue, client_fd);
		pthread_mutex_unlock(&queue_lock);

		if (!is_added)
		{
			printf("Rejected a connection!\n");
			close(client_fd);
			continue;
		}
		pthread_cond_signal(&cond);
		printf("Accepted a connection!\n");
	}

	freeaddrinfo(res);
	free(queue);
	return 0;
}

void* watch_queue(void* arg)
{
	circular_queue* queue = (circular_queue*)arg;

	while (true)
	{
		pthread_mutex_lock(&queue_lock);
		int client_fd = dequeue(queue);
;		pthread_mutex_unlock(&queue_lock);

		if (client_fd != -1)
		{
			handle_client_request(client_fd);
			continue;
		}

		pthread_cond_wait(&cond, &watch_queue_lock);
	}

	return NULL;
}

void handle_client_request(int client_fd)
{
	char* buffer = malloc(((MAX_BUFFER_SIZE + 1) * sizeof(char)));

	if (buffer == NULL) 
	{
		send_internal_server_error(client_fd);
		return;
	}

	int recv_bytes = 0;
	int total_bytes = 0;

	while ((recv_bytes = recv(client_fd, &buffer[total_bytes], (MAX_BUFFER_SIZE - total_bytes), 0)) > 0)
	{
		total_bytes += recv_bytes;

		//it does not block the recv function in an infinite loop
		shutdown(client_fd, SHUT_RD);
	}

	if (recv_bytes == -1)
	{
		send_internal_server_error(client_fd);
		free(buffer);
		return;
	}

	buffer[total_bytes] = '\0';
	char* ptr_buffer = buffer;

	char* raw_request_line = strtok_r(buffer, "\r\n", &buffer);

	if (raw_request_line == NULL)
	{
		send_internal_server_error(client_fd);
		free(ptr_buffer);
		return;
	}

	request_line request_line = {
		.method = strtok_r(raw_request_line, EMPTY_SPACE, &raw_request_line),
		.path = strtok_r(raw_request_line, EMPTY_SPACE, &raw_request_line),
		.version = strtok_r(raw_request_line, EMPTY_SPACE, &raw_request_line)
	};

	if (request_line.version == NULL || strcmp(HTTP_SERVER_VERSION, request_line.version) != 0)
	{
		char* error_message = "HTTP/1.1 505 HTTP Version Not Supported\r\n\r\n";
		send(client_fd, error_message, strlen(error_message), 0);
		close(client_fd);
		free(ptr_buffer);
		return;
	}

	char* raw_body = strstr(buffer, "\r\n\r\n");

	if (raw_body == NULL)
	{
		send_internal_server_error(client_fd);
		free(ptr_buffer);
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