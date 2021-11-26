// Copyright Epic Games, Inc. All Rights Reserved.

#include "TowerOfCodeThrowingGameMode.h"
#include "TowerOfCodeThrowingHUD.h"
#include "TowerOfCodeThrowingCharacter.h"
#include "UObject/ConstructorHelpers.h"

ATowerOfCodeThrowingGameMode::ATowerOfCodeThrowingGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPersonCPP/Blueprints/FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

	// use our custom HUD class
	HUDClass = ATowerOfCodeThrowingHUD::StaticClass();
}
