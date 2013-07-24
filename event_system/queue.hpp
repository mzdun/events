#ifndef __QUEUE_HPP__
#define __QUEUE_HPP__

#include <functional>
#include <string>
#include <thread>

namespace queue
{
	struct Queue;

	namespace impl
	{
		Queue* create_queue(const std::string& name, unsigned int indent);
		Queue* get_queue(std::thread::id id);
		void on_queue_start(Queue* q, const std::function<void()> on_start);
		void on_queue_stop(Queue* q, const std::function<void()> on_stop);
		void run_queue(Queue* q);
		void stop_queue(Queue* q);
		void post_queue(Queue* q, const std::string& tag, const std::function<void()> ev);
		const std::string& queue_name(Queue* q);
		unsigned int queue_indent(Queue* q);
	};

	inline bool start(const std::string& name, unsigned int indent)
	{
		auto q = impl::create_queue(name, indent);
		if (!q)
			return false;
		return true;
	}

	void wait(std::thread::id id);

	inline void wait(const std::thread& th)
	{
		wait(th.get_id());
	}

	template <typename T1, typename T2>
	inline void run(T1 on_start, T2 on_stop)
	{
		auto q = impl::get_queue(std::this_thread::get_id());
		if (!q)
			return;

		impl::on_queue_start(q, on_start);
		impl::on_queue_stop(q, on_stop);
		impl::run_queue(q);
	}

	inline void run()
	{
		auto q = impl::get_queue(std::this_thread::get_id());
		if (!q)
			return;

		impl::run_queue(q);
	}

	inline void stop(std::thread::id id)
	{
		auto q = impl::get_queue(id);
		if (q)
			impl::stop_queue(q);
	}

	template <typename T>
	inline void post(std::thread::id id, const std::string& tag, T ev)
	{
		auto q = impl::get_queue(id);
		if (q)
			impl::post_queue(q, tag, ev);
	}

	template <typename T, typename... Args>
	inline void post(std::thread::id id, const std::string& tag, T ev, Args && ... args)
	{
		auto q = impl::get_queue(id);
		if (q)
			impl::post_queue(q, tag, std::bind(ev, std::forward<Args>(args)...));
	}

	struct QueueHandle
	{
		std::thread::id m_id;
		QueueHandle() : m_id(std::this_thread::get_id()) {}
		QueueHandle(std::thread::id id) : m_id(id) {}
		QueueHandle(const std::thread& th) : m_id(th.get_id()) {}

		void stop() const { queue::stop(m_id); }

		template <typename T>
		void post(const std::string& tag, T ev) const { queue::post(m_id, tag, ev); }

		template <typename T, typename... Args>
		void post(const std::string& tag, T ev, Args && ... args) const
		{
			queue::post(m_id, tag, ev, std::forward<Args>(args)...);
		}
	};
}

#endif // __QUEUE_HPP__