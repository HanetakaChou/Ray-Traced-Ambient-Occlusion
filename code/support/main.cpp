//
// Copyright (C) YuqiaoZhang(HanetakaChou)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#include <stddef.h>
#include <stdint.h>
#include "camera_controller.h"
#include "renderer.h"

static class renderer *g_renderer = NULL;

#if defined(__GNUC__)

#if defined(__linux__) && defined(__ANDROID__)
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <android/native_activity.h>

static int32_t g_resolution_width = -1;
static int32_t g_resolution_height = -1;

static void ANativeActivity_Destroy(ANativeActivity *native_activity);
static void ANativeActivity_WindowFocusChanged(ANativeActivity *native_activity, int hasFocus);
static void ANativeActivity_NativeWindowCreated(ANativeActivity *native_activity, ANativeWindow *native_window);
static void ANativeActivity_NativeWindowResized(ANativeActivity *native_activity, ANativeWindow *native_window);
static void ANativeActivity_NativeWindowRedrawNeeded(ANativeActivity *native_activity, ANativeWindow *native_window);
static void ANativeActivity_NativeWindowDestroyed(ANativeActivity *native_activity, ANativeWindow *native_window);
static void ANativeActivity_InputQueueCreated(ANativeActivity *native_activity, AInputQueue *input_queue);
static void ANativeActivity_InputQueueDestroyed(ANativeActivity *native_activity, AInputQueue *input_queue);

static bool g_this_process_has_inited = false;

static int g_main_thread_looper_draw_callback_fd_read = -1;
static int g_main_thread_looper_draw_callback_fd_write = -1;

static int main_thread_looper_draw_callback(int fd, int, void *);

static pthread_t g_draw_request_thread;
bool volatile g_draw_request_thread_running = false;

static void *draw_request_thread_main(void *);

static int main_thread_looper_input_queue_callback(int fd, int, void *input_queue_void);

extern "C" JNIEXPORT void ANativeActivity_onCreate(ANativeActivity *native_activity, void *saved_state, size_t saved_state_size)
{
	native_activity->callbacks->onStart = NULL;
	native_activity->callbacks->onResume = NULL;
	native_activity->callbacks->onSaveInstanceState = NULL;
	native_activity->callbacks->onPause = NULL;
	native_activity->callbacks->onStop = NULL;
	native_activity->callbacks->onDestroy = ANativeActivity_Destroy;
	native_activity->callbacks->onWindowFocusChanged = ANativeActivity_WindowFocusChanged;
	native_activity->callbacks->onNativeWindowCreated = ANativeActivity_NativeWindowCreated;
	native_activity->callbacks->onNativeWindowResized = ANativeActivity_NativeWindowResized;
	native_activity->callbacks->onNativeWindowRedrawNeeded = ANativeActivity_NativeWindowRedrawNeeded;
	native_activity->callbacks->onNativeWindowDestroyed = ANativeActivity_NativeWindowDestroyed;
	native_activity->callbacks->onInputQueueCreated = ANativeActivity_InputQueueCreated;
	native_activity->callbacks->onInputQueueDestroyed = ANativeActivity_InputQueueDestroyed;
	native_activity->callbacks->onContentRectChanged = NULL;
	native_activity->callbacks->onConfigurationChanged = NULL;
	native_activity->callbacks->onLowMemory = NULL;

	if (!g_this_process_has_inited)
	{
		g_renderer = renderer_init();

		// Simulate the following callback on Android:
		// WM_PAINT
		// CVDisplayLinkSetOutputCallback
		// displayLinkWithTarget
		{
			// the looper of the main thread
			ALooper *main_thread_looper = ALooper_forThread();
			assert(main_thread_looper != NULL);

			// Evidently, the real "draw" is slower than the "request"
			// There are no message boundaries for "SOCK_STREAM", and We can read all the data once
			int sv[2];
			int res_socketpair = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
			assert(0 == res_socketpair);
			g_main_thread_looper_draw_callback_fd_read = sv[0];
			g_main_thread_looper_draw_callback_fd_write = sv[1];

			// the identifier is ignored when callback is not NULL (the ALooper_pollOnce always returns the ALOOPER_POLL_CALLBACK)
			int res_looper_add_fd = ALooper_addFd(main_thread_looper, g_main_thread_looper_draw_callback_fd_read, -1, ALOOPER_EVENT_INPUT, main_thread_looper_draw_callback, NULL);
			assert(1 == res_looper_add_fd);

			// the "draw request" thread
			// TODO: use Load/Store
			g_draw_request_thread_running = false;
			int res_ptread_create = pthread_create(&g_draw_request_thread, NULL, draw_request_thread_main, NULL);
			assert(0 == res_ptread_create);
			while (!g_draw_request_thread_running)
			{
				// pthread_yield();
				sched_yield();
			}
		}

		// Don't worry
		// Single thread
		g_this_process_has_inited = true;
	}
}

// TODO:
// destroy before this process terminates

static void ANativeActivity_Destroy(ANativeActivity *native_activity)
{
}

static void ANativeActivity_WindowFocusChanged(ANativeActivity *native_activity, int hasFocus)
{
}

static void ANativeActivity_NativeWindowCreated(ANativeActivity *native_activity, ANativeWindow *native_window)
{
	g_resolution_width = ANativeWindow_getWidth(native_window);
	g_resolution_height = ANativeWindow_getHeight(native_window);

	renderer_attach_window(g_renderer, native_window);
}

static void ANativeActivity_NativeWindowResized(ANativeActivity *native_activity, ANativeWindow *native_window)
{
}

static void ANativeActivity_NativeWindowRedrawNeeded(ANativeActivity *native_activity, ANativeWindow *native_window)
{
}

static void ANativeActivity_NativeWindowDestroyed(ANativeActivity *native_activity, ANativeWindow *native_window)
{
	renderer_dettach_window(g_renderer);

	g_resolution_width = -1;
	g_resolution_height = -1;
}

static void ANativeActivity_InputQueueCreated(ANativeActivity *native_activity, AInputQueue *input_queue)
{
	// the looper of the main thread
	ALooper *main_thread_looper = ALooper_forThread();
	assert(NULL != main_thread_looper);

	// the identifier is ignored when callback is not NULL (the ALooper_pollOnce always returns the ALOOPER_POLL_CALLBACK)
	AInputQueue_attachLooper(input_queue, main_thread_looper, 0, main_thread_looper_input_queue_callback, input_queue);
}

static void ANativeActivity_InputQueueDestroyed(ANativeActivity *native_activity, AInputQueue *input_queue)
{
	ALooper *looper = ALooper_forThread();
	assert(NULL != looper);

	AInputQueue_detachLooper(input_queue);
}

static int main_thread_looper_draw_callback(int fd, int, void *)
{
	// Evidently, the real "draw" is slower than the "request"
	// There are no message boundaries for "SOCK_STREAM", and We can read all the data once
	{
		uint8_t buf[4096];
		ssize_t res_recv;
		while ((-1 == (res_recv = recv(fd, buf, 4096U, 0))) && (EINTR == errno))
		{
			// pthread_yield();
			sched_yield();
		}
		assert(-1 != res_recv);
	}

	// draw
	renderer_draw(g_renderer);

	return 1;
}

static void *draw_request_thread_main(void *)
{
	g_draw_request_thread_running = true;

	while (g_draw_request_thread_running)
	{
		// 60 FPS
		uint32_t milli_second = 1000U / 60U;

		// wait
		{
			struct timespec request = {((time_t)milli_second) / ((time_t)1000), ((long)1000000) * (((long)milli_second) % ((long)1000))};

			struct timespec remain;
			int res_nanosleep;
			while ((-1 == (res_nanosleep = nanosleep(&request, &remain))) && (EINTR == errno))
			{
				assert(remain.tv_nsec > 0 || remain.tv_sec > 0);
				request = remain;
			}
			assert(0 == res_nanosleep);
		}

		// draw request
		{
			uint8_t buf[1] = {7}; // seven is the luck number
			ssize_t res_send;
			while ((-1 == (res_send = send(g_main_thread_looper_draw_callback_fd_write, buf, 1U, 0))) && (EINTR == errno))
			{
				// pthread_yield();
				sched_yield();
			}
			assert(1 == res_send);
		}
	}

	return NULL;
}

static int main_thread_looper_input_queue_callback(int fd, int, void *input_queue_void)
{
	AInputQueue *input_queue = static_cast<AInputQueue *>(input_queue_void);

	AInputEvent *input_event;
	while (AInputQueue_getEvent(input_queue, &input_event) >= 0)
	{
		// The app will be "No response" if we don't call AInputQueue_finishEvent and pass the non-zero value for all events which is not pre-dispatched
		if (0 == AInputQueue_preDispatchEvent(input_queue, input_event))
		{
			int handled = 0;

			switch (AInputEvent_getType(input_event))
			{
			case AINPUT_EVENT_TYPE_MOTION:
			{

				float Current_X = AMotionEvent_getX(input_event, 0U);
				float Current_Y = AMotionEvent_getY(input_event, 0U);

				float CurrentNormalized_X = Current_X / static_cast<float>(g_resolution_width);
				float CurrentNormalized_Y = Current_Y / static_cast<float>(g_resolution_height);

				switch (AMotionEvent_getAction(input_event) & AMOTION_EVENT_ACTION_MASK)
				{
				case AMOTION_EVENT_ACTION_DOWN:
				{
					g_camera_controller.OnMouseMove(CurrentNormalized_X, CurrentNormalized_Y, false);
				}
				break;
				case AMOTION_EVENT_ACTION_UP:
				{
					g_camera_controller.OnMouseMove(CurrentNormalized_X, CurrentNormalized_Y, false);
				}
				break;
				case AMOTION_EVENT_ACTION_MOVE:
				{
					g_camera_controller.OnMouseMove(CurrentNormalized_X, CurrentNormalized_Y, true);
				}
				break;
				default:
				{
					// Do Nothing
				}
				}
			}
			break;
			default:
			{
				// Do Nothing
			}
			}

			AInputQueue_finishEvent(input_queue, input_event, handled);
		}
	}

	return 1;
}
#else
#error Unknown Platform
#endif

#elif defined(_MSC_VER)

#define NOMINMAX 1
#define WIN32_LEAN_AND_MEAN 1
#include <sdkddkver.h>
#include <Windows.h>
#include <windowsx.h>

static LONG const g_resolution_width = 512U;
static LONG const g_resolution_height = 512U;

static LRESULT CALLBACK wnd_proc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
#if defined(USE_D3D12) && USE_D3D12
	// Do nothing for D3D12 debug layer
#elif defined(USE_VK) && USE_VK
	// Vulkan
#ifndef NDEBUG
	{
		// We assume that the "VkLayer_khronos_validation.json" is at the same directory of the executable file
		WCHAR file_name[4096];
		DWORD res_get_file_name = GetModuleFileNameW(NULL, file_name, sizeof(file_name) / sizeof(file_name[0]));
		assert(0U != res_get_file_name);

		for (int i = (res_get_file_name - 1); i > 0; --i)
		{
			if (L'\\' == file_name[i])
			{
				file_name[i] = L'\0';
				break;
			}
		}

		BOOL res_set_environment_variable = SetEnvironmentVariableW(L"VK_LAYER_PATH", file_name);
		assert(FALSE != res_set_environment_variable);
	}
#endif
#else
#error Unknown Backend
#endif

	ATOM hWndCls;
	{
		WNDCLASSEXW Desc = {
			sizeof(WNDCLASSEX),
			CS_OWNDC,
			wnd_proc,
			0,
			0,
			hInstance,
			LoadIconW(NULL, IDI_APPLICATION),
			LoadCursorW(NULL, IDC_ARROW),
			(HBRUSH)(COLOR_WINDOW + 1),
			NULL,
			L"Demo:0XFFFFFFFF",
			LoadIconW(NULL, IDI_APPLICATION),
		};
		hWndCls = RegisterClassExW(&Desc);
	}

	HWND hWnd;
	{
		HWND hDesktop = GetDesktopWindow();
		HMONITOR hMonitor = MonitorFromWindow(hDesktop, MONITOR_DEFAULTTONEAREST);
		MONITORINFOEXW MonitorInfo;
		MonitorInfo.cbSize = sizeof(MONITORINFOEXW);
		GetMonitorInfoW(hMonitor, &MonitorInfo);

		RECT rect = {(MonitorInfo.rcWork.left + MonitorInfo.rcWork.right) / 2 - g_resolution_width / 2,
					 (MonitorInfo.rcWork.bottom + MonitorInfo.rcWork.top) / 2 - g_resolution_height / 2,
					 (MonitorInfo.rcWork.left + MonitorInfo.rcWork.right) / 2 + g_resolution_width / 2,
					 (MonitorInfo.rcWork.bottom + MonitorInfo.rcWork.top) / 2 + g_resolution_height / 2};
		AdjustWindowRectEx(&rect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, FALSE, WS_EX_APPWINDOW);

		hWnd = CreateWindowExW(WS_EX_APPWINDOW,
							   MAKEINTATOM(hWndCls),
							   L"Demo",
							   WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
							   rect.left,
							   rect.top,
							   rect.right - rect.left,
							   rect.bottom - rect.top,
							   hDesktop,
							   NULL,
							   hInstance,
							   NULL);
	}

	g_renderer = renderer_init();

	renderer_attach_window(g_renderer, hWnd);

	MSG msg;
	while (GetMessageW(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	renderer_dettach_window(g_renderer);

	renderer_destroy(g_renderer);

	return (int)msg.wParam;
}

static LRESULT CALLBACK wnd_proc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_DESTROY:
	{
		PostQuitMessage(0);
	}
		return 0;
	case WM_PAINT:
	{
		renderer_draw(g_renderer);
	}
		return 0;
	case WM_ERASEBKGND:
		return 1;
	case WM_KEYDOWN:
	{
		switch (wParam)
		{
		case 'W':
		{
			g_camera_controller.MoveForward();
		}
		break;
		case 'S':
		{
			g_camera_controller.MoveBack();
		}
		break;
		case 'A':
		{
			g_camera_controller.MoveLeft();
		}
		break;
		case 'D':
		{
			g_camera_controller.MoveRight();
		}
		break;
		case 'Q':
		{
			g_camera_controller.MoveDown();
		}
		break;
		case 'E':
		{
			g_camera_controller.MoveUp();
		}
		break;
		}
	}
		return 0;
	case WM_MOUSEMOVE:
	{
		int Current_X = GET_X_LPARAM(lParam);
		int Current_Y = GET_Y_LPARAM(lParam);

		float CurrentNormalized_X = static_cast<float>(Current_X) / g_resolution_width;
		float CurrentNormalized_Y = static_cast<float>(Current_Y) / g_resolution_height;

		g_camera_controller.OnMouseMove(CurrentNormalized_X, CurrentNormalized_Y, (0 != (wParam & MK_RBUTTON)));
	}
		return 0;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
}

#else
#error Unknown Compiler
#endif