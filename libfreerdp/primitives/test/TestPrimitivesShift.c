/* test_shift.c
 * vi:ts=4 sw=4
 *
 * (c) Copyright 2012 Hewlett-Packard Development Company, L.P.
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0.
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include <freerdp/config.h>

#include <winpr/sysinfo.h>
#include "prim_test.h"

#define FUNC_TEST_SIZE 65536

static BOOL test_lShift_16s_func(void)
{
	pstatus_t status = 0;
	INT16 ALIGN(src[FUNC_TEST_SIZE + 3]);
	INT16 ALIGN(d1[FUNC_TEST_SIZE + 3]);
	UINT32 val = 0;
	winpr_RAND(&val, sizeof(val));
	winpr_RAND(src, sizeof(src));
	val = val % 16;
	/* Negative tests */
	status = generic->lShiftC_16s(src + 1, 16, d1 + 1, FUNC_TEST_SIZE);

	if (status == PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->lShiftC_16s(src + 1, 16, d1 + 1, FUNC_TEST_SIZE);

	if (status == PRIMITIVES_SUCCESS)
		return FALSE;

	/* Aligned */
	status = generic->lShiftC_16s(src + 1, val, d1 + 1, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->lShiftC_16s(src + 1, val, d1 + 1, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	/* Unaligned */
	status = generic->lShiftC_16s(src + 1, val, d1 + 2, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->lShiftC_16s(src + 1, val, d1 + 2, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	return TRUE;
}

static BOOL test_lShift_16u_func(void)
{
	pstatus_t status = 0;
	UINT16 ALIGN(src[FUNC_TEST_SIZE + 3]);
	UINT16 ALIGN(d1[FUNC_TEST_SIZE + 3]);
	UINT32 val = 0;
	winpr_RAND(&val, sizeof(val));
	winpr_RAND(src, sizeof(src));
	val = val % 16;

	/* Negative tests */
	status = generic->lShiftC_16u(src + 1, 16, d1 + 1, FUNC_TEST_SIZE);

	if (status == PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->lShiftC_16u(src + 1, 16, d1 + 1, FUNC_TEST_SIZE);

	if (status == PRIMITIVES_SUCCESS)
		return FALSE;

	/* Aligned */
	status = generic->lShiftC_16u(src + 1, val, d1 + 1, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->lShiftC_16u(src + 1, val, d1 + 1, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	/* Unaligned */
	status = generic->lShiftC_16u(src + 1, val, d1 + 2, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->lShiftC_16u(src + 1, val, d1 + 2, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	return TRUE;
}

static BOOL test_rShift_16s_func(void)
{
	pstatus_t status = 0;
	INT16 ALIGN(src[FUNC_TEST_SIZE + 3]);
	INT16 ALIGN(d1[FUNC_TEST_SIZE + 3]);
	UINT32 val = 0;
	winpr_RAND(&val, sizeof(val));
	winpr_RAND(src, sizeof(src));
	val = val % 16;

	/* Negative Tests */
	status = generic->rShiftC_16s(src + 1, 16, d1 + 1, FUNC_TEST_SIZE);

	if (status == PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->rShiftC_16s(src + 1, 16, d1 + 1, FUNC_TEST_SIZE);

	if (status == PRIMITIVES_SUCCESS)
		return FALSE;

	/* Aligned */
	status = generic->rShiftC_16s(src + 1, val, d1 + 1, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->rShiftC_16s(src + 1, val, d1 + 1, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	/* Unaligned */
	status = generic->rShiftC_16s(src + 1, val, d1 + 2, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->rShiftC_16s(src + 1, val, d1 + 2, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	return TRUE;
}

static BOOL test_rShift_16u_func(void)
{
	pstatus_t status = 0;
	UINT16 ALIGN(src[FUNC_TEST_SIZE + 3]);
	UINT16 ALIGN(d1[FUNC_TEST_SIZE + 3]);
	UINT32 val = 0;
	winpr_RAND(&val, sizeof(val));
	winpr_RAND(src, sizeof(src));
	val = val % 16;
	/* Negative tests */
	status = generic->rShiftC_16u(src + 1, 16, d1 + 1, FUNC_TEST_SIZE);

	if (status == PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->rShiftC_16u(src + 1, 16, d1 + 1, FUNC_TEST_SIZE);

	if (status == PRIMITIVES_SUCCESS)
		return FALSE;

	/* Aligned */
	status = generic->rShiftC_16u(src + 1, val, d1 + 1, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->rShiftC_16u(src + 1, val, d1 + 1, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	/* Unaligned */
	status = generic->rShiftC_16u(src + 1, val, d1 + 2, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->rShiftC_16u(src + 1, val, d1 + 2, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	return TRUE;
}

static BOOL test_ShiftWrapper_16s_func(void)
{
	pstatus_t status = 0;
	INT16 ALIGN(src[FUNC_TEST_SIZE + 3]) = { 0 };
	INT16 ALIGN(d1[FUNC_TEST_SIZE + 3]) = { 0 };
	UINT32 tmp = 0;
	winpr_RAND(&tmp, sizeof(tmp));
	winpr_RAND(src, sizeof(src));
	INT32 val = WINPR_ASSERTING_INT_CAST(int32_t, tmp % 16);

	/* Negative tests */
	status = generic->shiftC_16s(src + 1, 16, d1 + 1, FUNC_TEST_SIZE);

	if (status == PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->shiftC_16s(src + 1, 16, d1 + 1, FUNC_TEST_SIZE);

	if (status == PRIMITIVES_SUCCESS)
		return FALSE;

	/* Aligned */
	status = generic->shiftC_16s(src + 1, val, d1 + 1, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->shiftC_16s(src + 1, val, d1 + 1, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = generic->shiftC_16s(src + 1, -val, d1 + 1, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->shiftC_16s(src + 1, -val, d1 + 1, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	/* Unaligned */
	status = generic->shiftC_16s(src + 1, val, d1 + 2, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->shiftC_16s(src + 1, val, d1 + 2, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = generic->shiftC_16s(src + 1, -val, d1 + 2, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->shiftC_16s(src + 1, -val, d1 + 2, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	return TRUE;
}

static BOOL test_ShiftWrapper_16u_func(void)
{
	pstatus_t status = 0;
	UINT16 ALIGN(src[FUNC_TEST_SIZE + 3]) = { 0 };
	UINT16 ALIGN(d1[FUNC_TEST_SIZE + 3]) = { 0 };
	UINT32 tmp = 0;
	winpr_RAND(&tmp, sizeof(tmp));
	winpr_RAND(src, sizeof(src));
	INT32 val = WINPR_ASSERTING_INT_CAST(int32_t, tmp % 16);

	/* Negative */
	status = generic->shiftC_16u(src + 1, 16, d1 + 1, FUNC_TEST_SIZE);
	if (status == PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->shiftC_16u(src + 1, 16, d1 + 1, FUNC_TEST_SIZE);

	if (status == PRIMITIVES_SUCCESS)
		return FALSE;

	/* Aligned */
	status = generic->shiftC_16u(src + 1, val, d1 + 1, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->shiftC_16u(src + 1, val, d1 + 1, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = generic->shiftC_16u(src + 1, -val, d1 + 1, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->shiftC_16u(src + 1, -val, d1 + 1, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	/* Unaligned */
	status = generic->shiftC_16u(src + 1, val, d1 + 2, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->shiftC_16u(src + 1, val, d1 + 2, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = generic->shiftC_16u(src + 1, -val, d1 + 2, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->shiftC_16u(src + 1, -val, d1 + 2, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	return TRUE;
}

/* ------------------------------------------------------------------------- */
static BOOL test_lShift_16s_speed(void)
{
	UINT32 val = 0;
	INT16 ALIGN(src[MAX_TEST_SIZE + 1]);
	INT16 ALIGN(dst[MAX_TEST_SIZE + 1]);
	winpr_RAND(src, sizeof(src));
	winpr_RAND(&val, sizeof(val));

	val = val % 16;
	if (!speed_test("lShift_16s", "aligned", g_Iterations, (speed_test_fkt)generic->lShiftC_16s,
	                (speed_test_fkt)optimized->lShiftC_16s, src, val, dst, MAX_TEST_SIZE))
		return FALSE;

	if (!speed_test("lShift_16s", "unaligned", g_Iterations, (speed_test_fkt)generic->lShiftC_16s,
	                (speed_test_fkt)optimized->lShiftC_16s, src + 1, val, dst, MAX_TEST_SIZE))
		return FALSE;

	return TRUE;
}

/* ------------------------------------------------------------------------- */
static BOOL test_lShift_16u_speed(void)
{
	UINT32 val = 0;
	UINT16 ALIGN(src[MAX_TEST_SIZE + 1]);
	UINT16 ALIGN(dst[MAX_TEST_SIZE + 1]);
	winpr_RAND(&val, sizeof(val));
	winpr_RAND(src, sizeof(src));

	val = val % 16;
	if (!speed_test("lShift_16u", "aligned", g_Iterations, (speed_test_fkt)generic->lShiftC_16u,
	                (speed_test_fkt)optimized->lShiftC_16u, src, val, dst, MAX_TEST_SIZE))
		return FALSE;

	if (!speed_test("lShift_16u", "unaligned", g_Iterations, (speed_test_fkt)generic->lShiftC_16u,
	                (speed_test_fkt)optimized->lShiftC_16u, src + 1, val, dst, MAX_TEST_SIZE))
		return FALSE;

	return TRUE;
}

/* ------------------------------------------------------------------------- */
static BOOL test_rShift_16s_speed(void)
{
	UINT32 val = 0;
	INT16 ALIGN(src[MAX_TEST_SIZE + 1]);
	INT16 ALIGN(dst[MAX_TEST_SIZE + 1]);
	winpr_RAND(src, sizeof(src));
	winpr_RAND(&val, sizeof(val));

	val = val % 16;
	if (!speed_test("rShift_16s", "aligned", g_Iterations, (speed_test_fkt)generic->rShiftC_16s,
	                (speed_test_fkt)optimized->rShiftC_16s, src, val, dst, MAX_TEST_SIZE))
		return FALSE;

	if (!speed_test("rShift_16s", "unaligned", g_Iterations, (speed_test_fkt)generic->rShiftC_16s,
	                (speed_test_fkt)optimized->rShiftC_16s, src + 1, val, dst, MAX_TEST_SIZE))
		return FALSE;

	return TRUE;
}

/* ------------------------------------------------------------------------- */
static BOOL test_rShift_16u_speed(void)
{
	UINT32 val = 0;
	UINT16 ALIGN(src[MAX_TEST_SIZE + 1]);
	UINT16 ALIGN(dst[MAX_TEST_SIZE + 1]);
	winpr_RAND(&val, sizeof(val));
	winpr_RAND(src, sizeof(src));

	val = val % 16;
	if (!speed_test("rShift_16u", "aligned", g_Iterations, (speed_test_fkt)generic->rShiftC_16u,
	                (speed_test_fkt)optimized->rShiftC_16u, src, val, dst, MAX_TEST_SIZE))
		return FALSE;

	if (!speed_test("rShift_16u", "unaligned", g_Iterations, (speed_test_fkt)generic->rShiftC_16u,
	                (speed_test_fkt)optimized->rShiftC_16u, src + 1, val, dst, MAX_TEST_SIZE))
		return FALSE;

	return TRUE;
}

int TestPrimitivesShift(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	prim_test_setup(FALSE);

	if (!test_lShift_16s_func())
		return 1;

	if (g_TestPrimitivesPerformance)
	{
		if (!test_lShift_16s_speed())
			return 1;
	}

	if (!test_lShift_16u_func())
		return 1;

	if (g_TestPrimitivesPerformance)
	{
		if (!test_lShift_16u_speed())
			return 1;
	}

	if (!test_rShift_16s_func())
		return 1;

	if (g_TestPrimitivesPerformance)
	{
		if (!test_rShift_16s_speed())
			return 1;
	}

	if (!test_rShift_16u_func())
		return 1;

	if (g_TestPrimitivesPerformance)
	{
		if (!test_rShift_16u_speed())
			return 1;
	}

	if (!test_ShiftWrapper_16s_func())
		return 1;

	if (!test_ShiftWrapper_16u_func())
		return 1;

	return 0;
}
