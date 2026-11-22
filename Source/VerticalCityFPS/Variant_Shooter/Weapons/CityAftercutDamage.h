#pragma once

#include "GameFramework/DamageType.h"
#include "CityAftercutDamage.generated.h"

// The delayed katana wound presents its own per-target impact, including on a killing blow.
UCLASS()
class UCityAftercutDamage : public UDamageType
{
    GENERATED_BODY()
};
