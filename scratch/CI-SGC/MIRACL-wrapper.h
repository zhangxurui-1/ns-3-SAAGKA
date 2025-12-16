// miracl_wrapper.h
#pragma once

extern "C"
{
#include "miracl.h"
}

// 清理 MIRACL 的全局宏
#undef OFF
#undef ON
#undef TRUE
#undef FALSE
