#ifndef COMMON_HPP
#define COMMON_HPP

#include <boost/log/trivial.hpp>
#include <boost/asio.hpp>
#include <memory>

#define dlog(X) BOOST_LOG_TRIVIAL(X)

class periodic_timer
{
	private:
		struct data_t : public std::enable_shared_from_this<data_t>
		{
			boost::asio::deadline_timer timer;
			boost::asio::deadline_timer::duration_type duration;
			typedef std::function<void(void)> cb_t;
			cb_t cb;
			std::atomic<bool> stop;

			data_t(boost::asio::io_service &ios, boost::asio::deadline_timer::duration_type _duration):
				timer(ios), duration(_duration), stop(false)
			{
			}
			void start_once()
			{
				auto self = shared_from_this();
				timer.expires_from_now(duration);
				timer.async_wait([self](const boost::system::error_code& ec)
					{
						if (ec != boost::asio::error::operation_aborted && !self->stop)
						{
							self->cb();
						}
					});
			}

			void start_many()
			{
				auto self = shared_from_this();
				timer.expires_from_now(duration);
				timer.async_wait([self](const boost::system::error_code& ec)
					{
						if (ec != boost::asio::error::operation_aborted && !self->stop)
						{
							self->cb();
							self->start_many();
						}
					});
			}
		};
		std::shared_ptr<data_t> data;
	public:
		periodic_timer(boost::asio::io_service &ios, boost::asio::deadline_timer::duration_type _duration):
			data(std::make_shared<data_t>(ios, _duration))
		{
		}

		~periodic_timer()
		{
			data->stop = true;
			data->timer.cancel();
		}

		void set_cb(const data_t::cb_t& _cb)
		{
			data->cb = _cb;
		}

		void start_once()
		{
			data->start_once();
		}

		void start_many()
		{
			data->start_many();
		}
};

#endif
