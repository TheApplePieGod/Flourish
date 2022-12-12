#pragma once

struct Windows
{
    static void PollEvents(HWND window);
    static HWND CreateWindowAndGet(int width, int height);
};