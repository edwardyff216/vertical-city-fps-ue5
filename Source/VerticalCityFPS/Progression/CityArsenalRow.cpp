#include "Progression/CityArsenalRow.h"
const FCityArsenalRow* FCityArsenalRow::Find(FName Kind)
{
    static TWeakObjectPtr<UDataTable> Table;
    if (!Table.IsValid()) Table=LoadObject<UDataTable>(nullptr,TEXT("/Game/VerticalCity/Data/DT_Arsenal.DT_Arsenal"));
    return Table.IsValid()?Table->FindRow<FCityArsenalRow>(Kind,TEXT("Arsenal"),false):nullptr;
}
