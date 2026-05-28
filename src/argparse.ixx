// Copyright (c) Team CharLS.
// SPDX-License-Identifier: MIT

module;

#include "macros.hpp"

export module argparse;

#define ARGPARSE_MODULE_USE_STD_MODULE
import std;
import std.compat;

MSVC_WARNING_SUPPRESS(5244 5246 4866 26496)
#include "argparse/argparse.hpp"
MSVC_WARNING_UNSUPPRESS()

export namespace argparse {

using argparse::default_arguments;
using argparse::nargs_pattern;
using argparse::operator&;
using argparse::Argument;
using argparse::ArgumentParser;

} // namespace argparse