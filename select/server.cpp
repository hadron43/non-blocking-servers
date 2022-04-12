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

#define PORT 5000
#define BIND_ADDR "127.0.0.1"
#define MAXLINE 1024
#define THREAD_COUNT 10
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
	if(verbose)
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

	while(true) {
		// set udpfd in readset
		FD_SET(udpfd, &rset);

		// select the ready descriptor
		int nready = select(udpfd + 1, &rset, NULL, NULL, NULL);

		// if udp socket is readable receive the message.
		if (FD_ISSET(udpfd, &rset)) {
			char *buffer = new char[MAXLINE];
			sockaddr *cliaddr = (sockaddr *) malloc(sizeof(sockaddr));
			socklen_t len = sizeof(sockaddr);
			int n = recvfrom(udpfd, buffer, MAXLINE, 0, cliaddr, &len);
			pool.push(handle_packet, buffer, n, cliaddr);
		}
	}
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
	init_server();
	recv_packets();
	return 0;
}
