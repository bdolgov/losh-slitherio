#ifndef SNAKE_LIBRARY_H
#define SNAKE_LIBRARY_H

#include <vector>
#include <string>
#include <utility>
#include <sstream>
#include <iostream>
#include <future>

#include "snake_generated.h"

struct Configuration
{
	int player;
	double k10;
} configuration; 

struct Point {
    Point(double _x = 0, double _y = 0): x(_x), y(_y) {}
	double x, y;
};

struct Food {
	Point p;
	double w;
};

struct Snake {
	int player;
	int id;
	double r;
	std::vector<Point> skeleton;
	bool headVisible; 
    bool boost;
};

struct Field {
	int id;
	double w;
	double time;
	std::vector<Snake> snakes;
	std::vector<Food> foods;
	std::vector<std::pair<Point, Point>> borders;
};

Point play(const Field& field, bool &boost, bool &split);

namespace snake_impl
{   
	using namespace std;
	using namespace asio;
	using ip::tcp;

	struct dlog
    {
        dlog() { cerr << "SNAKELIB: "; }
        ~dlog() { cerr << endl; }
        template<class T>
        const dlog& operator<<(const T& what) const { cerr << what; return *this; }
    };

	class Client
	{
        private:
            io_service ios;
            string hostname, port, login, password;
            tcp::socket sock;
            int field;
            atomic<bool> isBusy;
		public:
			Client(const string& _s, const string& _l, const string& _p, int _f):
				login(_l), password(_p), field(_f),
                sock(ios)
			{
				stringstream srvss(_s);
				getline(srvss, hostname, ':');
				srvss >> port;
				dlog() << "hostname=" << hostname << " port=" << port;
			}

			int run()
			{
                try
                {
                    tcp::resolver resolver(ios);
                    connect(sock, resolver.resolve({hostname, port}));
                    dlog() << "connected";
                    {
                        flatbuffers::FlatBufferBuilder fbb;
                        auto login = fbb.CreateString(this->login);
                        auto password = fbb.CreateString(this->password);
                        auto w = SnakeGame::CreateLogin(fbb, login, password, field);
                        auto pkg = SnakeGame::CreatePackage(fbb, SnakeGame::PackageType_Login, w.Union());
                        SnakeGame::FinishPackageBuffer(fbb, pkg);
                        send(fbb);
                    }
                    for (;;)
                    {
                        uint32_t msglen;
                        unsigned char msglen_buf[sizeof(msglen)];
                        read(sock, buffer(msglen_buf, sizeof(msglen_buf)));
                        msglen =  ((static_cast<size_t>(msglen_buf[0]) * 256
                            + msglen_buf[1]) * 256
                            + msglen_buf[2]) * 256
                            + msglen_buf[3];
                        vector<char> message(msglen);
                        read(sock, buffer(message));
                        auto pkg = SnakeGame::GetPackage(message.data());
                        switch (pkg->pkg_type())
                        {
                            case SnakeGame::PackageType_Welcome:
                            {
                                auto welcome = static_cast<const SnakeGame::Welcome*>(pkg->pkg());
                                configuration.k10 = welcome->k10();
                                configuration.player = welcome->player_id();
                            }
                            break;
                            case SnakeGame::PackageType_Field:
                            {
                                if (isBusy)
                                {
                                    dlog() << "Frame dropped :-(";
                                }
                                else
                                {
                                    auto field = static_cast<const SnakeGame::Field*>(pkg->pkg());
                                    auto myField = f2f(field);
                                    isBusy = true;
                                    async(launch::async, [this, myField]()
                                        {
                                            bool boost = false;
                                            for (auto &i : myField.snakes)
                                            {
                                                if (i.player == configuration.player && i.id == myField.id)
                                                {
                                                    boost = i.boost;
                                                }
                                            }
                                            bool split = false;
                                            Point ret = play(myField, boost, split);
                                            isBusy = false;
                                            flatbuffers::FlatBufferBuilder fbb;
                                            auto point = SnakeGame::Point(ret.x, ret.y);
                                            auto d = SnakeGame::CreateDirection(fbb, myField.id, &point, boost, split);
                                            auto pkg = SnakeGame::CreatePackage(fbb, SnakeGame::PackageType_Direction, d.Union());
                                            SnakeGame::FinishPackageBuffer(fbb, pkg);
                                            send(fbb);
                                        });
                                }
                            }
                            break;
                            case SnakeGame::PackageType_Error:
                            {
                                auto error = static_cast<const SnakeGame::Error*>(pkg->pkg());
                                dlog() << "Remote error: " << error->description()->str();
                            }
                            break;
                            default:
                                throw std::logic_error("Unknown package arrived");
                        }
                    }
                }
                catch (exception& e)
                {
                    dlog() << "Local error: " << e.what();
                    return 1;
                }
                return 0;
			}




            void send(const flatbuffers::FlatBufferBuilder& fbb)
            {
                uint32_t sz = htonl(fbb.GetSize());
                write(sock, buffer(&sz, sizeof(sz)));
                write(sock, buffer(fbb.GetBufferPointer(), fbb.GetSize()));
            }

            Field f2f(const SnakeGame::Field *f)
            {
                Field ret;
                ret.id = f->snake_id();
                ret.w = f->w();
                ret.time = f->time();
                for (auto i : *f->snakes())
                {
                    Snake cur;
                    cur.player = i->player_id();
                    cur.id = i->snake_id();
                    cur.r = i->r();
                    for (auto j : *i->skeleton())
                    {
                        cur.skeleton.emplace_back(j->x(), j->y());
                    }
                    cur.headVisible = i->head_visible();
                    cur.boost = i->boost();
                    ret.snakes.emplace_back(move(cur));
                }
                for (auto i : *f->foods())
                {
                    Food cur;
                    cur.p.x = i->p().x();
                    cur.p.y = i->p().y();
                    cur.w = i->w();
                    ret.foods.emplace_back(cur);
                }

                if (f->borders()) for (auto i : *f->borders())
                {
                    ret.borders.emplace_back(Point(i->first().x(), i->first().y()),
                        Point(i->second().x(), i->second().y()));
                }
                return ret;
            }
	};
}

#define SLITHERIO_RUN(server, login, password, field) \
	int main() { return snake_impl::Client(server, login, password, field).run(); }

#endif
