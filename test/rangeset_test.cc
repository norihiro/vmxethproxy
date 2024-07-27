#include <cstdio>
#include <cstdint>
#undef NDEBUG
#include <cassert>

#define RANGESET_TEST
#include "util/rangeset.h"

using namespace std;

typedef uint32_t value_t;

int test_set(const vector<pair<value_t, value_t>> &v)
{
	for (int i = 0; i < v.size(); i++) {
		if (v[i].first >= v[i].second) {
			printf("Error in r.set at %d\n", i);
			return 1;
		}

		if (i && v[i - 1].second >= v[i].first) {
			printf("Error in r.set at %d %d\n", i - 1, i);
			return 1;
		}
	}

	return 0;
}

static bool overwrap(value_t a, value_t b, value_t i, value_t j)
{
	if (j <= a)
		return false;
	if (b <= i)
		return false;
	return true;
}

int main(int argc, char **argv)
{
	int ret = 0;

	for (value_t a = 1; a < 19; a += 2) {
		for (value_t b = a + 2; b < 19; b += 2) {
			for (value_t c = 0; c < 19; c += 2) {
				for (value_t d = c + 2; d < 19; d += 2) {
					rangeset<value_t> r;
					r.add(a, b);
					r.add(c, d);
					printf("[%d %d) [%d %d) =>", a, b, c, d);
					for (int i = 0; i < r.set.size(); i++)
						printf(" [%d %d)", r.set[i].first, r.set[i].second);
					printf("\n");

					ret |= test_set(r.set);

					for (value_t i = 0; i <= 19; i++) {
						bool actual = r.test(i);
						bool expected = (a <= i && i < b) || (c <= i && i < d);
						if (actual != expected) {
							printf("Error: a=%d b=%d c=%d d=%d i=%d actual=%d expected=%d\n",
							       a, b, c, d, i, actual, expected);
							ret = 1;
						}
					}
				}
			}
		}
	}

	for (value_t a = 4; a <= 40; a += 2) {
		for (value_t b = a + 2; b <= 42; b += 2) {
			rangeset<value_t> r;
			r.add(8, 14);
			r.add(20, 26);
			r.add(32, 38);
			r.add(a, b);

			printf("[8 14) [20 26) [32 38) [%d %d) =>", a, b);
			for (value_t i = 0; i < r.set.size(); i++)
				printf(" [%u %u)", r.set[i].first, r.set[i].second);
			printf("\n");

			for (value_t i = 1; i < r.set.size(); i++) {
				if (r.set[i - 1].second >= r.set[i].first) {
					printf("Error: wrong order %d >= %d\n", r.set[i - 1].second, r.set[i].first);
					ret = 1;
				}
			}

			ret |= test_set(r.set);

			for (value_t i = 3; i <= 43; i++) {
				bool expected = (8 <= i && i < 14) || (20 <= i && i < 26) || (32 <= i && i < 38) ||
						(a <= i && i < b);
				bool actual = r.test(i);
				if (actual != expected) {
					printf("Error: a=%u b=%u i=%u actual=%d expected=%d\n", a, b, i, actual,
					       expected);
					ret = 1;
				}
			}

			for (value_t i = 3; i < 43; i++) {
				for (value_t j = i + 1; j <= 43; j++) {
					bool expected = overwrap(8, 14, i, j) || overwrap(20, 26, i, j) ||
							overwrap(32, 38, i, j) || overwrap(a, b, i, j);
					bool actual = r.test(i, j);
					if (actual != expected) {
						printf("Error: a=%u b=%u i=%u j=%u actual=%d expected=%d\n", a, b, i, j,
						       actual, expected);
						ret = 1;
					}
				}
			}
		}
	}

	{
		rangeset<value_t> r;
		if (r.test(0)) {
			printf("Error: empty rangeset: expected `test` returns false but got true.\n");
			ret = 1;
		}
	}

	return ret;
}
