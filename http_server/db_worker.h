#include <pqxx/pqxx>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string_regex.hpp>

using IdCount = std::pair<int, int>;

class DB_Worker {
public:
	DB_Worker();
	DB_Worker(std::string connStr);
	void Connect(std::string connStr);
	void CountMatches(std::string& strRequest);
	void PickMostFrequent(std::list<std::string>& links);
	bool IsNoMatches();

private:
	pqxx::connection* DB_Connection;
	// pair<link_id, count>
	std::vector<IdCount> LinkIds;
};