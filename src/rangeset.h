#pragma once

#ifdef __cplusplus

#include <vector>
#include <algorithm>

template<typename T> class rangeset {
#ifdef RANGESET_TEST
public:
#endif
	typedef std::pair<T, T> element_t;
	typedef std::vector<element_t> container_t;
	container_t set;

public:
	void add(const T begin, const T end)
	{
		auto it0 = std::partition_point(set.begin(), set.end(),
						[begin](const element_t &e) { return e.second < begin; });
		if (it0 == set.end()) {
			set.push_back(std::make_pair(begin, end));
			return;
		}

		if (it0->first <= begin /* && begin <= it0->second */) {
			it0->second = std::max(it0->second, end);
		}
		else if (/* begin < it0->first && */ it0->first <= end) {
			it0->first = begin;
			it0->second = std::max(it0->second, end);
		}
		else /* if (end < it0->first) */ {
			set.insert(it0, std::make_pair(begin, end));
			return;
		}

		auto it1 = std::partition_point(it0, set.end(), [end](const element_t &e) { return e.first <= end; });

		typename container_t::iterator it1b = it1 - 1;
		if (it1b->first <= end && end < it1b->second)
			it0->second = std::max(it0->second, it1b->second);

		if (it0 != it1 && it0 + 1 != it1)
			set.erase(it0 + 1, it1);
	}

	bool test(const T &value) const
	{
		auto it = std::partition_point(set.begin(), set.end(),
					       [value](const element_t &e) { return e.second < value; });
		return it != set.end() && it->first <= value && value < it->second;
	}
};

#endif // __cplusplus
