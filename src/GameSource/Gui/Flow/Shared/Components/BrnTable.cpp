#include "GameSource/Gui/Flow/Shared/Components/BrnTable.h"

// BrnGui::TableDataSet - the row-data getter for the GUI table component.
// See BrnTable.h for the recovered layout and the X360 address.

namespace BrnGui
{
    // @0x824E47D8 - bounds-checked row-data fetch, returned by value (X360 lwzx of the
    // 4-byte element at maRowData[liRow]). The dual bound (liRow < 0 || liRow >= count)
    // is faithful: the X360 reads the count with lbz+extsb (a signed byte) and tests both
    // ends, firing "TableDataSet::GetRowData() invalid index specified" (BrnTable.h:477).
    u32 TableDataSet::GetRowData(s32 liRow) const
    {
        CGS_ASSERT(liRow >= 0 && liRow < miRowCount,
                   "TableDataSet::GetRowData() invalid index specified");
        return maRowData[liRow];
    }
}
