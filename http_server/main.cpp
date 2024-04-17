#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>

#include <string>
#include <iostream>

#include "http_connection.h"
#include <Windows.h>


void httpServer(tcp::acceptor& acceptor, tcp::socket& socket, const std::string& ConnectionData)
{
	acceptor.async_accept(socket,
		[&](beast::error_code ec)
		{
			if (!ec)
				std::make_shared<HttpConnection>(std::move(socket))->start(ConnectionData);
			httpServer(acceptor, socket, ConnectionData);
		});
}


int main(int argc, char* argv[])
{
	SetConsoleCP(CP_UTF8);
	SetConsoleOutputCP(CP_UTF8);	

	try
	{
		Parser p("config/config.ini");
		auto const address = net::ip::make_address("0.0.0.0");
		unsigned short port = std::stoi(p.GetValue("[Searcher]", "Port"));

		std::string ConnectionData = "host=" + p.GetValue("[DataBase]", "Host") +
			" port=" + p.GetValue("[DataBase]", "Port") +
			" dbname=" + p.GetValue("[DataBase]", "Name") +
			" user=" + p.GetValue("[DataBase]", "User") +
			" password=" + p.GetValue("[DataBase]", "Password");

		net::io_context ioc{1};

		tcp::acceptor acceptor{ioc, { address, port }};
		tcp::socket socket{ioc};
		httpServer(acceptor, socket, ConnectionData);

		std::cout << "Open browser and connect to http://localhost:8080 to see the web server operating" << std::endl;

		ioc.run();
	}
	catch (std::string const& s)
	{
		std::cerr << "Error: " << s << std::endl;
		return EXIT_FAILURE;
	}
	catch (std::exception const& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}
}