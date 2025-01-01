/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "Platform/GenericPlatformStackTrace.h"

#ifndef HLVM_USE_BACKWARD_FOR_STACK_TRACE
	#define HLVM_USE_BACKWARD_FOR_STACK_TRACE 0
#endif

#ifndef HLVM_USE_CPPTRACE_FOR_STACK_TRACE
	#define HLVM_USE_CPPTRACE_FOR_STACK_TRACE 0
#endif

#if HLVM_USE_BACKWARD_FOR_STACK_TRACE
	#include <backward.hpp>

FStdString FGenericPlatformStackTrace::GetStackTrace(size_t skip)
{
	backward::StackTrace st;
	st.load_here(10);
	st.skip_n_firsts(1 + skip); // Skip the first frame of backward to get our frame
	std::ostringstream os;
	backward::Printer  p;
	p.print(st, os);
	return FStdString(MoveTemp(os.str()));
}

#elif HLVM_USE_CPPTRACE_FOR_STACK_TRACE

//	#include <cpptrace/cpptrace.hpp>
//
// FStdString FGenericPlatformStackTrace::GetStackTrace(size_t skip)
//{
//	std::ostringstream oss;
// Skip the first frame of backward to get our frame
//	auto			   resolvedTrace = cpptrace::generate_raw_trace(1 + skip, 10).resolve();
//	resolvedTrace.print(oss, false);
//	return FStdString(MoveTemp(oss.str()));
//}

#else
	#include <boost/stacktrace.hpp>

FStdString FGenericPlatformStackTrace::GetStackTrace(size_t skip, size_t max_depth)
{
	std::ostringstream os;
	// Skip the first frame of backward to get our frame
	auto bt = boost::stacktrace::stacktrace(1 + skip, max_depth);
	{
		const std::streamsize w = os.width();
		const std::size_t	  frames = bt.size();
		for (std::size_t i = 0; i < frames; ++i)
		{
			os.width(2);
			os << i;
			os.width(w);
			os << "# ";
			os << bt[i].address();
			os << " ";
			os << bt[i].source_file();
			os << ":";
			os << bt[i].source_line();
			os << " ";
			os << bt[i].name();
			os << '\n';
		}
	}
	return FStdString(MoveTemp(os.str()));
}

#endif
