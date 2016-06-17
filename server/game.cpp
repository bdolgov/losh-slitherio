#include "game.hpp"
#include "common.hpp"
#include <stdexcept>
#include <iostream>

using namespace game_logic;
using namespace std;

std::shared_ptr<field> game::get_current_field() const
{
	lock_guard<mutex> l(field_mutex);
	return current_field;
}

void game::set_current_field(const std::shared_ptr<field>& field)
{
	lock_guard<mutex> lg(field_mutex);
	current_field = field;
}

game::directions_t game::get_directions()
{
	lock_guard<mutex> lg(directions_mutex);
	directions_t ret;
	swap(ret, directions_queue);
	return ret;
}

float game::snake_r(const snake& s) const
{
	return cfg.snake_r_k1 * log(cfg.snake_r_k2 * s.w + cfg.snake_r_k3);
}

int game::snake_len(const snake& s) const
{
	return cfg.snake_l_k4 * s.w / sqr(s.r) + cfg.snake_l_k5;
}

vector<snake_request> game::get_create_snakes()
{
	lock_guard<mutex> lg(directions_mutex);
	vector<snake_request> ret;
	swap(ret, create_snakes_queue);
	return ret;
}

void game::create_snake(const snake_request& r)
{
	lock_guard<mutex> lg(directions_mutex);
	create_snakes_queue.push_back(r);
}

void game::tick()
{
	auto old_field = get_current_field();
	auto field = make_shared<game_logic::field>(old_field->arena.get_total());
	field->time = old_field->time + 0.1;
	dlog(debug) << "tick snakes=" << old_field->snakes.size();
	auto directions = get_directions();
	vector<snake_request> create_snakes = get_create_snakes();

	for (auto& i : directions)
	{
		get<0>(i)->directions[get<1>(i)] = get<2>(i);
	}

	field->snakes.alloc(field->arena, old_field->snakes.size() + create_snakes.size());

	/* Process snake positions and properties */
	size_t idx = 0;
	for (auto &prev : old_field->snakes)
	{
		if (prev.w == 0)
		{
			if (--prev.p->snakes == 0)
			{
				create_snake(snake_request(prev.p));
			}
			continue;
		}

		snake &cur = field->snakes[idx++];

		cur.p = prev.p;
		cur.id = prev.id;
		
		direction &d = prev.p->directions[prev.id];
		if (d.split)
		{
			d.split = false;
			cur.w = prev.w - cfg.k_10;
			snake_request s(prev.p);
			s.w = cfg.k_10;
			for (ssize_t i = prev.skeleton.size() - 1; i >= 0; --i)
			{
				s.skeleton.push_back(prev.skeleton[i]);
			}
			create_snake(s);
		}
		else
		{
			cur.w = prev.w;
		}
		cur.r = snake_r(cur);
		/* Calculate head direction */
		point prev_direction_vec = prev.skeleton[0] - prev.skeleton[1];
		point cur_direction_vec = d.p - prev.skeleton[0];
		if (cur_direction_vec.dist2() < 1e-2)
		{
			/* Direction is unknown; keep original direction */
			cur_direction_vec = prev_direction_vec;
		}
		float direction_angle = point::angle(prev_direction_vec, cur_direction_vec);
		if (fabs(direction_angle) > cfg.max_direction_angle)
		{
			if (direction_angle > 0)
			{
				cur_direction_vec = prev_direction_vec.rot(cfg.max_direction_angle);
			}
			else
			{
				cur_direction_vec = prev_direction_vec.rot(-cfg.max_direction_angle);
			}
		}

		/* Update speed */
		if (d.boost)
		{
			cur.speed = min(prev.speed + cfg.boost_acceleration_per_tick,
				cfg.max_speed_multiplier * log(cur.w) + cfg.base_boost_speed);
		}
		else
		{
			cur.speed = max(prev.speed - cfg.boost_acceleration_per_tick,
				cfg.min_speed_multiplier * log(cur.w) + cfg.base_speed);
		}

		/* Find head position */
		point cur_head_position = cur_direction_vec.norm() * prev.speed + prev.skeleton[0];

		/* Move the snake */
		{
			int len = snake_len(cur);
			cur.skeleton.alloc(field->arena, len);
			cur.skeleton[0] = cur_head_position;
			int i;
			for (i = 1; i < prev.skeleton.size() && i < len; ++i)
			{
				point direction = prev.skeleton[i] - cur.skeleton[i - 1]; /* From head towards tail */
				if (direction.dist2() <= cur.r * cur.r)
				{
					cur.skeleton[i] = prev.skeleton[i];
				}
				else
				{
					cur.skeleton[i] = cur.skeleton[i - 1] + direction.norm() * cur.r;
				}
			}
			for (; i < len; ++i)
			{
				cur.skeleton[i] = cur.skeleton[i - 1];
			}
		}
	}

	/* Create snakes */
	for (auto &i : create_snakes)
	{
		snake &cur = field->snakes[idx++];
		cur.p = i.p;
		cur.id = cur.p->get_next_snake_id();
		cur.w = i.w ? i.w : cfg.default_w;
		cur.r = snake_r(cur);
		cur.speed = cfg.min_speed_multiplier * log(cur.w) + cfg.base_speed;
		int len = snake_len(cur);
		cur.skeleton.alloc(field->arena, len);
		int k;
		if (i.skeleton.empty())
		{
			i.skeleton.emplace_back(0,0);
			i.skeleton.emplace_back(0,1);
		}
		for (k = 0; k < i.skeleton.size() && k < len; ++k)
		{
			cur.skeleton[k] = i.skeleton[k];
		}
		for (; k < len; ++k)
		{
			cur.skeleton[k] = cur.skeleton[k - 1];
		}
		dlog(info) << "Creating snake " << cur.p->get_id() << "," << cur.id;
		++cur.p->snakes;
	}

	field->snakes.realloc(field->arena, idx);

	vector<food> new_foods;

	auto death = [&](snake& s)
		{
			if (s.w == 0)
			{
				return;
			}
			dlog(debug) << "Killing snake " << s.p->get_id() << "," << s.id;
			float w = s.w / s.skeleton.size();
			for (auto &i : s.skeleton)
			{
				new_foods.emplace_back(i, w);
			}
			s.w = 0;
		};

	/* Check snake collisions */
	for (auto &i : field->snakes)
	{
		point head = i.skeleton[0];
		for (auto &j : field->snakes)
		{
			if (&i == &j) continue;
			if ((head - j.skeleton[0]).dist2() <= sqr(i.r + j.r)
				&& i.speed < j.speed)
			{
				/* Head collision */
				death(i);
				break;

			}
			for (auto &k : j.skeleton)
			{
				if ((head - k).dist2() <= sqr(i.r + j.r))
				{
					/* Common collision */
					death(i);
					break;
				}
			}
			if (i.w == 0)
			{
				break;
			}
		}
	}

	/* FIXME: spread food with boost */

	/* FIXME: borders collision */

	/* Food generation */
	for (int i = old_field->foods.size(); i < 150; ++i)
	{
		new_foods.emplace_back(point(cfg.food_coord_distribution(rng), cfg.food_coord_distribution(rng)), 5);
	}

	/* Copy food to the new field and feed the snakes */
	field->foods.alloc(field->arena, old_field->foods.size() + new_foods.size());
	int foods_n = 0;
	for (auto &i : old_field->foods)
	{
		bool eaten = false;
		for (auto &j : field->snakes)
		{
			if (j.w == 0)
			{
				/* The snake is dead; it shouldn't eat itself */
				continue;
			}

			if ((j.skeleton[0] - i.p).dist2() <= sqr(j.r))
			{
				j.w += i.w;
				eaten = true;
				break;
			}
		}
		if (!eaten)
		{
			field->foods[foods_n++] = i;
		}
	}
	for (auto &i : new_foods)
	{
		field->foods[foods_n++] = i;
	}
	field->foods.realloc(field->arena, foods_n);

	set_current_field(field);
}

food::food(point _p, float _w):
	p(_p), w(_w)
{
}

field::field(size_t arena_size):
	arena(arena_size)
{
}

std::shared_ptr<player> game::get_player(const string& login, int level)
{
	auto it = players.find(login);
	if (it == players.end())
	{
		it = players.emplace(login, make_shared<player>(player_id_seq++)).first;
		if (game_started)
		{
			/* Add first snake */
			create_snake(snake_request(it->second.get()));
		}
	}
	return it->second;
}

const configuration& game::get_configuration() const
{
	return cfg;
}

void game::set_direction(player *p, int snake_id, const direction& d)
{
	lock_guard<mutex> l(directions_mutex);
	directions_queue.emplace_back(p, snake_id, d);
}

player::player(int _id):
	id(_id),
	snake_id_seq(0)
{
}

int player::get_id() const
{
	return id;
}

int player::get_next_snake_id()
{
	return snake_id_seq++;
}

game::game(const configuration& _cfg):
	current_field(make_shared<field>(16384)),
	cfg(_cfg),
	player_id_seq(0),
	game_started(true)
{
	current_field->time = 0;
}
