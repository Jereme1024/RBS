#ifndef __RBS_HPP__
#define __RBS_HPP__

#include <windows.h>

#include <map>
#include <vector>
#include <memory>
#include <iostream>
#include <fstream>

#include "parser.hpp"

#define WM_SOCKET_NOTIFY (WM_USER + 1)

template <class Service>
class Client : public Service
{
public:
	int sockfd; // server fd
	int portno;
	char *ip;
	int err;
	struct sockaddr_in server_addr;
	struct hostent *server;
	bool is_connected_flag;

	Client() : is_connected_flag(false)
	{}

	Client(char *ip_in, int port = 5487)
		: ip(ip_in)
		, portno(port)
		, is_connected_flag(false)
	{
		connect(ip, portno);
	}

	bool connect(char *ip_in, int port = 5487)
	{
		ip = ip_in;
		portno = port;

		auto &hwnd = GlobalHwnd::get_instance();

		sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sockfd == INVALID_SOCKET)
		{
			WSACleanup();
			return TRUE;
		}

		err = WSAAsyncSelect(sockfd, hwnd, WM_SOCKET_NOTIFY, FD_CLOSE | FD_READ | FD_WRITE | FD_CONNECT);

		if (err == SOCKET_ERROR) {
			closesocket(sockfd);
			WSACleanup();
			return TRUE;
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

		::connect(sockfd, (sockaddr *)&server_addr, sizeof(server_addr));

		return true;
	}


	bool on_connect(int sockid)
	{
		::connect(sockfd, (sockaddr *)&server_addr, sizeof(server_addr));
		int err = WSAGetLastError();
		if (err == WSAEWOULDBLOCK)
		{
			/*
			const bool border = true;
			Service::print_html("Connect timeout!", border);
			Service::is_exit_flag = true;

			return false;
			*/
		}
		else
		{
			is_connected_flag = true;
			Service::enter(sockid);
			Service::recv_msg();
			Service::send_cmd();
		}

		return true;
	}

};


class BatchExec
{
public:
	std::fstream batch;
	bool is_writeable;
	bool is_exit_flag;
	int serverfd;
	int id;

public:
	BatchExec()
		: batch(), is_writeable(false), is_exit_flag(false), serverfd(-1), id(0)
	{}

	int contain_prompt(char* line)
	{
		int i, prompt = 0;
		for (i = 0; line[i]; ++i) {
			switch (line[i]) {
			case '%': prompt = 1; break;
			case ' ': if (prompt) return 1;
			default: prompt = 0;
			}
		}
		return 0;
	}

	inline bool is_exit()
	{
		return is_exit_flag;
	}

	int recv_msg()
	{
		int from = serverfd;
		char buf[3000], *tmp;
		int len;

		len = recv(from, buf, sizeof(buf)-1, 0);
		if (len < 0) return -1;

		buf[len] = 0;
		if (len>0)
		{
			for (tmp = strtok(buf, "\n"); tmp; tmp = strtok(NULL, "\n"))
			{
				if (contain_prompt(tmp)) is_writeable = true;
				print_html(tmp);
			}
		}
		fflush(stdout);
		return len;
	}


	void send_cmd()
	{
		const bool border = true;

		if (is_writeable && batch)
		{
			std::string buffer;
			std::getline(batch, buffer);

			print_html(buffer, border);

			buffer.append("\n");
			send(serverfd, buffer.c_str(), buffer.length(), 0);
			is_writeable = false;

			if (buffer.find("exit") != std::string::npos)
			{
				is_exit_flag = true;
			}
		}
	}

	void remove_return_symbol(std::string &text)
	{
		for (unsigned int i = 0; i < text.length(); i++)
		if (text[i] == '\r') text[i] = ' ';
	}

	inline void print_html(std::string plaintext, const bool is_border = false)
	{
		remove_return_symbol(plaintext);

		auto sb = std::cout.rdbuf();

		std::cout << "<script>document.all['m" << id << "'].innerHTML += \"";
		if (is_border) std::cout << "<b>";
		std::cout << plaintext;
		if (is_border) std::cout << "</b>";

		if (plaintext[0] != '%')
			std::cout << "<br/>";
		std::cout << "\";</script>" << std::endl;
	}


	void set_batch(std::string filename)
	{
		batch.open(filename);

		if (!batch)
		{
			std::cerr << "Batch file open failed! " << filename << std::endl;
			exit(-1);
		}
	}


	void enter(int serverfd, sockaddr_in &server_addr)
	{
		this->serverfd = serverfd;
	}

	void enter(int serverid)
	{
		this->serverfd = serverid;
	}

	void routine(int serverfd)
	{
		return;
	}
};

class RbsWin32Cgi
{
public:
	typedef Client<BatchExec> ClientType;
	typedef std::shared_ptr<ClientType> ClientPtr;

	std::map<int, ClientPtr> clients;

	std::map<std::string, std::string> params;

	static const int MAXUSER = 5;

	int browser_fd;
	
	RbsWin32Cgi() : browser_fd(-1)
	{}

	~RbsWin32Cgi()
	{
		for (auto &c : clients)
		{
			const int fd = c.first;
			closesocket(fd);
		}

		clients.clear();
	}

	void set_query_string(std::string query_string)
	{
		auto by_and = SimpleParser::split(query_string, "&");

		for (auto &str : by_and)
		{
			auto by_equal = SimpleParser::split(str, "=");

			if (by_equal.size() == 2)
				params[by_equal[0]] = by_equal[1];
			else
				params[by_equal[0]] = "";
		}
	}

	inline bool is_mine(int sockid)
	{
		return (clients.find(sockid) != clients.end());
	}

	void establish_connection()
	{
		for (int i = 1; i <= MAXUSER; i++)
		{
			std::string index = std::to_string(i);
			std::string ip = "h" + index;
			std::string port = "p" + index;
			std::string batch = "f" + index;

			std::cerr << "ip = " << params[ip] << " port = " << params[port] << " batch = " << params[batch] << "\n";

			if (params[ip] != "" && params[port] != "" && params[batch] != "")
			{
				char *ip_ = (char *)params[ip].c_str();
				int port_ = std::atoi(params[port].c_str());

				auto client = std::make_shared<ClientType>();

				client->connect(ip_, port_);
				client->set_batch(params[batch]);
				client->id = (i - 1);

				const int sockfd = client->sockfd;
				clients[sockfd] = client;
			}
		}
	}

	void on_connect(int sockid)
	{
		if (is_mine(sockid))
		{
			bool done = clients[sockid]->on_connect(sockid);
			if (!done)
			{
				closesocket(clients[sockid]->sockfd);
				clients.erase(sockid);

				if (is_leave())
					show_html_bottom();
			}
		}
	}

	void on_receive(int sockid)
	{
		if (is_mine(sockid))
		{
			auto c = clients[sockid];

			if (c->is_connected_flag)
			{
				c->recv_msg();
				c->send_cmd();

				if (c->is_exit())
				{
					closesocket(c->sockfd);
					clients.erase(sockid);
				}
			}
		}

		if (is_leave())
			show_html_bottom();
	}

	void main()
	{
		std::cout << "Content-type: text/html\n\n";
		show_html_top();
	}

	void show_html_top()
	{
		std::cout << R"(
			<html>
			<head>
			<meta http-equiv="Content-Type" content="text/html; charset=big5" />
			<title>Network Programming Homework 3</title>
			</head>
			<body bgcolor=#336699>
			<font face="Courier New" size=2 color=#FFFF99>
			<table width="800" border="1">
			<tr>
		)";

		std::vector<char *> is_used(MAXUSER, NULL);

		for (auto &client : clients)
		{
			is_used[client.second->id] = client.second->ip;
		}

		for (int i = 0; i < MAXUSER; i++)
		{
			if (is_used[i] != NULL)
				std::cout << "<td>" << is_used[i] << "</td>";
			else
				std::cout << "<td></td>";
		}

		std::cout << R"(
			</tr>
			<tr>
		)";

		for (int i = 0; i < MAXUSER; i++)
		{
			std::cout << "<td valign=\"top\" id=\"m" << std::to_string(i) << "\"></td>";
		}


		std::cout << R"(
			</tr>
			</table>
		)";
	}

	void show_html_bottom()
	{
		std::cout << R"(
			</font>
			</body>
			</html>
		)";
	}

	inline bool is_leave()
	{
		return (clients.size() == 0);
	}
};

#endif