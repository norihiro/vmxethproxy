#include <cstdio>
#undef NDEBUG
#include <cassert>

#define RANGESET_TEST
#include "rangeset.h"

using namespace std;

int test_set(const vector<pair<int, int>> &v)
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

int main(int argc, char **argv)
{
	int ret = 0;

	for (int a = 0; a < 18; a += 2) {
		for (int b = a + 2; b < 18; b += 2) {
			for (int c = 0; c < 18; c += 2) {
				for (int d = c + 2; d < 18; d += 2) {
					rangeset<int> r;
					r.add(a, b);
					r.add(c, d);
					printf("[%d %d) [%d %d) =>", a, b, c, d);
					for (int i = 0; i < r.set.size(); i++)
						printf(" [%d %d)", r.set[i].first, r.set[i].second);
					printf("\n");

					ret |= test_set(r.set);

					for (int i = -1; i <= 18; i++) {
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

	for (int a = -4; a <= 32; a += 2) {
		for (int b = a + 2; b <= 34; b += 2) {
			rangeset<int> r;
			r.add(0, 6);
			r.add(12, 18);
			r.add(24, 30);
			r.add(a, b);

			printf("[0 6) [12 18) [24 30) [%d %d) =>", a, b);
			for (int i = 0; i < r.set.size(); i++)
				printf(" [%d %d)", r.set[i].first, r.set[i].second);
			printf("\n");

			ret |= test_set(r.set);

			for (int i = -5; i <= 35; i++) {
				bool expected = (0 <= i && i < 6) || (12 <= i && i < 18) || (24 <= i && i < 30) ||
						(a <= i && i < b);
				bool actual = r.test(i);
				if (actual != expected) {
					printf("Error: a=%d b=%d i=%d actual=%d expected=%d\n", a, b, i, actual,
					       expected);
					ret = 1;
				}
			}
		}
	}

	return ret;
}
