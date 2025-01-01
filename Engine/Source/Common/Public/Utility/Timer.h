/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include <chrono>

/**
 * @brief Timer that set and return time point in sec/millisec, etc. FTimer will now initialized until user called Reset() or Mark()
 *        This class has the advantage of trivial construction (save couple hundred clocks) for counting down only when you need it.
 */
class FTimer
{
public:
	FTimer() = default;

	/**
	 * @brief 构造函数，创建一个FTimer对象 如果需要重置 调用重置函数
	 */
	explicit FTimer(bool reset)
	{
		if (reset)
		{
			Reset();
		}
	}

	/**
	 * @brief 构造函数，创建一个FTimer对象 period 参数为持续时间，单位为秒
	 */
	FTimer(std::chrono::duration<double, std::ratio<1>> period, bool reset = false)
		: m_period(period.count())
	{
		if (reset)
		{
			Reset();
		}
	}

	/**
	 * @brief 重置Timer到当前时间
	 */
	inline void Reset() noexcept
	{
		m_last = std::chrono::steady_clock::now();
		m_init = true;
	}

	/**
	 * @brief 泛型方法，返回从上次重置到现在的时间间隔，默认单位为秒
	 * @return 时间间隔，单位为秒
	 */
	template <typename ratio = std::ratio<1>, typename ret_type = double>
	inline ret_type Mark(bool reset = false) noexcept
	{
		auto now = std::chrono::steady_clock::now();
		if (!m_init)
			HLVM_UNLIKELY
			{
				m_last = now;
				m_init = true;
				return 0;
			}
		ret_type ret = std::chrono::duration<ret_type, ratio>(now - m_last).count();
		if (reset)
			HLVM_UNLIKELY
			{
				m_last = now;
			}
		return ret;
	}

	/**
	 * @brief 返回从上次重置到现在的时间间隔，单位为秒
	 * @param reset 是否重置Timer
	 * @return 时间间隔，单位为秒
	 */
	inline double MarkSec(bool reset = false) noexcept
	{
		return Mark<std::ratio<1>, double>(reset);
	}

	/**
	 * @brief 返回从上次重置到现在的时间间隔，单位为毫秒
	 * @param reset 是否重置Timer
	 * @return 时间间隔，单位为毫秒
	 */
	inline double MarkMilli(bool reset = false) noexcept
	{
		return Mark<std::milli, double>(reset);
	}

	/**
	 * @brief 返回从上次重置到现在的时间间隔，单位为微秒
	 * @param reset 是否重置Timer
	 * @return 时间间隔，单位为微秒
	 */
	inline double MarkMicro(bool reset = false) noexcept
	{
		return Mark<std::micro, double>(reset);
	}

	/**
	 * @brief 返回从上次重置到现在的时间间隔，单位为纳秒
	 * @param reset 是否重置Timer
	 * @return 时间间隔，单位为纳秒
	 */
	inline double MarkNano(bool reset = false) noexcept
	{
		return Mark<std::nano, double>(reset);
	}

	/**
	 * @brief 设置Timer的周期
	 * @param period 周期，单位为秒
	 */
	inline void SetPeriod(std::chrono::duration<double, std::ratio<1>> period) noexcept
	{
		m_period = period.count();
	}

	/**
	 * @brief 检查是否满足周期要求
	 * @param reset 是否重置Timer
	 * @return 是否满足周期要求
	 */
	inline bool Check(bool reset = false) noexcept
	{
		if (m_period <= 0.0)
		{
			return true;
		}
		if (MarkSec(reset) < m_period)
		{
			return false;
		}
		return true;
	}

private:
	TTimePoint m_last;			///< 时间点
	double	   m_period{ 0.0 }; ///< 周期
	BIT_FLAG(m_init){ false };	///< 是否初始化
};
