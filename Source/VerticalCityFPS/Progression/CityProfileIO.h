#pragma once
#include "Progression/CityProgression.h"

namespace CityProfileIO
{
    bool Read(const FString& File, FCityProfile& Profile);
    bool Write(const FString& File, const FCityProfile& Profile, FString& Error);
}

