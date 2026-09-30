#include "uniqueid.h"
#include <climits>

UniqueId::UniqueId()
{
    mId = 0;
}

IdType UniqueId::get()
{
    auto id = ++mId;
    if (id >= ULLONG_MAX)
        mId=1;

    return mId;
}
