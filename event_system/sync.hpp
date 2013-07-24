#ifndef __SYNC_HPP__
#define __SYNC_HPP__

#include "queue.hpp"
#include <sstream>
#include <iostream>

struct sync
{
	std::ostringstream os;
	bool flushed;
	sync()
		: flushed(false)
	{
		auto id = std::this_thread::get_id();

		auto q = queue::impl::get_queue(id);

		os << id;
		if (q)
		{
			os << "/" << queue::impl::queue_name(q) << ": ";
			for (unsigned int i = 0; i < queue::impl::queue_indent(q); ++i)
				os << "    ";
		}
		else os << ": ";
	}
	~sync() { if (!flushed) flush(); }

	void flush()
	{
		flushed = true;
		os << "\n";
		std::cout << os.str() << std::flush;

		os.clear();
	}

	template <typename T> sync& operator << (T t) { os << t; flushed = false; return *this; }
};


#endif // __SYNC_HPP__