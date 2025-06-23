/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *
 * @defgroup bm_util NCS Bare Metal Utilities library
 * @{
 */

#ifndef BM_UTIL_H__
#define BM_UTIL_H__

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void bm_util_critical_section_enter(bool *is_nested);

void bm_util_critical_section_exit(bool is_nested);

/**
 * @brief Enter a critical section.
 *
 * @note Due to implementation details, there must exist one and only one call to
 *       BM_UTIL_CRITICAL_SECTION_EXIT() for each call to BM_UTIL_CRITICAL_SECTION_ENTER(), and
 *       they must be located in the same scope.
 */
#define BM_UTIL_CRITICAL_SECTION_ENTER()                                                           \
	{                                                                                          \
		bool __CS_NESTED = false;                                                          \
		bm_util_critical_section_enter(&__CS_NESTED);

/**
 * @brief Exit a critical section.
 *
 * @note Due to implementation details, there must exist one and only one call to
 *       BM_UTIL_CRITICAL_SECTION_EXIT() for each call to BM_UTIL_CRITICAL_SECTION_ENTER(), and
 *       they must be located in the same scope.
 */
#define BM_UTIL_CRITICAL_SECTION_EXIT()                                                            \
		bm_util_critical_section_exit(__CS_NESTED);                                        \
	}

#ifdef __cplusplus
}
#endif

#endif /* BM_UTIL_H__ */

/** @} */
