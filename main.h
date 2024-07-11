void handle_client_request(int client_fd, char* buffer);

void* listen_for_events(void* arg);

void send_internal_server_error_response(int client_fd);