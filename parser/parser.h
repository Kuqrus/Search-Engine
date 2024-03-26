#include <string>
#include <fstream>
#include <map>

class Parser {
public:
	Parser(std::string FileName);

	std::string GetValue(std::string Section, std::string Name);

private:
	std::ifstream IniFile;
	std::map<std::string, std::map<std::string, std::string>> ParsedData;

	void ParseData();
};