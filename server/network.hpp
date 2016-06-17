#ifndef NETWORK_HPP
#define NETWORK_HPP

#include <memory>
#include <boost/asio.hpp>
#include <map>
#include "common.hpp"


namespace game_logic { class game; class player; class field; }
namespace userdb { class user_db; }
namespace flatbuffers { class FlatBufferBuilder; }
namespace SnakeGame { class Login; class Direction; }

namespace network
{
	class server : public std::enable_shared_from_this<server>
	{
		private:
			std::map<int, std::shared_ptr<game_logic::game>> games;
			std::shared_ptr<userdb::user_db> users;
			boost::asio::ip::tcp::acceptor acceptor;
			boost::asio::ip::tcp::socket socket;
			void do_accept();

		public:
			server(boost::asio::io_service& _ios, const boost::asio::ip::tcp::endpoint& _endpoint);
			std::shared_ptr<userdb::user_db> get_users() const;
			void set_users(const std::shared_ptr<userdb::user_db>& _users);
			std::shared_ptr<game_logic::game> get_game(int field) const;
			void add_game(int field, const std::shared_ptr<game_logic::game>& game);
	};

	class connection : public std::enable_shared_from_this<connection>
	{
		private:
			std::shared_ptr<server> srv;
			boost::asio::ip::tcp::socket sock;
			char current_length_buf[4];
			std::vector<char> current_body_read_buf;
			std::vector<char> current_write_buf;
			std::shared_ptr<game_logic::game> game;
			std::shared_ptr<game_logic::player> player;
			periodic_timer timer;
			int pkg_queue;

			void do_read_header();
			void do_read_body();
			void handle_body();
			void handle_login(const SnakeGame::Login* pkg);
			void handle_direction(const SnakeGame::Direction* pkg);
			void error(const std::string& text);
			void do_send_welcome();

		public:
			connection(const std::shared_ptr<server>& _srv, boost::asio::ip::tcp::socket _sock);
			~connection();
			void start();
			void send_package(const flatbuffers::FlatBufferBuilder& fbb);
			void send_field(const std::shared_ptr<game_logic::field>& field);
	};
}

#endif
