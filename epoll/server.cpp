#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/select.h>
#include "../ctpl/ctpl.h"
#include <iostream>
#include <cstring>
#include <sys/epoll.h>

#define PORT 5000
#define BIND_ADDR "127.0.0.1"
#define MAXLINE 1024
#define THREAD_COUNT 10
#define MAX_EVENTS 5
#define READ_SIZE 10

using namespace std;

int udpfd;
bool verbose = false;

void init_server() {
	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr(BIND_ADDR);
	servaddr.sin_port = htons(PORT);

	/* create UDP socket */
	udpfd = socket(AF_INET, SOCK_DGRAM, 0);
	// binding server addr structure to udp sockfd
	bind(udpfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
}

void handle_packet(int thread_id, char *buffer, int size, sockaddr *cliaddr) {
	if (verbose)
		printf("\nMessage from UDP client: %s\n", buffer);

	sendto(udpfd, buffer, size, 0, cliaddr, sizeof(sockaddr));
	free(buffer);
	free(cliaddr);
}

void recv_packets() {
	fd_set rset;
	ctpl::thread_pool pool(THREAD_COUNT);

	// clear the descriptor set
	FD_ZERO(&rset);

	int running = 1, event_count, i;
	size_t bytes_read;
	char read_buffer[READ_SIZE + 1];
	struct epoll_event event, events[MAX_EVENTS];
	int epoll_fd = epoll_create1(0);

	event.events = EPOLLIN;
	event.data.fd = 0;

	epoll_ctl(epoll_fd, EPOLL_CTL_ADD, 0, &event);


	while (running) {
		event_count = epoll_wait(epoll_fd, events, MAX_EVENTS, 30000);
		for (i = 0; i < event_count; i++) {
			// printf("Reading file descriptor '%d' -- ", events[i].data.fd);
			bytes_read = read(events[i].data.fd, read_buffer, READ_SIZE);
			// printf("%zd bytes read.\n", bytes_read);
			read_buffer[bytes_read] = '\0';
			// printf("Read '%s'\n", read_buffer);

			if (!strncmp(read_buffer, "stop\n", 5))
				running = 0;
		}
	}

	close(epoll_fd);
}

int main(int argc, char **argv) {
	if (argc > 1) {
		if (!strcmp(argv[1], "-v") || !strcmp(argv[1], "--verbose"))
			verbose = true;
		else {
			cerr << "Usage: ./server" << "\n";
			return 0;
		}
	}
	init_server();
	recv_packets();
	return 0;
}
