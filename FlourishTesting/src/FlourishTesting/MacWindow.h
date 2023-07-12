#pragma once

struct MacOS
{
    static void PollEvents();
    static void* CreateWindowAndGetLayer(int width, int height);
};
