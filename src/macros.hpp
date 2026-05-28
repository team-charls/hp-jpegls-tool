// Copyright (c) Team CharLS.
// SPDX-License-Identifier: MIT

#pragma once

#define MSVC_WARNING_SUPPRESS(x) \
    __pragma(warning(push)) __pragma(warning(disable : x)) // NOLINT(misc-macro-parentheses, bugprone-macro-parentheses)
#define MSVC_WARNING_UNSUPPRESS() __pragma(warning(pop))
#define MSVC_WARNING_SUPPRESS_NEXT_LINE(x) \
    __pragma(                              \
        warning(suppress : x)) // NOLINT(misc-macro-parentheses, bugprone-macro-parentheses, cppcoreguidelines-macro-usage)
