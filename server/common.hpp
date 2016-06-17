#ifndef COMMON_HPP
#define COMMON_HPP

#include <boost/log/trivial.hpp>
#include <boost/asio.hpp>

#define dlog(X) BOOST_LOG_TRIVIAL(X)

class periodic_timer
{
	public:
		periodic_timer(boost::asio::io_service &ios, boost::asio::deadline_timer::duration_type _duration):
			timer(ios),
			duration(_duration)
		{
		}

		void set_cb(const std::function<void(void)>& _cb)
		{
			cb = _cb;
		}

		void start_once()
		{
			timer.expires_from_now(duration);
			timer.async_wait([this](const boost::system::error_code& ec)
				{
					if (ec != boost::asio::error::operation_aborted)
					{
						cb();
					}
				});
		}

		void start_many()
		{
			timer.expires_from_now(duration);
			timer.async_wait([this](const boost::system::error_code& ec)
				{
					if (ec != boost::asio::error::operation_aborted)
					{
						cb();
						start_many();
					}
				});
		}

	private:
		boost::asio::deadline_timer timer;
		boost::asio::deadline_timer::duration_type duration;
		std::function<void(void)> cb;
};

#endif
