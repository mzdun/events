// event_system.cpp : Defines the entry point for the console application.
//

#include "queue.hpp"
#include "sync.hpp"

#include <thread>
#include <chrono>

void f(int answer)
{
	sync() << "------- f(" << answer << ")";
}

void response(int result)
{
	sync() << "------- response(" << result << ")";
}

void query(const queue::QueueHandle& sender, int lhs, int rhs)
{
	sync() << "------- query(" << lhs << ", " << rhs << ")";
	sender.post("QUERY-2", response, lhs + rhs);
}

void messages(const queue::QueueHandle& thread)
{
	sync() << "....... messages()";

	queue::QueueHandle main;

	thread.post("QUERY-1", query, queue::QueueHandle(), 2, 2);
	main.post("f.42", f, 42);
	thread.post("f.256", []{ f(256); });
	thread.post("f.512/A", [=]{
		sync() << "....... [thread].post(...) (should be local)";
		thread.post("f.512/B", f, 512);
	});
	thread.post("main/stop", [=]{
		sync() << "....... [main].stop()";
		main.stop();
	});
}

int main(int argc, char* argv[])
{
	sync() << "         main()";

	queue::start("main", 1);

	auto th = std::thread([]{
		sync() << "               std::thread(...)";
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
		queue::start("thread", 2);
		queue::run();
		sync() << "... exit";
	});

	queue::wait(th);

	queue::QueueHandle thread { th };
	queue::run(
		[=]{ messages(thread); },
		[=]{ thread.stop(); }
	);

	sync() << "... exit";

	th.join();
	return 0;
}
