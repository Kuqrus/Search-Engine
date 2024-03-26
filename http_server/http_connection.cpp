#include "http_connection.h"

#include <sstream>
#include <iomanip>
#include <locale>
#include <codecvt>
#include <iostream>

#include <pqxx/pqxx>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;


std::string url_decode(const std::string& encoded) {
	std::string res;
	std::istringstream iss(encoded);
	char ch;

	while (iss.get(ch)) {
		if (ch == '%') {
			int hex;
			iss >> std::hex >> hex;
			res += static_cast<char>(hex);
		}
		else {
			res += ch;
		}
	}

	return res;
}

std::string convert_to_utf8(const std::string& str) {
	std::string url_decoded = url_decode(str);
	return url_decoded;
}

HttpConnection::HttpConnection(tcp::socket socket)
	: socket_(std::move(socket))
{
}


void HttpConnection::start()
{
	readRequest();
	checkDeadline();
}


void HttpConnection::readRequest()
{
	auto self = shared_from_this();

	http::async_read(
		socket_,
		buffer_,
		request_,
		[self](beast::error_code ec,
			std::size_t bytes_transferred)
		{
			boost::ignore_unused(bytes_transferred);
			if (!ec)
				self->processRequest();
		});
}

void HttpConnection::processRequest()
{
	response_.version(request_.version());
	response_.keep_alive(false);

	switch (request_.method())
	{
	case http::verb::get:
		response_.result(http::status::ok);
		response_.set(http::field::server, "Beast");
		createResponseGet();
		break;
	case http::verb::post:
		response_.result(http::status::ok);
		response_.set(http::field::server, "Beast");
		createResponsePost();
		break;

	default:
		response_.result(http::status::bad_request);
		response_.set(http::field::content_type, "text/plain");
		beast::ostream(response_.body())
			<< "Invalid request-method '"
			<< std::string(request_.method_string())
			<< "'";
		break;
	}

	writeResponse();
}


void HttpConnection::createResponseGet()
{
	if (request_.target() == "/")
	{
		response_.set(http::field::content_type, "text/html");
		beast::ostream(response_.body())
			<< "<html>\n"
			<< "<head><meta charset=\"UTF-8\"><title>Search Engine</title></head>\n"
			<< "<body>\n"
			<< "<h1>Search Engine</h1>\n"
			<< "<p>Welcome!<p>\n"
			<< "<form action=\"/\" method=\"post\">\n"
			<< "    <label for=\"search\">Search:</label><br>\n"
			<< "    <input type=\"text\" id=\"search\" name=\"search\"><br>\n"
			<< "    <input type=\"submit\" value=\"Search\">\n"
			<< "</form>\n"
			<< "</body>\n"
			<< "</html>\n";
	}
	else
	{
		response_.result(http::status::not_found);
		response_.set(http::field::content_type, "text/plain");
		beast::ostream(response_.body()) << "File not found\r\n";
	}
}

void HttpConnection::createResponsePost()
{

	if (request_.target() == "/")
	{		
		std::string ParamsForDBConnection;
		try
		{
			Parser p("config/config.ini");
			ParamsForDBConnection = "host=" + p.GetValue("[DataBase]", "Host") +
									" port=" + p.GetValue("[DataBase]", "Port") +
									" dbname=" + p.GetValue("[DataBase]", "Name") +
									" user=" + p.GetValue("[DataBase]", "User") +
									" password=" + p.GetValue("[DataBase]", "Password");
		}
		catch (std::string& s)
		{
			std::cout << s << std::endl;
			return;
		}
		pqxx::connection DB_Connection(ParamsForDBConnection);

		std::string s = buffers_to_string(request_.body().data());

		size_t pos = s.find('=');
		if (pos == std::string::npos)
		{
			response_.result(http::status::not_found);
			response_.set(http::field::content_type, "text/plain");
			beast::ostream(response_.body()) << "File not found\r\n";
			return;
		}

		std::string key = s.substr(0, pos);
		std::string value = s.substr(pos + 1);

		std::string utf8value = convert_to_utf8(value);

		boost::algorithm::replace_all_regex(utf8value, boost::regex("[[:punct:]]"), std::string{ "|" });
		boost::trim_if(utf8value, boost::is_punct(std::locale()));
		boost::algorithm::replace_all_regex(utf8value, boost::regex("\\|{2,}"), std::string{ "|" });

		if (utf8value.find('|') != std::string::npos) { utf8value = '(' + utf8value + ')'; }


		if (key != "search")
		{
			response_.result(http::status::not_found);
			response_.set(http::field::content_type, "text/plain");
			beast::ostream(response_.body()) << "File not found\r\n";
			return;
		}

		pqxx::result res;
		std::multimap<int, int> Count_Id;
		try
		{
			pqxx::work txn(DB_Connection);
			res = txn.exec("SELECT "
				"link_id, "
				"COUNT(*) AS count_one "
				"FROM (SELECT link_id, regexp_matches(data, '\\y" + utf8value + "\\y', 'g') FROM data) AS matches "
				"GROUP BY link_id"
			);


			for (const auto& row : res) {
				int count = row["count_one"].as<int>();
				int id = row["link_id"].as<int>();
				Count_Id.insert({ count, id });
			}
			txn.commit();
		}
		catch (const std::exception& e)
		{
			std::cout << e.what() << std::endl;
			return;
		}		

		response_.set(http::field::content_type, "text/html");
		beast::ostream(response_.body())
			<< "<html>\n"
			<< "<head><meta charset=\"UTF-8\"><title>Search Engine</title></head>\n"
			<< "<body>\n"
			<< "<h1>Search Engine</h1>\n"
			<< "<p>Response:<p>\n"
			<< "<ul>\n";

		if (Count_Id.empty()) {
			beast::ostream(response_.body())
				<< "Nothing was found by your search request!";
		}
		else {
			std::vector<std::string> searchResult;
			try
			{
				pqxx::work CollectLink(DB_Connection);
				for (auto i = Count_Id.rbegin(); i != Count_Id.rend(); i++) {
					std::string ID = std::to_string(i->second);
					for (const auto& [link] : CollectLink.query<std::string>("SELECT link FROM links WHERE id='" + ID + "'")) {
						searchResult.push_back(link);
					}
					if (searchResult.size() >= 10) {
						break;
					}
				}
				CollectLink.commit();
			}
			catch (const std::exception& e)
			{
				std::cout << e.what() << std::endl;
				return;
			}

			for (const auto& url : searchResult) {

				beast::ostream(response_.body())
					<< "<li><a href=\""
					<< url << "\">"
					<< url << "</a></li>";
			}
		}

		beast::ostream(response_.body())
			<< "</ul>\n"
			<< "</body>\n"
			<< "</html>\n";
	}
	else
	{
		response_.result(http::status::not_found);
		response_.set(http::field::content_type, "text/plain");
		beast::ostream(response_.body()) << "File not found\r\n";
	}
}

void HttpConnection::writeResponse()
{
	auto self = shared_from_this();

	response_.content_length(response_.body().size());

	http::async_write(
		socket_,
		response_,
		[self](beast::error_code ec, std::size_t)
		{
			self->socket_.shutdown(tcp::socket::shutdown_send, ec);
			self->deadline_.cancel();
		});
}

void HttpConnection::checkDeadline()
{
	auto self = shared_from_this();

	deadline_.async_wait(
		[self](beast::error_code ec)
		{
			if (!ec)
			{
				self->socket_.close(ec);
			}
		});
}

