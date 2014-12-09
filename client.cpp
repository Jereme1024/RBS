#ifndef _CLIENT_HPP_
#define _CLIENT_HPP_

/// @file
/// @author Jeremy
/// @version 1.0
/// @section DESCRIPTION
///
/// The client class is able to handle socket operation to build a server. It accepts a Service policy with "run" interface.

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <functional>

#include <vector>
#include <arpa/inet.h>
#include <netdb.h>

const int MAX_CONNECTION = 5;
const int MAX_CLIENT = 31;
const int MAX_NAME = 32;


template <class Service>
class Client : public Service
{
protected:
	int sockfd; // listen fd
	int portno;
	struct sockaddr_in server_addr;
	struct hostent *server;

public:
	/// @brief This method is used to initialize a server by following socket -> bind -> listen(...).
	/// @param port A port number with a default value 5487.
	Client(char *ip, int port = 5487)
		: portno(port)
	{
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd < 0)
		{
			std::cerr << "Server socket open failed.\n";
			exit(EXIT_FAILURE);
		}

		server = gethostbyname(ip);
		if (server == NULL) {
			std::cerr << "Server host by name failed.\n";
			exit(EXIT_FAILURE);
		}

		memset(&server_addr, '0', sizeof(server_addr));
		server_addr.sin_family = AF_INET;
		server_addr.sin_addr = *((struct in_addr *)server->h_addr);
		server_addr.sin_port = htons(portno);

		if (connect(sockfd, (sockaddr *) &server_addr, sizeof(server_addr)) < 0)
		{
			std::cerr << "Connect failed.\n";
			exit(EXIT_FAILURE);
		}

		Service::enter(sockfd, server_addr);
	}


	/// @brief This method is used to accept clients and perform a Service run operation. It will control the fork/wait(...) operations at the same time.
	void run()
	{
		Service::routine(sockfd);
	}
};
#endif