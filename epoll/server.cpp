#include <iostream>
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
#include <sys/epoll.h>
using namespace std;

#include "../ctpl/ctpl.h"
#include "../tomlplusplus/toml.hpp"

string bind_addr;
int max_msg_size;
int thread_count;
int epoll_timeout;

vector<int> ports, sock_fds;

bool verbose = false;

void read_config() {
	auto config = toml::parse_file( "config.toml" );
	bind_addr = config["bind_address"].value_or("0.0.0.0");
	max_msg_size = config["max_msg_size"].value_or(1024);
	epoll_timeout = config["epoll_timeout"].value_or(-1);
	thread_count = config["thread_count"].value_or(1);

	for(int i = 0; i < config["ports"].as_array() -> size(); ++i)
		ports.push_back(config["ports"][i].value_or(-1));
}

void init_server() {
	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr(bind_addr.c_str());

	for(auto &port : ports) {
		servaddr.sin_port = htons(port);

		/* create UDP socket */
		int udpfd = socket(AF_INET, SOCK_DGRAM, 0);

		// binding server addr structure to udp sockfd
		bind(udpfd, (struct sockaddr*)&servaddr, sizeof(servaddr));

		sock_fds.push_back(udpfd);
	}
}

void handle_packet(int thread_id, int udpfd, char *buffer, int size, sockaddr *cliaddr) {
	if(verbose)
		printf("Reply to UDP client: %s\n", buffer);

	sendto(udpfd, buffer, size, 0, cliaddr, sizeof(sockaddr));
	free(buffer);
	free(cliaddr);
}

void recv_packets() {
	ctpl::thread_pool pool(thread_count);
	int event_count, i;
	size_t bytes_read;
	char read_buffer[max_msg_size + 1];
	struct epoll_event events[ports.size()];
	int epoll_fd = epoll_create1(0);

	for(int &sock_fd : sock_fds) {
		struct epoll_event event;
		event.events = EPOLLIN;
		event.data.fd = sock_fd;
		epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock_fd, &event);
	}

	while (true) {
		event_count = epoll_wait(epoll_fd, events, ports.size(), epoll_timeout);

		// timeout condition
		if(event_count == 0)
			break;

		for (i = 0; i < event_count; i++) {
			int udpfd = events[i].data.fd;
			char *buffer = new char[max_msg_size];
			sockaddr *cliaddr = (sockaddr *) malloc(sizeof(sockaddr));
			socklen_t len = sizeof(sockaddr);
			int n = recvfrom(udpfd, buffer, max_msg_size, 0, cliaddr, &len);
			if(verbose)
				printf("Message from UDP client: %s\n", buffer);
			pool.push(handle_packet, udpfd, buffer, n, cliaddr);
		}
	}

	close(epoll_fd);
}

void cleanup() {
	for(int &udpfd: sock_fds)
		close(udpfd);
}

int main(int argc, char **argv) {
	if(argc > 1) {
		if(!strcmp(argv[1], "-v") || !strcmp(argv[1], "--verbose"))
			verbose = true;
		else {
			cerr << "Usage: ./server" << "\n";
			return 0;
		}
	}
	read_config();
	init_server();
	recv_packets();
	cleanup();
	return 0;
}
