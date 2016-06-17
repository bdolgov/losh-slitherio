#include "userdb.hpp"
#include <fstream>
#include <sstream>
#include <vector>

using namespace userdb;
using namespace std;

user_db::user_db(const string& filename)
{
	ifstream in(filename);
	if (!in)
	{
		throw std::runtime_error("Users file not found");
	}

	string line;

	while (getline(in, line))
	{
		if (line.empty() || line[0] == '#')
		{
			continue;
		}
		stringstream ss(line);
		string login, password; int level;
		if (!(ss >> login >> password >> level))
		{
			throw std::runtime_error("Bad users file");
		}
		users[login] = make_tuple(password, level);
	}
}

bool user_db::authen(const string& login, const string& password, int role)
{
	auto it = users.find(login);
	if (it == users.end()) return false;
	return get<0>(it->second) == password && get<1>(it->second) >= role;
}
