#ifndef GAME_HPP
#define GAME_HPP

#include "alloc.hpp"
#include <memory>
#include <vector>
#include <mutex>
#include <tuple>
#include <cmath>
#include <map>
#include <string>
#include <set>
#include <random>
#include <sstream>

namespace network { class connection; }

namespace game_logic
{
	struct point
	{
		float x, y;

		explicit point(float _x = 0, float _y = 0): x(_x), y(_y) {}

		point operator+(const point& other) const { return point(x + other.x, y + other.y); }
		point operator-(const point& other) const { return point(x - other.x, y - other.y); }
		point operator*(float k) const { return point(x * k, y * k); }
		point operator/(float k) const { return point(x / k, y / k); }
		float dist2() const { return x * x + y * y; }
		float dist() const { return sqrt(dist2()); }
		point norm() const { return *this / dist(); }
		point rot(float angle) const
		{
			return point(x * cos(angle) - y * sin(angle), x * sin(angle) + y * cos(angle));
		}
		
		static float sprod(const point& a, const point& b) { return a.x * b.x + a.y * b.y; }
		static float vprod(const point& a, const point& b) { return a.x * b.y - a.y * b.x; }
		static float angle(const point& a, const point& b) { return atan2(vprod(a, b), sprod(a, b)); }
	};

	inline static std::ostream& operator<<(std::ostream& s, point p)
	{
		s << "(" << p.x << "," << p.y << ")";
		return s;
	}
	
	inline static float sqr(float x) { return x * x; }

	class player;

	struct snake
	{
		player* p;
		int id;
		float w;
		float r;
		float speed;
		bool boost;
		mem::dynarr<point> skeleton;
	};

	struct food
	{
		food(point _p = point(), float _w = 0);
		point p;
		float w;
	};

	struct field
	{
		field(size_t arena_size);
		mem::arena arena;
		float time;
		int tick;
		mem::dynarr<snake> snakes;
		mem::dynarr<food> foods;
	};

	struct direction
	{
		point p = point(10,10);
		bool boost;
		bool split;
	};

	struct player
	{
		private:
			int id;
			int snake_id_seq;
		public:
			player(int _id, int _level = 1);
			int get_id() const;
			int get_next_snake_id();
			int connections = 0;
			std::map<int, direction> directions;
			int snakes = 0;
			float w_sum = 0;
			float w_max = 0;
			int level = 0;
	};

	struct configuration
	{
		float boost_acceleration_per_tick;
		float boost_spend_per_8_ticks;
		float max_direction_angle;
		float snake_r_k1, snake_r_k2, snake_r_k3, snake_l_k4, snake_l_k5;
		float default_w;
		float k_10;
		float max_speed_multiplier, min_speed_multiplier, base_speed, base_boost_speed;
		std::normal_distribution<float> food_coord_distribution, food_w_distribution;
		int tick_ms;
	};

	struct snake_request
	{
		snake_request(player* _p): p(_p), w(0) {}
		player* p;
		float w;
		std::vector<point> skeleton;
	};

	class game
	{
		public:
			game(const configuration& _cfg);
			std::shared_ptr<field> get_current_field() const;
			void set_direction(player *p, int snake_id, const direction& d);
			int tick();
			void create_snake(const snake_request& r);
			std::shared_ptr<player> get_player(const std::string& login, int level = 1);
			const configuration& get_configuration() const;
			bool game_started;

		private:
			std::map<std::string, std::shared_ptr<player>> players;

			std::shared_ptr<field> current_field;
			mutable std::mutex field_mutex;
			void set_current_field(const std::shared_ptr<field>& field);

			typedef std::vector<std::tuple<player*, int, direction>> directions_t;
			directions_t directions_queue;
			mutable std::mutex directions_mutex;
			directions_t get_directions();
			std::vector<snake_request> create_snakes_queue;
			std::vector<snake_request> get_create_snakes();

			configuration cfg;

			int player_id_seq;

			float snake_r(const snake& s) const;
			int snake_len(const snake& s) const;

			std::mt19937_64 rng;
	};
}

#endif
