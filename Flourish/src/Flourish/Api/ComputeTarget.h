#pragma once

namespace Flourish
{
    class ComputeTarget 
    {
    public:
        ComputeTarget() = default;

    public:
        // TS
        static std::shared_ptr<ComputeTarget> Create();
    };
}