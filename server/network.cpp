#include "network.hpp"
#include "common.hpp"
#include "game.hpp"
#include "userdb.hpp"

#include "snake_generated.h"

using namespace network;
using namespace std;
using namespace SnakeGame;

#define MAX_LEN 16384
#define MAX_CONNECTIONS 5

server::server(boost::asio::io_service& _ios, const boost::asio::ip::tcp::endpoint& _endpoint):
	acceptor(_ios, _endpoint),
	socket(_ios)
{
	dlog(info) << "Created server " << this;
	do_accept();
}

void server::do_accept()
{
	acceptor.async_accept(socket, [this](boost::system::error_code ec)
		{
			if (!ec)
			{
				make_shared<connection>(shared_from_this(), move(socket))->start();
			}
			else
			{
				dlog(warning) << "Accept failed on server " << this << ": " << ec.message();
			}
			do_accept();
		});
}

connection::connection(const shared_ptr<server>& _srv, boost::asio::ip::tcp::socket _sock):
	srv(_srv), sock(move(_sock)), timer(sock.get_io_service(), boost::posix_time::milliseconds(100)),
	pkg_queue(0)
{
	dlog(info) << "Created connection " << this;
}

void connection::start()
{
	dlog(info) << "Starting connection " << this;
	do_read_header();
	std::weak_ptr<connection> wp(shared_from_this());
	timer.set_cb([this, wp]()
		{
			if (!wp.expired())
			{
				send_field(game->get_current_field());
			}
		});
}

void connection::do_read_header()
{
	auto self = shared_from_this();
	boost::asio::async_read(sock, boost::asio::buffer(current_length_buf, sizeof(current_length_buf)),
		[this, self](boost::system::error_code ec, size_t)
		{
			if (!ec)
			{
				size_t len = ((static_cast<size_t>(current_length_buf[0]) * 256 
					+ current_length_buf[1]) * 256 
					+ current_length_buf[2]) * 256 
					+ current_length_buf[3];
				if (len > MAX_LEN)
				{
					error("Too big package");
					do_read_header();
				}
				else
				{
					current_body_read_buf.resize(len);
					do_read_body();
				}
			}
			else
			{
				dlog(warning) << " Read failed: " << ec.message();
			}
		});
}

void connection::do_read_body()
{
	auto self = shared_from_this();
	boost::asio::async_read(sock, boost::asio::buffer(current_body_read_buf),
		[this, self](boost::system::error_code ec, size_t)
		{
			if (!ec)
			{
				handle_body();
			}
			else
			{
				dlog(warning) << this << " Read failed: " << ec.message();
			}
		});
}

void connection::error(const string& text)
{
	dlog(warning) << this << " Client error: " << text;
	flatbuffers::FlatBufferBuilder fbb;
	auto s = fbb.CreateString(text);
	auto e = CreateError(fbb, s);
	auto p = CreatePackage(fbb, PackageType_Error, e.Union());
	FinishPackageBuffer(fbb, p);
	send_package(fbb);
}

void connection::send_package(const flatbuffers::FlatBufferBuilder& fbb)
{
	++pkg_queue;
	int32_t pkg_size = htonl(fbb.GetSize());
	current_write_buf.resize(sizeof(pkg_size) + fbb.GetSize());
	memcpy(current_write_buf.data(), &pkg_size, sizeof(pkg_size));
	memcpy(current_write_buf.data() + sizeof(pkg_size), fbb.GetBufferPointer(), fbb.GetSize());
	auto self = shared_from_this();
	boost::asio::async_write(sock, boost::asio::buffer(current_write_buf), [this, self](boost::system::error_code ec, size_t)
		{
			if (ec)
			{
				dlog(info) << "Connection " << this << ": write error: " << ec.message() << ". Dropping connection.";
			}
			else
			{
				if (!--pkg_queue)
				{
					timer.start_once();
				}
			}
		});
}

void connection::handle_body()
{
	auto verifier = flatbuffers::Verifier(reinterpret_cast<const uint8_t*>(current_body_read_buf.data()), current_body_read_buf.size());
	if (!VerifyPackageBuffer(verifier))
	{
		error("Bad package arrived");
		return;
	}
	auto pkg = GetPackage(current_body_read_buf.data());
	switch (pkg->pkg_type())
	{
		case PackageType_Login:
			handle_login(static_cast<const Login*>(pkg->pkg()));
		break;
		
		case PackageType_Direction:
			handle_direction(static_cast<const Direction*>(pkg->pkg()));
		break;
		
		case PackageType_Exit:
			dlog(info) << this << "Client sent exit package. Don't read.";
			return;
		break;

		default:
			error("Bad package type");
			dlog(info) << pkg->pkg_type();
		break;
	}
	do_read_header();
}

void connection::handle_login(const Login* pkg)
{
	string login = pkg->login()->str(), password = pkg->password()->str();
	int level = pkg->level();
	if (player)
	{
		error("You have already logged in");
		return;
	}

	if (!srv->get_users()->authen(login, password, level))
	{
		error("Wrong login, password or level");
		return;
	}

	game = srv->get_game(pkg->field());
	if (!game)
	{
		error("Wrong field");
		return;
	}

	player = game->get_player(login, level);
	if (!player || player->connections > MAX_CONNECTIONS)
	{
		error("Cannot register the player for the game");
		game = nullptr;
		player = nullptr;
		return;
	}

	++player->connections; 

	dlog(info) << this 
		<< " logged in login=" << login 
		<< " level=" << level 
		<< " field=" << pkg->field()
		<< " player=" << player.get();

	do_send_welcome();
}

void connection::handle_direction(const Direction* pkg)
{
	if (!player)
	{
		error("You have not logged in");
		return;
	}

	game_logic::direction d;
	d.p.x = pkg->direction()->x();
	d.p.y = pkg->direction()->y();
	d.boost = pkg->boost();
	d.split = pkg->split();
	game->set_direction(player.get(), pkg->snake_id(), d);
}

std::shared_ptr<userdb::user_db> server::get_users() const
{
	return users;
}

std::shared_ptr<game_logic::game> server::get_game(int field) const
{
	auto i = games.find(field);
	return i != games.end() ? i->second : nullptr;
}

void connection::do_send_welcome()
{
	flatbuffers::FlatBufferBuilder fbb;
	auto w = CreateWelcome(fbb, player->get_id(), game->get_configuration().k_10);
	auto p = CreatePackage(fbb, PackageType_Welcome, w.Union());
	FinishPackageBuffer(fbb, p);
	send_package(fbb);
}

connection::~connection()
{
	--player->connections;
	dlog(info) << "Connection " << this << " destroyed";
}

void server::add_game(int field, const shared_ptr<game_logic::game>& g)
{
	if (games.find(field) != games.end())
	{
		throw std::runtime_error("Field already exists!");
	}
	games.emplace(field, g);
}

void server::set_users(const std::shared_ptr<userdb::user_db>& _users)
{
	users = _users;
}

void connection::send_field(const std::shared_ptr<game_logic::field>& field)
{
	if (!player) return;
	/* Find the snakes of this player */
	for (auto &i : field->snakes)
	{
		flatbuffers::FlatBufferBuilder fbb;
		std::vector<flatbuffers::Offset<Snake>> snakes;
		if (i.p != player.get())
		{
			continue;
		}
		/* Find nearby snakes */
		for (auto &j : field->snakes)
		{
			std::vector<Point> skeleton;
			for (auto &k : j.skeleton)
			{
				if ((k - i.skeleton[0]).dist2() < game_logic::sqr(100 * i.r))
				{
					skeleton.emplace_back(k.x, k.y);
				}
			}
			if (!skeleton.empty())
			{
				snakes.emplace_back(
					CreateSnake(fbb, j.p->get_id(), j.id, j.r, fbb.CreateVectorOfStructs(skeleton), true, false)
				);
			}
		}

		/* Find nearby foods */
		std::vector<Food> foods;
		for (auto &j : field->foods)
		{
			if ((j.p - i.skeleton[0]).dist2() < game_logic::sqr(100*i.r))
			{
				foods.emplace_back(Food(Point(j.p.x, j.p.y), j.w));
			}
		}
		auto f = CreateField(fbb, i.id, i.w, field->time, fbb.CreateVector(snakes), fbb.CreateVectorOfStructs(foods));
		auto p = CreatePackage(fbb, PackageType_Field, f.Union());
		FinishPackageBuffer(fbb, p);
		send_package(fbb);
	}
	if (!pkg_queue)
	{
		timer.start_once();
	}
}
