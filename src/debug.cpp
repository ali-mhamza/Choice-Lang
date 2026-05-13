#include "../include/debug.h"

bool DebugRange::operator==(const DebugRange& other) const
{
    return (this->bytesEqual(other) && this->locationEqual(other));
}

bool DebugRange::bytesEqual(const DebugRange& other) const
{
    return ((this->byteStart == other.byteStart)
            && (this->byteEnd == other.byteEnd));
}

bool DebugRange::locationEqual(const DebugRange& other) const
{
    return ((this->sourceStart == other.sourceStart)
        && (this->sourceEnd == other.sourceEnd));
}

bool DebugRange::bytesInside(const DebugRange& other) const
{
    if (this->bytesEqual(other)) return false;

    return ((this->byteStart >= other.byteStart)
            && (this->byteEnd <= other.byteEnd));
}

bool DebugRange::locationInside(const DebugRange& other) const
{
    if (this->locationEqual(other)) return false;

    return ((this->sourceStart >= other.sourceStart)
        && (this->sourceEnd <= other.sourceEnd));
}