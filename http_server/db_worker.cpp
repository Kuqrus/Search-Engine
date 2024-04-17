#include "db_worker.h"

DB_Worker::DB_Worker()
{
}

DB_Worker::DB_Worker(std::string connStr)
{
	DB_Connection = new pqxx::connection(connStr);
}

void DB_Worker::Connect(std::string connStr)
{
	DB_Connection = new pqxx::connection(connStr);
}

void DB_Worker::CountMatches(std::string& strRequest)
{
	LinkIds.clear();

	boost::algorithm::to_lower(strRequest, std::locale());
	boost::algorithm::replace_all_regex(strRequest, boost::regex("[[:punct:]]"), std::string{ "|" });
	boost::trim_if(strRequest, boost::is_punct(std::locale()));
	boost::algorithm::replace_all_regex(strRequest, boost::regex("\\|{2,}"), std::string{ "|" });

	std::list<std::string> request;
	boost::split(request, strRequest, boost::is_any_of("|"));
	
	try
	{
		for (const std::string& word : request) {
			pqxx::work Count(*DB_Connection);
			pqxx::result res = Count.exec("SELECT link_id, count FROM words WHERE word='" + word + "' ORDER BY count DESC");
			for (const auto& row : res) {
				int link_id = row["link_id"].as<int>();
				int count = row["count"].as<int>();

				auto IsAdded = std::find_if(LinkIds.begin(), LinkIds.end(), [&](IdCount& el) {return el.first == link_id; });
				if (IsAdded != LinkIds.end()) {
					(*IsAdded).second += count;
				}
				else {
					LinkIds.push_back(std::make_pair(link_id, count));
				}
			}
		}
	}
	catch (const std::exception& e)
	{
		throw std::exception(e);
	}
	std::sort(LinkIds.begin(), LinkIds.end(), [](const IdCount& a, const IdCount& b) {return a.second > b.second; });
}

void DB_Worker::PickMostFrequent(std::list<std::string>& links)
{
	try
	{
		pqxx::work PickLinks(*DB_Connection);
		for (const auto& l : LinkIds) {
			for (const auto [link] : PickLinks.query<std::string>("SELECT link FROM links WHERE id='" + std::to_string(l.first) + "'")) {
				links.push_back(link);
			}
			if (links.size() >= 10) {
				break;
			}
		}
		PickLinks.commit();
	}
	catch (const std::exception& e)
	{
		throw std::exception(e);
	}
}

bool DB_Worker::IsNoMatches()
{
	return LinkIds.empty();
}
