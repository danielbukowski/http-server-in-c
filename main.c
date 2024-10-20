#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>

#include "main.h"
#include "circular_queue.h"
#include "arena_allocator.h"

#define SERVER_PORT            "8080"
#define HTTP_SERVER_VERSION    "HTTP/1.1"
#define BACKLOG                256
#define MAX_BUFFER_CAPACITY    4096
#define BLANK_SPACE            " "
#define THREAD_POOL_SIZE       3
#define REQUEST_QUEUE_CAPACITY 1024

static pthread_cond_t queue_is_not_empty_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t queue_lock =             PTHREAD_MUTEX_INITIALIZER;

static pthread_t thread_pool[THREAD_POOL_SIZE];

static circular_queue* request_queue;
static arena_allocator* arena;

typedef struct request_line
{
	char* method;
	char* path;
	char* version;
} request_line;

typedef struct request_details
{
	request_line request_line;
	char*		 headers;
	char*		 body;
} request_details;

typedef struct thread_parameters {
	char*  request_buffer;
	char*  response_buffer;
	int    buffer_capacity;
} thread_parameters;

int main(void)
{
	int sockfd;
	struct addrinfo hints = {
		.ai_family =   AF_INET,
		.ai_socktype = SOCK_STREAM,
		.ai_flags =    AI_PASSIVE
	};
	struct addrinfo* res;

	if (getaddrinfo(NULL, SERVER_PORT, &hints, &res) < 0)
	{
		fprintf(stderr, "failed to get an address info");
		return -1;
	}

	if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0)
	{
		fprintf(stderr, "failed to create a socket");
		return -1;
	}

	if (bind(sockfd, res->ai_addr, res->ai_addrlen) < 0)
	{
		fprintf(stderr, "failed to bind a socket to the address");
		return -1;
	}

	if (listen(sockfd, BACKLOG) < 0)
	{
		fprintf(stderr, "failed to set accepting connections");
		return -1;
	}
	printf("Listening on port %s\n", SERVER_PORT);

	arena = init_arena_allocator(MAX_BUFFER_CAPACITY, THREAD_POOL_SIZE * 2);
	if (arena == NULL)
	{
		fprintf(stderr, "failed to initialize an arena allocator");
		return -1;
	}

	request_queue = init_queue(REQUEST_QUEUE_CAPACITY);
	if (request_queue == NULL)
	{
		fprintf(stderr, "failed to initialize a circular queue");
		return -1;
	}

	for (int i = 0; i < THREAD_POOL_SIZE; i++)
	{
		thread_parameters thread_params = { 0 };
		thread_params.request_buffer = allocate_memory_area(arena);
		thread_params.response_buffer = allocate_memory_area(arena);
		thread_params.buffer_capacity = MAX_BUFFER_CAPACITY;

		if (thread_params.request_buffer == NULL || thread_params.response_buffer == NULL)
		{
			fprintf(stderr, "failed to bind a memory arena to a thread");
			return -1;
		}

		if (pthread_create(&thread_pool[i], NULL, listen_for_events, &thread_params) == -1)
		{
			fprintf(stderr, "failed to create %dth thread ", i);
			return -1;
		}
	}

	int client_fd;
	printf("Waiting for a connection...\n");

	while (true)
	{
		client_fd = accept(sockfd, NULL, 0);

		pthread_mutex_lock(&queue_lock);

		bool is_added = enqueue(request_queue, client_fd);
		pthread_cond_signal(&queue_is_not_empty_cond);

		pthread_mutex_unlock(&queue_lock);

		if (!is_added)
		{
			printf("Rejected a connection!\n");
			close(client_fd);
			continue;
		}
		printf("Accepted a connection!\n");
	}

	freeaddrinfo(res);
	free(request_queue);
	return 0;
}

void* listen_for_events(void* args)
{
	int client_fd;
	thread_parameters* parameters = args;

	while (true)
	{

		pthread_mutex_lock(&queue_lock);
		client_fd = dequeue(request_queue);

		if (client_fd == -1)
		{
			pthread_cond_wait(&queue_is_not_empty_cond, &queue_lock);
			client_fd = dequeue(request_queue);
		}

		pthread_mutex_unlock(&queue_lock);

		if (client_fd > 0)
		{
			handle_client_request(client_fd, parameters->request_buffer, parameters->response_buffer, parameters->buffer_capacity );
		}
	}

	return NULL;
}

void handle_client_request(int client_fd, char* request_buffer, char* response_buffer, int max_buffer_capacity)
{
	ssize_t recv_bytes = 0;
	int total_bytes = 0;

	//Do I need this (recv function) in a while loop??
	while ((recv_bytes = recv(client_fd, &request_buffer[total_bytes], (max_buffer_capacity - total_bytes <= 0 ? 0 : max_buffer_capacity - total_bytes), 0)) > 0)
	{
		total_bytes += recv_bytes;

		//it does not block the recv function in an infinite loop
		shutdown(client_fd, SHUT_RD);

		if (total_bytes > max_buffer_capacity)
		{
			char* response = "HTTP/1.1 413 Content Too Large\r\n\r\n";
			send(client_fd, response, strlen(response), 0);
			return;
		}
	}

	if (recv_bytes == -1)
	{
		send_internal_server_error_response(client_fd);
		return;
	}

	request_buffer[total_bytes] = '\0';

	char* raw_request_line = strtok_r(request_buffer, "\r\n", &request_buffer);

	if (raw_request_line == NULL)
	{
		send_internal_server_error_response(client_fd);
		return;
	}

	request_line request_line = {
		.method = strtok_r(raw_request_line, BLANK_SPACE, &raw_request_line),
		.path = strtok_r(raw_request_line, BLANK_SPACE, &raw_request_line),
		.version = strtok_r(raw_request_line, BLANK_SPACE, &raw_request_line)
	};

	if (request_line.version == NULL || strcmp(HTTP_SERVER_VERSION, request_line.version) != 0)
	{
		char* error_message = "HTTP/1.1 505 HTTP Version Not Supported\r\n\r\n";
		send(client_fd, error_message, strlen(error_message), 0);
		return;
	}

	char* raw_body = strstr(request_buffer, "\r\n\r\n");
	if (raw_body == NULL)
	{
		send_internal_server_error_response(client_fd);
		return;
	}

	size_t body_index = raw_body - request_buffer;

	request_buffer[body_index] = '\0';

	request_details request_details = {
		.request_line = request_line,
		//skip '\n' character
		.headers = request_buffer + 1,
		//skip '\r\n\r\n' characters
		.body = &request_buffer[body_index + 4]
	};


	char* method = request_details.request_line.method;
	char* path = request_details.request_line.path;
	
	char* response = response_buffer;
	if (strncmp(method, "GET", 3) == 0)
	{
		if (strlen(path) == 6 && strncmp(path, "/hello", 6) == 0)
		{
			char* message = "Hello from the server";
			sprintf(response, "HTTP/1.1 200 OK\r\n"
				"Content-Type: text/plain; charset=UTF-8\r\n"
				"Connection: close\r\n"
				"Content-Length: %zu\r\n"
				"\r\n"
				"%s", strlen(message), message
			);
		}
		else
		{
			sprintf(response, "HTTP/1.1 404 Not Found\r\n\r\n");
		}
	}
	else
	{
		sprintf(response, "HTTP/1.1 501 Not Implemented\r\n\r\n");
	}

	send(client_fd, response, strlen(response), 0);
	close(client_fd);
}

void send_internal_server_error_response(int client_fd) {
	char* error_message = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
	send(client_fd, error_message, strlen(error_message), 0);
	close(client_fd);
}