#ifndef __HTTPDSERVICE_HPP__
#define __HTTPDSERVICE_HPP__

#include <sys/types.h>
#include <iostream>
#include <fstream>
#include <memory>
#include "parser.hpp"
#include "rbs.hpp"

#ifdef WIN32
#include "wins_adaptor.hpp"
#endif

struct HtmlMeta
{
	std::string script_name;
	std::string script_path;
	std::string script_type;
	std::string query_string;
	std::string request_method;

	HtmlMeta(std::string str)
	{
		parse(str);
	}

	void parse(std::string &str)
	{
		// "GET /what.file?param1=val1&param2=val2...&paramN=valN HTTP/1.1"
		auto by_blank = SimpleParser::split(str);
		auto by_question = SimpleParser::split(by_blank[1], "?");

		request_method = by_blank[0];
		script_name = by_question[0];
		script_path = "." + by_question[0];
		script_type = script_name.c_str() + script_name.find_last_of(".") + 1;
		if (by_question.size() > 1)
			query_string = by_question[1];
	}
};

class HttpdService
{
private:
	HttpdService()
		: sin_sb(), sout_sb()
	{
		std::cout.rdbuf(&sout_sb);
		std::cin.rdbuf(&sin_sb);
	}

	static HttpdService *instance;

	enum FD_CTL{ IO_BOTH = 0, IN_ONLY, OUT_ONLY };

public:
	SocketInStreamBuf sin_sb;
	SocketOutStreamBuf sout_sb;

	typedef std::shared_ptr<RbsWin32Cgi> CgiPtr;
	std::map<int, bool> browsers;
	std::map<int, CgiPtr> cgi_list;

	static HttpdService &get_instance()
	{
		return *instance;
	}

	inline void replace_fd(int clientfd, int type = 0)
	{
		switch (type)
		{
		case IO_BOTH:
			sout_sb.set_sockid(clientfd);
			std::cout.rdbuf(&sout_sb);
		case IN_ONLY:
			sin_sb.set_sockid(clientfd);
			std::cin.rdbuf(&sin_sb);
			break;
		case OUT_ONLY:
			sout_sb.set_sockid(clientfd);
			std::cout.rdbuf(&sout_sb);
			break;
		}

	}

	void enter(int clientfd, sockaddr_in &client_addr)
	{
		replace_fd(clientfd);
		browsers[clientfd] = false;

		std::cout << "HTTP/1.1 200 OK\n";
	}

	void enter(int clientfd)
	{
		replace_fd(clientfd);
		browsers[clientfd] = false;

		std::cout << "HTTP/1.1 200 OK\n";
	}

	void routine(int clientfd)
	{
		if (is_browser(clientfd))
		{
			if (browsers[clientfd] != true)
			{
				replace_fd(clientfd);
				handle_new(clientfd);
			}
		}
		else
		{
			replace_fd(clientfd, IN_ONLY);
			handle_old(clientfd);
		}
	}

	inline bool is_browser(int clientfd)
	{
		return (browsers.find(clientfd) != browsers.end());
	}

	void on_connect(int clientfd)
	{
		for (auto &cgi : cgi_list)
		{
			cgi.second->on_connect(clientfd);

			if (cgi.second->is_leave())
			{
				disconnect(cgi.second->browser_fd);
				cgi_list.erase(cgi.first);

				if (cgi_list.size() == 0) break;
			}
		}
	}

	void handle_old(int clientfd)
	{
		for (auto &cgi : cgi_list)
		{
			if (cgi.second->is_mine(clientfd))
			{
				replace_fd(cgi.second->browser_fd, OUT_ONLY);
				cgi.second->on_receive(clientfd);

				if (cgi.second->is_leave())
				{
					disconnect(cgi.second->browser_fd);
					cgi_list.erase(cgi.first);
				}

				break;
			}
		}
	}

	void handle_new(int clientfd)
	{
		std::string input;
		std::vector<std::string> requests;

		while (std::getline(std::cin, input))
		{
			if (!std::cin.good() || std::cin.eof()) break;

			requests.push_back(input);
		}

		browsers[clientfd] = true;

		if (requests.size() > 0)
			dispatch(requests[0], clientfd);
		else
		{
			disconnect(clientfd);
			std::cerr << "Information fragment." << std::endl;
		}
	}

	void disconnect(int clientfd)
	{
		if (is_browser(clientfd))
		{
			cgi_list.erase(clientfd);
			browsers.erase(clientfd);
		}
		
		closesocket(clientfd);
	}

	void dispatch(std::string request, int clientfd)
	{
		auto html_meta = HtmlMeta(request);

		if (html_meta.script_type == "cgi")
		{
			open_cgi(html_meta, clientfd);
			return;
		}

		if (!is_file_exist(html_meta.script_path))
		{
			std::cout << "Content-type: text/html; charset=utf-8\n\n";
			std::cout << "<h1>404 Not found! ヾ(`・ω・´)</h1>\n";
			disconnect(clientfd);
		}
		else
		{
			if (html_meta.script_type == "cgi")
			{
				open_cgi(html_meta, clientfd);
			}
			else if (html_meta.script_type == "html")
			{
				open_html(html_meta, clientfd);
			}
			else
			{
				std::cout << "Content-type: text/html; charset=utf-8\n\n";
				std::cout << "<h1>403 Forbidden! ヾ(`・ω・´)</h1>\n";
				disconnect(clientfd);
			}
		}
	}

	inline bool is_file_exist(std::string &filename)
	{
		const bool is_exist = (access((char *)filename.c_str(), F_OK) == 0);
		return is_exist;
	}

	void open_html(HtmlMeta &html_meta, int clientfd)
	{
		std::fstream html_context(html_meta.script_path);
		std::string text;

		std::cout << "Content-type: text/html\n\n";

		while (html_context >> text)
		{
			std::cout << text << "\n";
		}

		html_context.close();

		disconnect(clientfd);
	}


	void open_cgi(HtmlMeta &html_meta, int clientfd)
	{
		char **argv = new char *[2];
		argv[1] = NULL;

		argv[0] = (char *)html_meta.script_path.c_str();

		set_environment(html_meta);

		auto cgi_ptr = execvp_cgi(argv[0], argv);
		cgi_ptr->browser_fd = clientfd;
		cgi_list[clientfd] = cgi_ptr;
	}

	CgiPtr execvp_cgi(char *path = NULL, char *argv[] = NULL)
	{
		auto rbs_win32_cgi = std::make_shared<RbsWin32Cgi>();

		rbs_win32_cgi->set_query_string(getenv("QUERY_STRING"));
		rbs_win32_cgi->establish_connection();
		rbs_win32_cgi->main();

		return rbs_win32_cgi;
	}

	void set_environment(HtmlMeta &html_meta)
	{
		setenv("QUERY_STRING", html_meta.query_string.c_str(), 1);
		setenv("CONTENT_LENGTH", "123", 1);
		setenv("REQUEST_METHOD", html_meta.request_method.c_str(), 1);
		setenv("SCRIPT_NAME", html_meta.script_name.c_str(), 1);
		setenv("REMOTE_HOST", "somewhere.nctu.edu.tw", 1);
		setenv("REMOTE_ADDR", "140.113.1.1", 1);
		setenv("AUTH_TYPE", "auth_type", 1);
		setenv("REMOTE_USER", "remote_user", 1);
		setenv("REMOTE_IDENT", "remote_ident", 1);
	}
};

HttpdService *HttpdService::instance = new HttpdService();

#endif