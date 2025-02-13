/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "Test.h"
#include "Core/Log.h"
#include "Core/Parallel/Async/Async.h"

DECLARE_LOG_CATEGORY(LogTest)

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wmissing-noreturn"
#pragma clang diagnostic ignored "-Wcast-function-type-strict"
#pragma clang diagnostic ignored "-Wunused-parameter"
#include "TestGLFW3_Data/deps/tinycthread.h"
#include "TestGLFW3_Data/deps/tinycthread.c"

#define GLAD_GL_IMPLEMENTATION
#include "TestGLFW3_Data/deps/glad/gl.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

typedef struct
{
	GLFWwindow* window;
	const char* title;
	float		r, g, b;
	thrd_t		id;
} Thread;

static volatile int running = GLFW_TRUE;

static void error_callback(int error, const char* description)
{
	fprintf(stderr, "Error %d: %s\n", error, description);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GLFW_TRUE);
}

static auto lock = FAtomicFlag();

static int thread_main(void* data)
{
	const Thread* thread = reinterpret_cast<const Thread*>(data);

	{
		// ATOMIC_LOCK_GUARD(lock);
		glfwMakeContextCurrent(thread->window);
		glfwSwapInterval(1);
	}

	while (running)
	{
		// ATOMIC_LOCK_GUARD(lock);
		const float v = static_cast<float>(fabs(sin(glfwGetTime() * 2.)));
		glClearColor(thread->r * v, thread->g * v, thread->b * v, 0.f);

		glClear(GL_COLOR_BUFFER_BIT);
		glfwSwapBuffers(thread->window);
	}
	{
		// ATOMIC_LOCK_GUARD(lock);
		glfwMakeContextCurrent(NULL);
	}
	return 0;
}

/*
	<test method>
*/
RECORD_BOOL(glfw_thread_test)
{
	int	   i, result;
	Thread threads[] = {
		{ NULL, "Red", 1.f, 0.f, 0.f, 0 },
		{ NULL, "Green", 0.f, 1.f, 0.f, 0 },
		{ NULL, "Blue", 0.f, 0.f, 1.f, 0 }
	};
	const int count = sizeof(threads) / sizeof(Thread);

	glfwSetErrorCallback(error_callback);

	if (!glfwInit())
		return false;

	for (i = 0; i < count; i++)
	{
		glfwWindowHint(GLFW_POSITION_X, 200 + 250 * i);
		glfwWindowHint(GLFW_POSITION_Y, 200);

		threads[i].window = glfwCreateWindow(200, 200,
			threads[i].title,
			NULL, NULL);
		if (!threads[i].window)
		{
			glfwTerminate();
			return false;
		}

		glfwSetKeyCallback(threads[i].window, key_callback);
	}

	glfwMakeContextCurrent(threads[0].window);
	gladLoadGL(glfwGetProcAddress);
	glfwMakeContextCurrent(NULL);

	std::vector<std::future<void>> jobs;
	for (i = 0; i < count; i++)
	{
		jobs.emplace_back(FAsync::Launch(EAsyncMode::PoolOrderlessExec,
			[_i = i, &threads]() {
				thread_main(threads + _i);
			}));
	}

	FTimer timer;
	while (running)
	{
		// glfwWaitEvents(); // YuHang : Blocking, so not used here
		glfwPollEvents();

		for (i = 0; i < count; i++)
		{
			if (glfwWindowShouldClose(threads[i].window))
				running = GLFW_FALSE;
		}

		if (!running || timer.MarkSec() > 2.0)
		{
			for (i = 0; i < count; i++)
			{
				glfwSetWindowShouldClose(threads[i].window, GLFW_TRUE);
			}
		}
	}

	for (i = 0; i < count; i++)
		glfwHideWindow(threads[i].window);

	for (auto& job : jobs)
	{
		job.wait();
	}

	return true;
};
#pragma clang diagnostic pop
