/*!
 * @file
 * @author Jackson McNeill
 *
 * A small network abstraction layer, using Unix sockets. No windows equivilent available. 
 */
#pragma once 

#include <stdio.h> 
#include <string>
#include <memory>

#include <netinet/in.h> 

namespace network
{
	/*!
	 * A TCP socket abstraction
	 */
	class socket
	{
		int socketfd;
		struct sockaddr_in addr;

		unsigned short port;
		public:
			socket(unsigned short port);

			inline socket(socket&& other) :
				socketfd(std::exchange(other.socketfd,-1)),
				addr(std::move(other.addr)),
				port(std::move(other.port))
			{
			}
			~socket();
			inline socket& operator=(socket&& other)
			{
				socketfd = (std::exchange(other.socketfd,-1));
				addr = (std::move(other.addr));
				port = std::move(other.port);
				return *this;
			}

			socket(socket const & other) = delete;
			socket& operator=(socket const & other) = delete;

			void bind_as_server();
			void connect_to_server(std::string hostname);

			/*!
			 * Connects (listens for) a client to your server. Blocks. 
			 *
			 * Returns a socket that is for the client; i.e. calling write on it will write to the client connected
			 */
			socket listen_for_client();
			std::string read(size_t amnt_data);
			void write(std::string data);
	};
}
