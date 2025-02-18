/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "Test.h"

#include "Core/Log.h"
#include "Utility/Timer.h"
#include "Utility/Hash.h"

#include <boost/chrono.hpp>
#include <boost/thread/thread.hpp>

DECLARE_LOG_CATEGORY(LogTest)


/*
	<test method>
*/
static bool test_timer_test()
{
	using namespace std::chrono_literals;
	HLVM_LOG(LogTest, info, TXT("Timer Test"));
	{
		// Trivially construct w/o reset, and no period given
		FTimer Timer;
		HLVM_ENSURE_F(Timer.Mark() == 0, TXT("Timer Mark() should be zero when not Reset()"));
		HLVM_ENSURE_F(Timer.Check() == true, TXT("Timer check should be true for period == 0"));
		HLVM_ENSURE_F(Timer.Mark() > 0, TXT("Timer Mark() should be greater than zero second calling"));
	}
	{
		// construct with reset
		FTimer Timer{ true };
		HLVM_ENSURE_F(Timer.Mark() > 0, TXT("Timer Mark() should be greater than zero second calling"));
		HLVM_ENSURE_F(Timer.Check() == true, TXT("Timer check should be true for period == 0"));
	}
	{
		// construct with period but no reset
		FTimer Timer{ 1s };
		HLVM_ENSURE_F(Timer.Mark() == 0, TXT("Timer Mark() should be zero when not Reset()"));
		HLVM_ENSURE_F(Timer.Check() == false, TXT("Timer check should be false for period not reached"));
		sleep(1);
		HLVM_ENSURE_F(Timer.Check() == true, TXT("Timer check should be false for period reached"));
	}
	{
		// construct with period but no reset
		FTimer Timer{ 1s, true };
		HLVM_ENSURE_F(Timer.Mark() > 0, TXT("Timer Mark() should be greater than zero second calling"));
		HLVM_ENSURE_F(Timer.Check(true) == false, TXT("Timer check should be false for period not reached"));
		boost::this_thread::sleep_for(boost::chrono::milliseconds(501));
		HLVM_ENSURE_F(Timer.Check() == false, TXT("Timer check should be false for period not reached"));
		boost::this_thread::sleep_for(boost::chrono::milliseconds(501));
		HLVM_ENSURE_F(Timer.Check() == true, TXT("Timer check should be false for period reached"));
		HLVM_LOG(LogTest, info, TXT("Timer mark on sleep finished {0}"), Timer.Mark());
	}
	return true;
};
RECORD_TEST_FUNC(timer_test);

static bool test_hash_test()
{
	// Generate some text data
	std::string TextData = "Hello World";
	{
		FMD5Digest Digest = FMD5Hash::Hash(TextData.c_str(), TextData.length());
		HLVM_LOG(LogTest, info, TXT("MD5 Hash digest for {0} is {1}"), TO_TCHAR_CSTR(TextData.c_str()), *Digest.ToString());
	}
	{
		FSHA1Digest Digest = FSHA1Hash::Hash(TextData.c_str(), TextData.length());
		HLVM_LOG(LogTest, info, TXT("SHA1 Hash digest for {0} is {1}"), TO_TCHAR_CSTR(TextData.c_str()), *Digest.ToString());
	}
	return true;
};
RECORD_TEST_FUNC(hash_test);
