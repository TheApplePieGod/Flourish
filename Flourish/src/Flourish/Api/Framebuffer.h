#pragma once

namespace Flourish
{
    enum class MsaaSampleCount
    {
        None = 1,
        Two, Four, Eight, Sixteen, Thirtytwo, Sixtyfour,
        Max = Sixtyfour
    };

    enum class SubpassAttachmentType
    {
        None = 0,
        Color, Depth
    };

    struct SubpassAttachment
    {
        SubpassAttachmentType Type;
        u32 AttachmentIndex;
    };
}