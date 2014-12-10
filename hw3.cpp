#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <memory>

#include "client.hpp"
#include "parser.hpp"

const int MAXUSER = 5;

class BatchExec
{
public:
	std::fstream batch;
	bool is_writeable;
	bool is_exit;
	int serverfd;
	int id;

public:
	BatchExec()
		: is_writeable(false), is_exit(false), serverfd(-1), id(0)
	{}

	int contain_prompt ( char* line )
	{
		int i, prompt = 0 ;
		for (i=0; line[i]; ++i) {
			switch ( line[i] ) {
				case '%' : prompt = 1 ; break;
				case ' ' : if ( prompt ) return 1;
				default: prompt = 0;
			}
		}
		return 0;
	} 


	int recv_msg()
	{
		int from = serverfd;
		char buf[3000],*tmp;
		int len,i;

		len=read(from,buf,sizeof(buf)-1);
		if(len < 0) return -1;

		buf[len] = 0;
		if(len>0)
		{
			for(tmp=strtok(buf,"\n"); tmp; tmp=strtok(NULL,"\n"))
			{
				if ( contain_prompt(tmp) ) is_writeable = true ;
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
			write(serverfd, buffer.c_str(), buffer.length());
			is_writeable = false;

			if (buffer.find("exit") != std::string::npos)
			{
				is_exit = true;
			}
		}
	}

	void remove_return_symbol(std::string &text)
	{
		for (int i = 0; i < text.length(); i++)
			if (text[i] == '\r') text[i] = ' ';
	}

	inline void print_html(std::string plaintext, const bool is_border = false)
	{
		remove_return_symbol(plaintext);

		std::cout << "<script>document.all['m" << id <<"'].innerHTML += \"";
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


	void routine(int serverfd)
	{
		return;
	}
};

class Hw3Cgi
{
public:
	typedef std::shared_ptr<Client<BatchExec>> ClientPtr;
	std::vector<ClientPtr> clients;
	std::map<std::string, std::string> params;
	fd_set rfds, afds;

	Hw3Cgi()
	{
		FD_ZERO(&rfds);
		FD_ZERO(&afds);
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

				clients.push_back(std::make_shared<Client<BatchExec>>(ip_, port_));
				clients.back()->set_batch(params[batch]);
				clients.back()->id = (i - 1);

				const int fd = clients.back()->serverfd;
				FD_SET(fd, &afds);
			}
		}
	}

	void main()
	{
		int connection = clients.size();
		int nfds = getdtablesize();

		show_html_top();

		while (true)
		{
			memcpy(&rfds, &afds, sizeof(fd_set));

			if(select(nfds, &rfds, NULL, NULL, NULL) < 0)
			{
				perror("select");
			}

			for (auto &client : clients)
			{
				if (FD_ISSET(client->serverfd, &rfds))
				{
					client->recv_msg();
					client->send_cmd();

					if (client->is_exit)
					{
						FD_CLR(client->serverfd, &afds);
						close(client->serverfd);

						connection--;
					}
				}
			}

			if (connection <= 0)
				break;
		}

		show_html_bottom();
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

		for (int i = 0; i < MAXUSER; i++)
		{
			bool is_printed = false;
			for (auto &client : clients)
			{
				if (client->id == i)
				{
					std::cout << "<td>" << client->ip << "</td>";
					is_printed = true;
					break;
				}
			}
			if (!is_printed)
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
};

int main(int argc, char *argv[])
{
	std::cerr << "QUERY_STRING: " << getenv("QUERY_STRING") << std::endl;

	std::cout << "Content-Type: text/html\n\n";

	Hw3Cgi hw3cgi;
	
	hw3cgi.set_query_string(getenv("QUERY_STRING"));
	hw3cgi.establish_connection();
	hw3cgi.main();

	return 0;
}
