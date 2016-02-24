#pragma once

// ag-note: make them debuggable
#define aslr_assert_equal(lhs, rhs) gas_assert(lhs == rhs)
#define aslr_assert_equal_str(lhs, rhs) gas_assert(strcmp(lhs, rhs) == 0)
#define aslr_assert(expr) gas_assert(expr)

// ag-note:. add func/line
#define aslr_not_reachable() gas_assert(false)

