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

#define SERVER_PORT         "8080"
#define HTTP_SERVER_VERSION "HTTP/1.1"
#define BACKLOG             256
#define MAX_REQUEST_SIZE    4096
#define BLANK_SPACE         " "
#define THREAD_POOL_SIZE    3

static pthread_cond_t queue_is_not_empty_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t queue_lock =             PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t arena_allocator_lock =   PTHREAD_MUTEX_INITIALIZER;

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

	arena = init_arena_allocator();
	if (arena == NULL)
	{
		perror("arena_allocator");
		return -1;
	}

	request_queue = init_queue();
	if (request_queue == NULL)
	{
		perror("request_queue");
		return -1;
	}

	for (int i = 0; i < THREAD_POOL_SIZE; i++)
	{
		if (pthread_create(&thread_pool[i], NULL, listen_for_events, NULL) == -1)
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
	char* buffer;
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
			pthread_mutex_lock(&arena_allocator_lock);
			buffer = allocate_memory_area(arena);
			pthread_mutex_unlock(&arena_allocator_lock);

			handle_client_request(client_fd, buffer);

			pthread_mutex_lock(&arena_allocator_lock);
			free_memory_area(arena, buffer);
			pthread_mutex_unlock(&arena_allocator_lock);
		}
	}

	return NULL;
}

void handle_client_request(int client_fd, char* buffer)
{
	if (buffer == NULL)
	{
		send_internal_server_error_response(client_fd);
		return;
	}

	ssize_t recv_bytes = 0;
	int total_bytes = 0;

	//Do I need this (recv function) in a while loop??
	while ((recv_bytes = recv(client_fd, &buffer[total_bytes], (MAX_REQUEST_SIZE - total_bytes <= 0 ? 0 : MAX_REQUEST_SIZE - total_bytes), 0)) > 0)
	{
		total_bytes += recv_bytes;

		//it does not block the recv function in an infinite loop
		shutdown(client_fd, SHUT_RD);

		if (total_bytes > MAX_REQUEST_SIZE)
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

	buffer[total_bytes] = '\0';

	char* raw_request_line = strtok_r(buffer, "\r\n", &buffer);

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

	char* raw_body = strstr(buffer, "\r\n\r\n");

	if (raw_body == NULL)
	{
		send_internal_server_error_response(client_fd);
		return;
	}

	size_t body_index = raw_body - buffer;

	buffer[body_index] = '\0';

	request_details request_details = {
		.request_line = request_line,
		//skip '\n' character
		.headers = buffer + 1,
		//skip '\r\n\r\n' characters
		.body = &buffer[body_index + 4]
	};


	char* method = request_details.request_line.method;
	char* path = request_details.request_line.path;

	char* response = malloc(1024 * sizeof(char));

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
	free(response);
}

void send_internal_server_error_response(int client_fd) {
	char* error_message = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
	send(client_fd, error_message, strlen(error_message), 0);
	close(client_fd);
}