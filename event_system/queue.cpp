#include "queue.hpp"
#include "sync.hpp"

#include <memory>
#include <vector>
#include <deque>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>

namespace queue
{
	struct Event
	{
		std::thread::id m_there;
		std::string m_tag;
		size_t m_id;
		std::function < void() > m_call;
		void operator()() { m_call(); }

		Event(std::thread::id there, const std::string& tag, size_t id, const std::function< void() >& call)
			: m_there(there)
			, m_tag(tag)
			, m_id(id)
			, m_call(call)
		{}
		explicit Event(Event && ev) : m_there(ev.m_there), m_tag(ev.m_tag), m_id(ev.m_id), m_call(std::move(ev.m_call)) {}
	};

	struct Queue
	{
		std::deque<Event> m_queue;
		std::thread::id m_here;
		std::mutex m_mutex;
		std::condition_variable m_cv;
		bool m_done;
		std::string m_name;
		unsigned int m_indent;
		size_t m_id_pool;

		std::function < void() > m_onStart, m_onStop;

		Queue(const std::string& name, unsigned int indent)
			: m_here(std::this_thread::get_id())
			, m_done(false)
			, m_name(name)
			, m_indent(indent)
			, m_id_pool(0)
		{
		}

		void stop()
		{
			sync() << "--- Posting a stop to " << m_here << ". ---";
			post("STOP", [&] { sync() << "--- Stop received. ---"; m_done = true; });
		}

		void run()
		{
			sync() << "=== Starting an event pump ===";

			if (m_onStart)
				m_onStart();

			while (!m_done)
				call_next();

			if (m_onStop)
				m_onStop();

			sync() << "=== Stopping the event pump ===";
		}

		void post(const std::string& tag, const std::function< void() >& ev)
		{
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				auto id = m_id_pool++;

				sync cout;
				cout << "[" << m_here << "/" << id << "] ";
				if (!tag.empty())
					cout << "{" << tag << "} ";

				auto thread = std::this_thread::get_id();
				if (thread == m_here)
				{
					cout << "Local call";
					cout.flush();

					ev();
					return;
				}

				cout << "(-> " << m_here << ")";

				if (m_done)
					cout << " STOP ALREADY RECEIVED, THIS WILL NOT BE CALLED!";

				m_queue.emplace_back(thread, tag, id, ev);
			}
			m_cv.notify_one();
		}

		void call_next()
		{
			{
				std::unique_lock<std::mutex> lock(m_mutex);
				m_cv.wait(lock, [&]() { return !m_queue.empty(); });
			}

			m_mutex.lock();

			auto count = m_queue.size();
			auto ev = std::move(m_queue.front());
			m_queue.pop_front();

			m_mutex.unlock();

			{
				sync cout;
				cout << "[" << m_here << "/" << ev.m_id << "] ";
				if (!ev.m_tag.empty())
					cout << "{" << ev.m_tag << "} ";
				cout << "(" << ev.m_there << " ->)";
			}
			ev();
		}

		template <typename T>
		void on_start(T ev) { m_onStart = ev; }

		template <typename T>
		void on_stop(T ev) { m_onStop = ev; }
	};

	namespace
	{
		struct ThreadData
		{
			std::vector< std::shared_ptr<Queue> > m_queues;
			std::mutex m_mutex;

			inline Queue* createQueue(const std::string& name, unsigned int indent)
			{
				std::lock_guard<std::mutex> lock { m_mutex };

				auto id = std::this_thread::get_id();
				for (auto && q : m_queues)
					if (q->m_here == id)
						return q.get();
				m_queues.push_back(std::make_shared<Queue>(name, indent));
				return m_queues.back().get();
			}

			inline Queue* getQueue(std::thread::id id)
			{
				std::lock_guard<std::mutex> lock { m_mutex };

				for (auto && q : m_queues)
					if (q->m_here == id)
						return q.get();
				return nullptr;
			}

			static inline ThreadData& get()
			{
				static ThreadData data;
				return data;
			}
		};
	}

	namespace impl
	{
		Queue* create_queue(const std::string& name, unsigned int indent)
		{
			return ThreadData::get().createQueue(name, indent);
		}

		Queue* get_queue(std::thread::id id)
		{
			return ThreadData::get().getQueue(id);
		}

		void on_queue_start(Queue* q, const std::function<void()> on_start)
		{
			q->on_start(on_start);
		}

		void on_queue_stop(Queue* q, const std::function<void()> on_stop)
		{
			q->on_stop(on_stop);
		}

		void run_queue(Queue* q)
		{
			q->run();
		}

		void stop_queue(Queue* q)
		{
			q->stop();
		}

		void post_queue(Queue* q, const std::string& tag, const std::function<void()> ev)
		{
			q->post(tag, ev);
		}

		const std::string& queue_name(Queue* q)
		{
			return q->m_name;
		}

		unsigned int queue_indent(Queue* q)
		{
			return q->m_indent;
		}
	}

	void wait(std::thread::id id)
	{
		//it should be cond. variable... :(

		bool waited = false;
		while (!impl::get_queue(id))
		{
			sync() << "waiting...";
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			waited = true;
		}
		sync() << "queue found.";
	}
}