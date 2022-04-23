// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "TowerOfCodePortalHUD.generated.h"

UCLASS()
class ATowerOfCodePortalHUD : public AHUD
{
	GENERATED_BODY()

public:
	ATowerOfCodePortalHUD();

	/** Primary draw call for the HUD */
	virtual void DrawHUD() override;

private:
	/** Crosshair asset pointer */
	class UTexture2D* CrosshairTex;

};

