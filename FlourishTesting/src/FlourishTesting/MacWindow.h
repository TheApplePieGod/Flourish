#pragma once

struct MacOS
{
    static void PollEvents();
    static void* CreateWindowAndGetView(int width, int height);
};