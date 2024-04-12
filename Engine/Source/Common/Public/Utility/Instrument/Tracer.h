/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

class ITracer
{
public:
	virtual ~ITracer() = default;

	virtual void Init(const TCHAR* InitMessage = nullptr) = 0;
	virtual void Final(const TCHAR* FinalMessage = nullptr) = 0;

	virtual void BeginScope(const TCHAR* message) = 0;
	virtual void EndScope(const TCHAR* message) = 0;

	virtual void NameThread(const TCHAR* ThreadName) = 0;
};
