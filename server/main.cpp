#include "network.hpp"
#include "game.hpp"
#include "userdb.hpp"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <functional>
#include <cmath>
#include "common.hpp"
#include <fstream>

int main(int ac, char** av)
{
	boost::asio::io_service ios;
	auto server = std::make_shared<network::server>(ios, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 2000));
	game_logic::configuration cfg;

	cfg.boost_acceleration_per_tick = 0.1;
	cfg.boost_spend_per_8_ticks = 0.01;
	cfg.max_direction_angle = 3.14 / 8;
	cfg.default_w = 20;
	cfg.snake_r_k1 = 1.0 / log(20);
	cfg.snake_r_k2 = 1;
	cfg.snake_r_k3 = 10;
	cfg.snake_l_k4 = 0.5;
	cfg.snake_l_k5 = 0;
	cfg.k_10 = 1000;
	cfg.max_speed_multiplier = 0.3;
	cfg.min_speed_multiplier = 0.2;
	cfg.base_speed = 0.6;
	cfg.base_boost_speed = 1.3;
	cfg.food_coord_distribution = std::normal_distribution<float>(0, 100);
	cfg.tick_ms = 75;
	auto users = std::make_shared<userdb::user_db>("users.txt");
	std::ofstream gameLog("gameLog.json");
		auto f0 = std::make_shared<game_logic::game>(cfg);
		f0->game_started = true;
		server->add_game(0, f0);
		server->set_users(users);
	
		periodic_timer *tick_timer = new periodic_timer(ios, boost::posix_time::milliseconds(75));
		tick_timer->set_cb([f0, &gameLog]()
			{
				int t = f0->tick();
				if ((t & 15) == 0)
				{
					dlog(debug) << "1";
					gameLog << "{" << "\n";
					auto f = f0->get_current_field();
					std::map<int, float> ws;
					for (auto &i : f->snakes)
					{
						ws[i.p->get_id()] += i.w;
					}
					bool first = true;
					for (auto &i : ws)
					{
						gameLog << "\t" << (first ? "" : ",") << "\"" << i.first << "\": " << i.second  << "\n";
						first = false;
					}
					gameLog << "}," << std::endl;
					gameLog.flush();
				}
			});
		tick_timer->start_many();
	ios.run();
	return 0;
}
