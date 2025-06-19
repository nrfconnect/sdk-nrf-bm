#include <string.h>
#include "nrf_mtx.h"
#include "unity.h"


void setUp(void)
{
}

void tearDown(void)
{
}

void test_mutex(void)
{
	nrf_mtx_t mtx = 0xDEADBEEF;

	nrf_mtx_init(&mtx);
	TEST_ASSERT_TRUE(mtx == NRF_MTX_UNLOCKED);

	TEST_ASSERT_TRUE(nrf_mtx_trylock(&mtx));
	TEST_ASSERT_TRUE(mtx == NRF_MTX_LOCKED);

	TEST_ASSERT_FALSE(nrf_mtx_trylock(&mtx));
	TEST_ASSERT_FALSE(nrf_mtx_trylock(&mtx));

	nrf_mtx_unlock(&mtx);
	TEST_ASSERT_TRUE(mtx == NRF_MTX_UNLOCKED);
}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
