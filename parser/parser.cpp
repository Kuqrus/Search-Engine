#include "parser.h"

#include <iostream>

Parser::Parser(std::string FileName) {
	IniFile.open(FileName);
	if (!IniFile.is_open()) {
		std::cout << "not opened!\n";
		throw std::string{ "Could not open file!" };
	}
	ParseData();
	IniFile.close();
}

std::string Parser::GetValue(std::string Section, std::string Name) {
	if (ParsedData.find(Section) == ParsedData.end()) {
		throw std::string{ "Has no \"" + Section + "\" section" };
	}
	else if (ParsedData[Section].find(Name) == ParsedData[Section].end()) {
		throw std::string{ "Section \"" + Section + "\" has no \"" + Name + "\" value" };
	}
	return ParsedData[Section].find(Name)->second;
}

void Parser::ParseData() {
	std::string line, ParamName, ParamValue, CurrentSection;
	while (std::getline(IniFile, line)) {
		if (line.find("[") != std::string::npos) {
			CurrentSection = line;
			ParsedData[CurrentSection];
			continue;
		}
		ParamName = line.substr(0, line.find("="));
		ParamValue = line.substr(line.find("=") + 1, line.size());
		ParsedData[CurrentSection].insert(std::make_pair(ParamName, ParamValue));
	}
}