#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>

#include "http_utils.h"
#include <functional>

#include "parser.h"

std::unordered_set<std::string> ViewedLinks;

std::mutex mtx;
std::condition_variable cv;
std::queue<std::function<void()>> tasks;
bool exitThreadPool = false;


void CollectContent(std::string& url, std::string& html, pqxx::connection& dbConn) {
	std::string returnString;
	parseHTML(html, returnString);

	// Создаем поток для чтения из строки
	std::istringstream inputStream(returnString);

	// Строка для хранения слов
	std::string DataToAppend = " ";

	// Читаем слова из строки и записываем их в вектор
	std::string word;
	while (inputStream >> word) {
		// Проверяем чтобы слово было длинне 1го и короче 32ух символов
		if (!(word.size() > 1 && word.size() < 32)) {
			continue;
		}

		// Преобразуем слово в нижний регистр
		std::string lowerWord;
		for (auto c : word) {
			lowerWord += std::tolower(c, std::locale());
		}

		// Удаляем знаки препинания
		lowerWord.erase(std::remove_if(lowerWord.begin(), lowerWord.end(), [](char c) {
			return std::ispunct(c, std::locale());
			}), lowerWord.end());

		DataToAppend += lowerWord + " ";
	}

	ViewedLinks.insert(url);

	// Добавляем в БД ссылку, которую только что распарсили
	pqxx::work addLink(dbConn);
	addLink.exec("INSERT INTO links (link) VALUES ('" + url + "')");
	addLink.commit();

	// Получаем ИД этой ссылки
	pqxx::work FindCurrId(dbConn);
	std::string CurrentUrlId;
	pqxx::result R = FindCurrId.exec("SELECT id FROM links WHERE link='" + url + "'");
	CurrentUrlId = R[0][0].c_str();
	FindCurrId.commit();

	// Добавляем в БД контент и связываем его с ИД ссылки
	pqxx::work addData(dbConn);
	addData.exec("INSERT INTO data (link_id, data) VALUES (" + CurrentUrlId + ", '" + DataToAppend + "')");
	addData.commit();
}

void threadPoolWorker() {
	std::unique_lock<std::mutex> lock(mtx);
	while (!exitThreadPool || !tasks.empty()) {
		if (tasks.empty()) {
			cv.wait(lock);
		}
		else {
			auto task = tasks.front();
			tasks.pop();
			lock.unlock();
			task();
			lock.lock();
		}
	}
}
void parseLink(const Link& CurrentLink, int depth, pqxx::connection& dbConn)
{
	try {

		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		std::string url = CurrentLink.hostName + CurrentLink.query;

		if (ViewedLinks.count(url) > 0) {
			std::cout << "Link " << url << " alredy parsed!" << std::endl;
			return;
		}

		std::string html = getHtmlContent(CurrentLink);		

		if (html.size() == 0)
		{			
			std::cout << "Failed to get HTML Content from: " << url << std::endl;
			ViewedLinks.insert(url);
			return;
		}
		
		CollectContent(url, html, dbConn);

		std::vector<Link> LinksVector;
		parseHref(CurrentLink, LinksVector, html);

		if (depth > 0) {
			std::lock_guard<std::mutex> lock(mtx);

			for (auto& subLink : LinksVector)
			{
				tasks.push([subLink, depth, &dbConn]() { parseLink(subLink, depth - 1, dbConn); });
			}
			cv.notify_one();
		}
	}
	catch (const std::exception& e)
	{
		std::cout << e.what() << std::endl;
	}
}


int main()
{	

	try {
		std::string ParamsForDBConnection;

		Parser p("config/config.ini");
		ParamsForDBConnection = "host=" + p.GetValue("[DataBase]", "Host") +
								" port=" + p.GetValue("[DataBase]", "Port") +
								" dbname=" + p.GetValue("[DataBase]", "Name") +
								" user=" + p.GetValue("[DataBase]", "User") +
								" password=" + p.GetValue("[DataBase]", "Password");
		pqxx::connection DB_Connection(ParamsForDBConnection);

		int Depth = std::stoi(p.GetValue("[Spider]", "Depth"));
		std::string StartLink = p.GetValue("[Spider]", "StartLink");
		std::string Host = StartLink.substr(0, StartLink.find("/"));
		std::string Query = StartLink.substr(StartLink.find("/"), StartLink.size());

		pqxx::work start(DB_Connection);
		start.exec("create table if not exists links("
			"id serial primary key, "
			"link text not null"
			");"
		);
		start.exec("create table if not exists data("
			"id serial primary key, "
			"link_id INT REFERENCES links(id),"
			"data text not null"
			");"
		);
		start.commit();

		
		int numThreads = std::thread::hardware_concurrency();
		std::vector<std::thread> threadPool;

		for (int i = 0; i < numThreads; ++i) {
			threadPool.emplace_back(threadPoolWorker);
		}

		Link link{ ProtocolType::HTTPS, Host, Query };

		{
			std::lock_guard<std::mutex> lock(mtx);
			tasks.push([link, Depth, &DB_Connection]() { parseLink(link, Depth, DB_Connection); });
			cv.notify_one();
		}

		std::this_thread::sleep_for(std::chrono::seconds(2));

		{
			std::lock_guard<std::mutex> lock(mtx);
			exitThreadPool = true;
			cv.notify_all();
		}

		for (auto& t : threadPool) {
			t.join();
		}
	}
	catch (std::string& s)
	{
		std::cout << s << std::endl;
	}
	catch (const std::exception& e)
	{
		std::cout << e.what() << std::endl;
	}

	return 0;
}
