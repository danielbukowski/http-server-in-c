void handle_client_request(int client_fd, char* request_buffer, char* response_buffer, int max_buffer_capacity);

void* listen_for_events(void* arg);

void send_internal_server_error_response(int client_fd);