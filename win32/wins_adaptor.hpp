#ifndef __WINS_ADAPTOR_HPP__
#define __WINS_ADAPTOR_HPP__

#include <stdlib.h>
#include <windows.h>

#define F_OK 0

int setenv(const char *name, const char *value, int overwrite)
{
	int errcode = 0;
	if (!overwrite)
	{
		size_t envsize = 0;
		errcode = getenv_s(&envsize, NULL, 0, name);
		if (errcode || envsize) return errcode;
	}

	return _putenv_s(name, value);
}

/// @return int(-1) when open file failed, int(0) when open successly.
int access(char *filename, int flags = 0)
{
	ifstream fin(filename);
	if (!fin)
		return -1;
	fin.close();
	return 0;
}

inline void dup2(int doing, int nothing)
{
	return;
}



#include <streambuf>

struct SocketIdUnset : public std::exception
{
	void operator()()
	{
		std::cerr << "Sockid is unset!\n";
	}
};

class SocketOutStreamBuf : public std::streambuf
{
public:
	int sockid;

	SocketOutStreamBuf(int sockfd) : sockid(sockfd)
	{}

	SocketOutStreamBuf() : sockid(-1)
	{}

	void set_sockid(int id)
	{
		sockid = id;
	}

	int_type overflow(int_type c)
	{
		try
		{
			if (sockid == -1) throw SocketIdUnset();
		}
		catch (SocketIdUnset &except)
		{
			except();
			return EOF;
		}

		if (c != EOF)
		{
			if (send(sockid, (char *)&c, 1, 0) <= 0)
				return EOF;
		}

		return c;
	}
};

class SocketInStreamBuf : public std::streambuf
{
public:
	int sockid;

	SocketInStreamBuf(int sockfd) : sockid(sockfd)
	{}

	SocketInStreamBuf() : sockid(-1)
	{}

	void set_sockid(int id)
	{
		sockid = id;
	}

	int_type underflow()
	{
		return flow_impl(MSG_PEEK);
	}

	int_type uflow()
	{
		return flow_impl();
	}

	inline int_type flow_impl(int flag = 0)
	{
		try
		{
			if (sockid == -1) throw SocketIdUnset();
		}
		catch (SocketIdUnset &except)
		{
			except();
			return EOF;
		}

		char c;

		if (recv(sockid, &c, 1, flag) <= 0)
		{
			return EOF;
		}

		return c;
	}
};

void sleep(unsigned int second)
{
	Sleep(second * 1000);
}

#endif