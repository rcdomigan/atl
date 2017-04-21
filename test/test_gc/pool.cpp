#include <iostream>
#include <vector>

#include <atl/gc/pool.hpp>

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

TEST(TestPool, test_bit_manipulations)
{
	using namespace atl;

	typedef uint8_t Bits;
	typedef memory_pool::Pool<uintptr_t, Bits, 512> Pool;

	Pool pool{};

	auto bit_fields = Pool::FIELDS;
	ASSERT_EQ(128, bit_fields);

	ASSERT_EQ(1, Pool::_alloc_bit(0));
	ASSERT_EQ(2, Pool::_mark_bit(0));

	// 01000000
	ASSERT_EQ(64, Pool::_alloc_bit(3));
	// 10000000
	ASSERT_EQ(128, Pool::_mark_bit(3));

	ASSERT_EQ(0, Pool::_field(0));
	ASSERT_EQ(0, Pool::_field(3));
	ASSERT_EQ(1, Pool::_field(4));
	ASSERT_EQ(1, Pool::_field(7));
	ASSERT_EQ(2, Pool::_field(8));

	pool.mark(pool._begin + 2);
	ASSERT_TRUE(pool.is_marked(2));
	ASSERT_FALSE(pool.is_marked(1));
	ASSERT_FALSE(pool.is_marked(3));
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

	auto pool = memory_pool::Pool<test::CallbackOnDestruct, unsigned char, 8>();

	new (pool.alloc())CBD(update_destroyed, "a");

	CBD *b = new (pool.alloc())CBD(update_destroyed, "b");
	CBD *c = new (pool.alloc())CBD(update_destroyed, "c");

	ASSERT_FALSE(pool.is_marked(b));
	pool.mark(b);
	ASSERT_TRUE(pool.is_marked(b));

	ASSERT_FALSE(pool.is_marked(c));
	pool.mark(c);
	ASSERT_TRUE(pool.is_marked(c));

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

