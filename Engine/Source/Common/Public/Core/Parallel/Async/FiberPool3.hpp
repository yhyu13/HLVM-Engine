// FiberPool based on:
// http://roar11.com/2016/01/a-platform-independent-thread-pool-using-c14/

#pragma once
#include "Core/Parallel/ConcurrentQueue.h"

#include <boost/fiber/all.hpp>
#include <boost/thread.hpp>

namespace FiberPool
{

	inline auto
	no_of_defualt_threads()
	{
		return std::thread::hardware_concurrency() / HLVM_PLATFORM_SIMT;
	}

	/**
	 * A wrapper primarly for boost::fibers::buffered_channel
	 * The buffered_channel has non-virtual member functions
	 * thus cant inherit from it and use it polyformically.
	 * This makes it diffuclt to mock its behaviour in unit tests
	 * The wrapper solves this (see tests for example mock channel)
	 */
	template <typename BaseQueue>
	class TaskQueue
	{
	public:
		using ValueType = typename BaseQueue::ValueType;

		TaskQueue() = default;
		NOCOPYMOVE(TaskQueue)

		void
		push(typename BaseQueue::ValueType const& value)
		{
			return m_base_channel.Push(value);
		}

		void
		push(typename BaseQueue::ValueType&& value)
		{
			return m_base_channel.Push(std::move(value));
		}

		bool
		pop(typename BaseQueue::ValueType& value)
		{
			return m_base_channel.template PopFront<false>(value);
		}

		void close() noexcept
		{
			m_base_channel.SignalStop();
		}

	private:
		BaseQueue m_base_channel;
	};

	/**
	 * All tasks executed by the FiberPool are
	 * automatically wrapped to use the
	 * following interface
	 */
	class IFiberTask
	{
	public:
		using Func = std::function<void(void)>;

		// how many running fibers there are
		inline static std::atomic_uint_fast32_t no_of_fibers{ 0 };

		IFiberTask() = default;
		explicit IFiberTask(Func&& func)
			: m_func{ std::move(func) }
		{
		}

		IFiberTask(const IFiberTask& rhs) = delete;
		IFiberTask& operator=(const IFiberTask& rhs) = delete;
		IFiberTask(IFiberTask&& other) = default;
		IFiberTask& operator=(IFiberTask&& other) = default;

		/**
		 * Run the task.
		 */
		void operator()()
		{
			no_of_fibers.fetch_add(1, std::memory_order_relaxed);
			m_func();
			no_of_fibers.fetch_sub(1, std::memory_order_relaxed);
		}

	private:
		Func m_func;
	};

	template <
		bool use_work_steal = false,
		typename work_task_t = std::tuple<boost::fibers::launch, IFiberTask>,
		typename task_queue_t = TConcurrentQueue<work_task_t>>
	class FiberPool
	{
	public:
		static constexpr bool work_stealing = use_work_steal;

		FiberPool()
			: FiberPool{ no_of_defualt_threads() }
		{
		}

		FiberPool(
			size_t no_of_threads)
			: m_threads_no{ no_of_threads }
		{
			try
			{
				for (std::uint32_t i = 0; i < m_threads_no; ++i)
				{
					m_threads.create_thread([this] { this->worker(); });
				}
			}
			catch (...)
			{
				close_queue();
				throw;
			}
		}

		/**
		 * Submit a task to be executed as fiber by worker threads
		 */
		template <typename Func, typename... Args>
		auto submit(boost::fibers::launch launch_policy,
			Func&&						  func, Args&&... args)
		{
			using TaskRetType = std::invoke_result_t<Func, Args...>;
			using TaskType = boost::fibers::packaged_task<TaskRetType()>;
			using task_t = IFiberTask;

			auto task = new TaskType(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
			auto result_future = task->get_future();
			auto work = [task]() { (*task)(); delete task; };

			// finally submit the packaged task into work queue
			m_work_queue.push(
				std::move(std::make_tuple(launch_policy,
					task_t(std::move(work)))));

			// return the future to the caller so that
			// we can get the result when the fiber with our task
			// completes
			return std::make_optional(std::move(result_future));
		}

		/**
		 * Use boost::fibers:launch::post as
		 * default lanuch strategy for fibers
		 */
		template <typename Func, typename... Args>
		auto submit(Func&& func, Args&&... args)
		{
			return submit(boost::fibers::launch::dispatch,
				std::forward<Func>(func),
				std::forward<Args>(args)...);
		}

		/**
		 * Non-copyable.
		 */
		FiberPool(FiberPool const& rhs) = delete;

		/**
		 * Non-assignable.
		 */
		FiberPool& operator=(FiberPool const& rhs) = delete;

		void close_queue() noexcept
		{
			m_work_queue.close();
		}

		size_t threads_no() const noexcept
		{
			return m_threads.size();
		}

		size_t fibers_no() const noexcept
		{
			return IFiberTask::no_of_fibers.load(std::memory_order_acquire);
		}

		~FiberPool()
		{
			close_queue();
			m_threads.join_all();
		}

	private:
		/**
		 * worker thread method. It participates with
		 * shared_work sheduler of fibers.
		 *
		 * It takes packaged taskes from the work_queue
		 * and launches fibers executing the tasks
		 */
		void worker()
		{
			// make this thread participate in shared_work
			// fiber sharing
			//

			if constexpr (work_stealing)
			{
				// work_stealing sheduling is much faster
				// than work_shearing, but it does not
				// allow for modifying number of threads
				// at runtime. Therefore if one uses
				// DefaultFiberPool, no other instance
				// of the fiber pool can be created
				// as this would change the number of
				// worker threads
				boost::fibers::use_scheduling_algorithm<
					boost::fibers::algo::work_stealing>(
					m_threads_no, true);
			}
			else
			{
				// it is slower but, can vary number of
				// worker threads at runtime. So you can
				// use DefaultFiberPool in one part of
				// you application, and custom instance
				// of the fiber pool in other part.
				boost::fibers::use_scheduling_algorithm<
					boost::fibers::algo::shared_work>(true);
			}

			// create a placeholder for packaged task for
			// to-be-created fiber to execute
			auto task_tuple = typename decltype(m_work_queue)::ValueType{};

			// fetch a packaged task from the work queue.
			// if there is nothing, we are just going to wait
			// here till we get some task
			while (m_work_queue.pop(task_tuple))
			{
				// creates a fiber from the pacakged task.
				//
				// the fiber is immedietly detached so that we
				// fetch next task from the queue without blocking
				// the thread and waiting here for the fiber to
				// complete

				// the task is tuple with launch policy and
				// accutal packaged_task to run
				auto& [launch_policy, task_to_run] = task_tuple;

				// earlier we already got future for the fiber
				// so we can get the result of our task if we want
				boost::fibers::fiber(launch_policy,
					std::move(task_to_run))
					.detach();
			}
		}

		size_t m_threads_no{ 1 };

		// worker threads. these are the threads which will
		// be executing our fibers. Since we use work_shearing scheduling
		// algorithm, the fibers should be shared evenly
		// between these threads
		boost::thread_group m_threads;

		// use buffered_channel (by default) so that we dont block when
		// there is no  reciver for the fiber. we are only
		// going to block when the buffered_channel is full.
		// Otherwise, tasks will be just waiting in the
		// queue till some fiber picks them up.
		TaskQueue<task_queue_t> m_work_queue;
	};

	template <
		typename work_task_t = std::tuple<boost::fibers::launch, IFiberTask>,
		typename task_queue_t = TConcurrentQueue<work_task_t>>
	using FiberPoolStealing = FiberPool<true, work_task_t, task_queue_t>;

	template <
		typename work_task_t = std::tuple<boost::fibers::launch, IFiberTask>,
		typename task_queue_t = TConcurrentQueue<work_task_t>>
	using FiberPoolSharing = FiberPool<false, work_task_t, task_queue_t>;

} // namespace FiberPool

/**
 * A static default FiberPool in which
 * number of threads is set automatically based
 * on your hardware
 */
namespace DefaultFiberPool
{

	inline auto
	get_pool()
	{
		/**
		 * New this static variable to avoid deleting on destruction,
		 * as boost scheduling context has life time problem with mimalloc allocation (free before destruction)
		 */
		static auto default_fp = new FiberPool::FiberPoolSharing<>{};
		return default_fp;
	};

	template <typename Func, typename... Args>
	inline auto
	submit_job(boost::fibers::launch launch_policy,
		Func&&						 func, Args&&... args)
	{
		return get_pool()->submit(
			launch_policy,
			std::forward<Func>(func),
			std::forward<Args>(args)...);
	}

	template <typename Func, typename... Args>
	inline auto
	submit_job(Func&& func, Args&&... args)
	{
		return get_pool()->submit(
			std::forward<Func>(func),
			std::forward<Args>(args)...);
	}

	inline void
	close()
	{
		get_pool()->close_queue();
	}

} // namespace DefaultFiberPool
