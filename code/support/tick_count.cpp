//
// Copyright (C) YuqiaoZhang(HanetakaChou)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#include "tick_count.h"
#include <assert.h>

#if defined(__GNUC__)

#include <time.h>

extern uint64_t tick_count_per_second()
{
    constexpr uint64_t const tick_count_per_second = 1000000000ULL;
    return tick_count_per_second;
}

extern uint64_t tick_count_now()
{
    struct timespec time_monotonic;
    int result_clock_get_time_monotonic = clock_gettime(CLOCK_MONOTONIC, &time_monotonic);
    assert(0 == result_clock_get_time_monotonic);

    uint64_t const tick_count_now = static_cast<uint64_t>(1000000000ULL) * static_cast<uint64_t>(time_monotonic.tv_sec) + static_cast<uint64_t>(time_monotonic.tv_nsec);
    return tick_count_now;
}

#elif defined(_MSC_VER)

#define NOMINMAX 1
#define WIN32_LEAN_AND_MEAN 1
#include <sdkddkver.h>
#include <Windows.h>

extern uint64_t tick_count_per_second()
{
    LARGE_INTEGER int64_frequency;
    BOOL result_query_performance_frequency = QueryPerformanceFrequency(&int64_frequency);
    assert(NULL != result_query_performance_frequency);

    uint64_t const tick_count_per_second = static_cast<uint64_t>(int64_frequency.QuadPart);
    return tick_count_per_second;
}

extern uint64_t tick_count_now()
{
    LARGE_INTEGER int64_performance_count;
    BOOL result_query_performance_counter = QueryPerformanceCounter(&int64_performance_count);
    assert(NULL != result_query_performance_counter);

    uint64_t const tick_count_now = static_cast<uint64_t>(int64_performance_count.QuadPart);
    return tick_count_now;
}

#else
#error Unknown Compiler
#endif