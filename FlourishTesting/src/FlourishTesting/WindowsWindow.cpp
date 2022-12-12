#include "flpch.h"
#include "WindowsWindow.h"

void Windows::PollEvents(HWND window)
{
    MSG msg;
    while (PeekMessage(&msg, window, 0, 0, PM_REMOVE))
    {
       TranslateMessage(&msg);
       DispatchMessage(&msg);
    }
}

HWND Windows::CreateWindowAndGet(int width, int height)
{
    HINSTANCE instance = GetModuleHandle(NULL);
    WNDCLASS wc{};
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = "Window";
    RegisterClass(&wc);
    HWND hwnd = CreateWindow(
        "Window",
        "Flourish",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        width, height,
        NULL,
        NULL,
        instance,
        NULL
    );
    ShowWindow(hwnd, SW_SHOW);
    return hwnd;
}