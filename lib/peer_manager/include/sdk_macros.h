/*
 * Copyright (c) 2015-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file
 *
 * @defgroup sdk_common_macros SDK Common Header
 * @ingroup app_common
 * @brief Macros for parameter checking and similar tasks
 * @{
 */

#ifndef SDK_MACROS_H__
#define SDK_MACROS_H__

#include <zephyr/sys/__assert.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Macro for parameter checking.
 *
 * If @p _cond evaluates to true, does nothing. Otherwise,
 * if @p _module ## _PARAM_CHECK_DISABLED is @e not set (default), prints an error message
 * if @p _printfn is provided, and returns from the calling function context with code @p _err.
 * If @p _module ## _PARAM_CHECK_DISABLED is set, behaves like the ASSERT macro.
 *
 * Parameter checking implemented using this macro can be optionally turned off for release code.
 * Only disable runtime parameter checks if size if a major concern.
 *
 * @param _module   The module name.
 * @param _cond     The condition to be evaluated.
 * @param _err      The error to be returned.
 * @param _printfn  A printf-compatible function used to log the error.
 *                  Leave empty if no logging is needed.
 *
 * @hideinitializer
 */
#define NRF_PARAM_CHECK(_module, _cond, _err, _printfn)                                            \
	do {                                                                                       \
		if ((_cond)) {                                                                     \
			/* Do nothing. */                                                          \
		} else if (!(_module##_PARAM_CHECK_DISABLED)) {                                    \
			_printfn("%s check failed in %s() with value 0x%x.", #_cond, __func__,     \
				 _err);                                                            \
			return _err;                                                               \
		} else {                                                                           \
			__ASSERT((_cond));                                                         \
		}                                                                                  \
	} while (0)

/**
 * @brief Macro for verifying statement to be true. It will cause the exterior function to return
 *        err_code if the statement is not true.
 *
 * @param[in]   statement   Statement to test.
 * @param[in]   err_code    Error value to return if test was invalid.
 *
 * @retval      nothing, but will cause the exterior function to return @p err_code if @p statement
 *              is false.
 */
#define VERIFY_TRUE(statement, err_code)                                                           \
	do {                                                                                       \
		if (!(statement)) {                                                                \
			return err_code;                                                           \
		}                                                                                  \
	} while (0)

/**
 * @brief Macro for verifying statement to be true. It will cause the exterior function to return
 *        if the statement is not true.
 *
 * @param[in]   statement   Statement to test.
 */
#define VERIFY_TRUE_VOID(statement) VERIFY_TRUE((statement),)

/**
 * @brief Macro for verifying statement to be false. It will cause the exterior function to return
 *        err_code if the statement is not false.
 *
 * @param[in]   statement   Statement to test.
 * @param[in]   err_code    Error value to return if test was invalid.
 *
 * @retval      nothing, but will cause the exterior function to return @p err_code if @p statement
 *              is true.
 */
#define VERIFY_FALSE(statement, err_code)                                                          \
	do {                                                                                       \
		if ((statement)) {                                                                 \
			return err_code;                                                           \
		}                                                                                  \
	} while (0)

/**
 * @brief Macro for verifying statement to be false. It will cause the exterior function to return
 *        if the statement is not false.
 *
 * @param[in]   statement    Statement to test.
 */
#define VERIFY_FALSE_VOID(statement) VERIFY_FALSE((statement),)

/**
 * @brief Macro for verifying that a function returned NRF_SUCCESS. It will cause the exterior
 *        function to return error code of statement if it is not @ref NRF_SUCCESS.
 *
 * @param[in] statement     Statement to check against NRF_SUCCESS.
 */
#define VERIFY_SUCCESS(statement)                                                                  \
	do {                                                                                       \
		uint32_t _err_code = (uint32_t)(statement);                                        \
		if (_err_code != NRF_SUCCESS) {                                                    \
			return _err_code;                                                          \
		}                                                                                  \
	} while (0)

/**
 * @brief Macro for verifying that a function returned NRF_SUCCESS. It will cause the exterior
 *        function to return if the err_code is not @ref NRF_SUCCESS.
 *
 * @param[in] err_code The error code to check.
 */
#define VERIFY_SUCCESS_VOID(err_code) VERIFY_TRUE_VOID((err_code) == NRF_SUCCESS)

/**
 * @brief Macro for verifying that the module is initialized. It will cause the exterior function to
 *        return @ref NRF_ERROR_INVALID_STATE if not.
 *
 * @note MODULE_INITIALIZED must be defined in each module using this macro. MODULE_INITIALIZED
 *       should be true if the module is initialized, false if not.
 */
#define VERIFY_MODULE_INITIALIZED() VERIFY_TRUE((MODULE_INITIALIZED), NRF_ERROR_INVALID_STATE)

/**
 * @brief Macro for verifying that the module is initialized. It will cause the exterior function to
 *        return if not.
 *
 * @note MODULE_INITIALIZED must be defined in each module using this macro. MODULE_INITIALIZED
 *       should be true if the module is initialized, false if not.
 */
#define VERIFY_MODULE_INITIALIZED_VOID() VERIFY_TRUE_VOID((MODULE_INITIALIZED))

/**
 * @brief Macro for verifying that the module is initialized. It will cause the exterior function to
 *        return false if not.
 *
 * @note MODULE_INITIALIZED must be defined in each module using this macro. MODULE_INITIALIZED
 *       should be true if the module is initialized, false if not.
 */
#define VERIFY_MODULE_INITIALIZED_BOOL() VERIFY_TRUE((MODULE_INITIALIZED), false)

/**
 * @brief Macro for verifying that the module is initialized. It will cause the exterior function to
 *        return if not.
 *
 * @param[in] param  The variable to check if is NULL.
 */
#define VERIFY_PARAM_NOT_NULL(param) VERIFY_FALSE(((param) == NULL), NRF_ERROR_NULL)

/**
 * @brief Macro for verifying that the module is initialized. It will cause the exterior function to
 *        return if not.
 *
 * @param[in] param  The variable to check if is NULL.
 */
#define VERIFY_PARAM_NOT_NULL_VOID(param) VERIFY_FALSE_VOID(((param) == NULL))

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* SDK_MACROS_H__ */
