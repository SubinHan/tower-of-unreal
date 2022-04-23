// Copyright Epic Games, Inc. All Rights Reserved.

#include "TowerOfCodePortalGameMode.h"
#include "TowerOfCodePortalHUD.h"
#include "TowerOfCodePortalCharacter.h"
#include "UObject/ConstructorHelpers.h"

ATowerOfCodePortalGameMode::ATowerOfCodePortalGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPersonCPP/Blueprints/FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

	// use our custom HUD class
	HUDClass = ATowerOfCodePortalHUD::StaticClass();
}
