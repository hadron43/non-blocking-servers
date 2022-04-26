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
#include <sys/select.h>
using namespace std;

#include "../ctpl/ctpl.h"
#include "../tomlplusplus/toml.hpp"

string bind_addr;
int max_msg_size;
int thread_count;
int select_timeout;
timeval *tv = NULL;

vector<int> ports, sock_fds;

bool verbose = false;

void read_config() {
	auto config = toml::parse_file( "config.toml" );
	bind_addr = config["bind_address"].value_or("0.0.0.0");
	max_msg_size = config["max_msg_size"].value_or(1024);
	select_timeout = config["select_timeout"].value_or(-1);
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
	fd_set rset;
	ctpl::thread_pool pool(thread_count);

	if(select_timeout > 0) {
		tv = new timeval();
		tv -> tv_sec = select_timeout;
		tv -> tv_usec = 0;
	}

	// clear the descriptor set
	FD_ZERO(&rset);

	while(true) {
		// set udpfd in readset
		int max_udpfd = -1;
		for(int &udpfd : sock_fds) {
			FD_SET(udpfd, &rset);
			max_udpfd = max(max_udpfd, udpfd);
		}

		// reset timer
		if(select_timeout > 0) {
			tv -> tv_sec = select_timeout;
			tv -> tv_usec = 0;
		}

		// select the ready descriptor
		int nready = select(max_udpfd + 1, &rset, NULL, NULL, tv);

		// if udp socket is readable receive the message.
		bool timeout = true;
		for(int &udpfd : sock_fds) {
			if (!FD_ISSET(udpfd, &rset))
				continue;

			timeout = false;
			char *buffer = new char[max_msg_size];
			sockaddr *cliaddr = (sockaddr *) malloc(sizeof(sockaddr));
			socklen_t len = sizeof(sockaddr);
			int n = recvfrom(udpfd, buffer, max_msg_size, 0, cliaddr, &len);
			if(verbose)
				printf("Message from UDP client: %s\n", buffer);
			pool.push(handle_packet, udpfd, buffer, n, cliaddr);
		}

		if(timeout)
			return;
	}
}

void cleanup() {
	for(int &udpfd: sock_fds)
		close(udpfd);
	if(tv)
		free(tv);
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
