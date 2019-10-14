/*
    This file is a part of Q2Ami project (AmiBroker data-source plugin to fetch
    data from QUIK terminal over the net; requires https://github.com/Arech/t18qsrv)
    Copyright (C) 2019, Arech (aradvert@gmail.com; https://github.com/Arech)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
// Q2Ami.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"


#ifdef BOOST_ENABLE_ASSERT_HANDLER

namespace boost {
	T18_COMP_SILENCE_MISSING_NORETURN;
	void assertion_failed_msg(char const * expr, char const * msg, char const * function, char const * file, long line) {
		T18_ASSERT(!"Boost assertion failed!");

		char _buf[4096];
		sprintf_s(_buf, "Boost assertion failed @%s / %s (%d): \"%s\". Message=%s", file ? file : ""
			, function ? function : "", line, expr ? expr : "", msg ? msg : "");
		T18_COMP_SILENCE_ZERO_AS_NULLPTR;
		::MessageBoxA(NULL, _buf, "Assertion failed", MB_OK | MB_ICONERROR);
		T18_COMP_POP;
		
		::std::terminate();
	}

	void assertion_failed(char const * expr, char const * function, char const * file, long line) {
		assertion_failed_msg(expr, nullptr, function, file, line);
	}
	T18_COMP_POP
} // namespace boost

#endif
