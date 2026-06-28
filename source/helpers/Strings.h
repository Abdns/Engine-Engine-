#ifndef STRINGS_H
#define STRINGS_H

#include "Types.h"

internal bool32 StringsAreEqual(const char *a, const char *b)
{
    while (*a && (*a == *b))
    {
        ++a;
        ++b;
    }
    return *a == *b;
}

// Дописать src в dest начиная с позиции at, не вылезая за destSize, с нулём в конце.
// Возвращает новую позицию — удобно цепочкой: at = Append(...); at = Append(...);
internal uint32 AppendString(char *dest, uint32 destSize, uint32 at, const char *src)
{
    while (*src && (at + 1) < destSize)
    {
        dest[at++] = *src++;
    }
    if (at < destSize)
    {
        dest[at] = 0;
    }
    return at;
}

#endif
