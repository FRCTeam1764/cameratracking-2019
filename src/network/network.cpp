#include "network.hpp"

#include <unistd.h>  
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <stdlib.h> 
#include <cstring>   
#include <string>
#include <stdexcept>

#include <errno.h>

namespace network
{

	socket::socket(unsigned short port)
	{
		this->port = port;
		socketfd = ::socket(AF_INET, SOCK_STREAM, 0);

		if (socketfd < 0 )
		{
			throw std::runtime_error("couldn't open socket, error code : " +  std::string(strerror(errno)));
		}
		memset(&(addr),0,sizeof(addr));

		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_port = htons(port);
	}
	socket::~socket()
	{
		if (socketfd != -1)
		{
			close(socketfd);
		}
	}
	void socket::bind_as_server()
	{
		int bindError = bind(socketfd, (struct sockaddr *) &addr, sizeof(addr));

		if (bindError < 0)
		{
			throw std::runtime_error("failed to bind as server error code : " +  std::string(strerror(errno)));
		}
	}
	void socket::connect_to_server(std::string hostname)
	{
		printf("hn %s\n",hostname.c_str());
		struct hostent* server = gethostbyname(hostname.c_str());
		if (server == NULL) 
		{
			throw std::runtime_error("server doesn't exist");
		}
		memmove((void *)server->h_addr, (void *)&(addr).sin_addr.s_addr, server->h_length);
		int connectError = connect(socketfd, (struct sockaddr*)&addr, sizeof(addr));
		if (connectError < 0 )
		{
			throw std::runtime_error("failed to connect to server error code: " +  std::string(strerror(errno)));
		}
	}

	socket socket::listen_for_client()
	{
		listen(socketfd,1);

		socklen_t size = sizeof(addr);
		socket client(port);
		client.socketfd = accept(socketfd,(struct sockaddr *)&client.addr, &size); 
		if (socketfd == -1 )
		{
			throw std::runtime_error("");
		}
		return std::move(client);
	}
	std::string socket::read(size_t amnt_data)
	{
		std::string buffer;
		buffer.resize(amnt_data);
		int success = ::read(socketfd, reinterpret_cast<void*>(buffer.data()), amnt_data);
		if (success == -1)
		{
			throw std::runtime_error("failed to read error code: "+std::string(strerror(errno)));
		}
		return buffer;
	}
	void socket::write(std::string data)
	{
		int success = ::write(socketfd,data.c_str(),data.size());
		if (success == -1)
		{
			throw std::runtime_error("failed to write error code: "+std::string(strerror(errno)));
		}
	}
}
