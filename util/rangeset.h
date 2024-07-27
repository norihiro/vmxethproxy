#pragma once

#ifdef __cplusplus

#include <vector>
#include <algorithm>

template<typename T> class rangeset {
#ifdef RANGESET_TEST
public:
#endif
	typedef std::pair<T, T> element_t;
	std::vector<element_t> set;

public:
	void add(const T begin, const T end)
	{
		/*  set:   [-- it0-1 --)      [-- it0 --)
		 *  begin:                ^^^^^^^^^^^^^^^
		 */
		auto it0 = std::partition_point(set.begin(), set.end(),
						[begin](const element_t &e) { return e.second < begin; });
		if (it0 == set.end()) {
			set.push_back(std::make_pair(begin, end));
			return;
		}

		if (end < it0->first) {
			set.insert(it0, std::make_pair(begin, end));
			return;
		}

		// Now, [begin, end) and it0 overlap. Merge into it0.
		if (begin < it0->first)
			it0->first = begin;
		if (it0->second < end)
			it0->second = end;
		else
			return;

		auto it0n = it0 + 1;
		if (it0n == set.end() || end < it0n->first)
			return;

		/*  set:   [-- it1-1 --)      [-- it1 --)
		 *  end:               ^^^^^^^^^^^^^^^^
		 */
		auto it1 = std::partition_point(it0n, set.end(), [end](const element_t &e) { return e.second <= end; });
		if (it1 != set.end() && it1->first <= end)
			it0->second = it1++->second;
		set.erase(it0n, it1);
	}

	bool test(const T &value) const
	{
		/*  set:   [-- it-1 --)      [-- it --)
		 *  value:            ^^^^^^^^^^^^^^
		 */
		auto it = std::partition_point(set.begin(), set.end(),
					       [value](const element_t &e) { return e.second <= value; });
		return it != set.end() && it->first <= value;
	}

	bool test(const T &begin, const T &end) const
	{
		/*  set:   [-- it-1 --)      [-- it --)
		 *  begin:            ^^^^^^^^^^^^^^
		 */
		auto it = std::partition_point(set.begin(), set.end(),
					       [begin](const element_t &e) { return e.second <= begin; });
		return it != set.end() && it->first < end;
	}
};

#endif // __cplusplus
