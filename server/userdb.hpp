#ifndef USERDB_HPP
#define USERDB_HPP

#include <string>
#include <map>
#include <tuple>

namespace userdb
{
	enum { roleNull, rolePlayer, roleAdmin };

	class user_db
	{
		public:
			user_db(const std::string& path);
			bool authen(const std::string& login, const std::string& password, int role);

		private:
			std::map<std::string, std::tuple<std::string, int>> users;
	};
}

#endif
