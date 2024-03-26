#pragma once 
#include <vector>
#include <string>
#include "link.h"

#include <pqxx/pqxx>



std::string getHtmlContent(const Link& link);

void parseHTML(const std::string& html, std::string& returnString);

void parseHref(const Link& HomeLink, std::vector<Link>& links, std::string& input);
