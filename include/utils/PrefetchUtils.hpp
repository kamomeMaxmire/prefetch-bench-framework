#pragma once
#include <xmmintrin.h>

// prefetch 操作
inline void do_prefetch(const void* p) {
    // 使用 T0 （将位于p位置的数据读取到所有缓存层级（优先L1））
    _mm_prefetch((const char*)p, _MM_HINT_T0);
}