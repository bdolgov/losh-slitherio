#include "losh-slitherio.hpp"
#include <cmath>
bool have_decision = false;
Point decision;
int tics;
double prev_w = 0;
using namespace std;
Point operator-(Point a, Point b)
{
	return Point(a.x - b.x, a.y - b.y);
}

double dist2(Point p)
{
	return p.x * p.x + p.y * p.y;
}

Point play(const Field& f, bool&, bool&)
{
	/* Find my head */
	Point head, afterHead;
	double r = 0;
	for (auto &i : f.snakes)
	{
		if (i.player == configuration.player && i.id == f.id)
		{
			r = i.r;
			head = i.skeleton[0];
			afterHead = i.skeleton[1];
		}
	}

	if (fabs(prev_w - f.w) > 1e-3)
	{
		cerr << "weight changed, drop decision" << endl;
		have_decision = false;
		prev_w = f.w;
	}
	/* Find best food */
	if (!have_decision)
	{
		double dist = 1e9;
		Point ret;
		for (auto i : f.foods)
		{
			Point dv(i.p - head);
			double d = dist2(dv);
			if (d < 9 * r * r) continue;
			if (d < dist)
			{
				decision = i.p;
				dist = d;
			}
		}
		cerr << "new decision " << decision.x << " " << decision.y << endl;
		have_decision = true;
		tics = 0;
	}
	
	if (dist2(head - decision) < 9 * r * r)
	{
		++tics;
		cerr << "near decision" << endl;
	}

	if (tics == 10)
	{
		have_decision = false;
		cerr << "drop ticks" << endl;
	}
	
	return decision;
}

SLITHERIO_RUN("127.0.0.1:2000", "boris", "123123123", 0);
