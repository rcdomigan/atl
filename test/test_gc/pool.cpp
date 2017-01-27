#include <iostream>
#include <vector>

#include <gc/pool.hpp>

#include <functional>

#include <gtest/gtest.h>

namespace test
{
	using namespace atl;

	struct CallbackOnDestruct
	{
		typedef std::function<void (std::string const&)> Callback;

		Callback callback;
		std::string name;

		CallbackOnDestruct(Callback const& callback_, std::string const& name_)
			: callback(callback_), name(name_)
		{}

		~CallbackOnDestruct()
		{ callback(name); }
	};
}


/* Check that the mark bit gets set and unset correctly */
TEST(TestPool, test_mark)
{
	using namespace atl;
	typedef test::CallbackOnDestruct CBD;

	typedef std::set<std::string> Set;
	Set destoyed;

	auto update_destroyed = [&destoyed](std::string const& name)
		{ destoyed.insert(name); };

	auto pool = memory_pool::Pool<test::CallbackOnDestruct>();

	new (pool.alloc())CBD(update_destroyed, "a");

	CBD *b = new (pool.alloc())CBD(update_destroyed, "b"),
		*c = new (pool.alloc())CBD(update_destroyed, "c");

	pool.mark(b);
	pool.mark(c);
	pool.sweep();

	ASSERT_EQ(Set({"a"}), destoyed);

	/* Does re-setting the mark bit work: */
	pool.mark(b);
	pool.sweep();

	ASSERT_EQ(Set({"a", "c"}), destoyed);

	/* does leaving the mark bit unset work:  */
	pool.sweep();

	ASSERT_EQ(Set({"a", "b", "c"}), destoyed);
}
