// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/DrawFrustumComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TowerOfCodePortalCharacter.h"
#include "Portal.generated.h"


UCLASS()
class TOWEROFCODEPORTAL_API APortal : public AActor
{
	GENERATED_BODY()


public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		APortal* Link;

        USceneCaptureComponent2D* SceneCapture;

        UTextureRenderTarget2D* RenderTarget;

protected:
	UPROPERTY(BlueprintReadOnly)
		USceneComponent* PortalRootComponent;

private:
    bool bIsActive;
    int RecursionThreshold;

public:
	// Sets default values for this actor's properties
	APortal();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

    void CreateRenderTarget();

    void CreateSceneCapture();

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

    UFUNCTION(BlueprintCallable)
        void UpdateCapture();

	UFUNCTION(BlueprintCallable)
		void SetLink(APortal* Target);

	UFUNCTION(BlueprintPure)
		APortal* GetLink();

	UFUNCTION(BlueprintCallable)
		bool IsPointBehindPortal(FVector Point, FVector PortalLocation, FVector PortalNormal, float Offset);

	UFUNCTION(BlueprintCallable)
		void TeleportActor(AActor* Target, FVector Offset);

    UFUNCTION(BlueprintCallable)
        FVector ConvertVectorToOppositeSpace(FVector Point);

    UFUNCTION(BlueprintCallable)
        FQuat ConvertQuatToOppositeSpace(FQuat Quat);

    UFUNCTION(BlueprintCallable)
        void SetActive(bool NewInput);

    UFUNCTION(BlueprintCallable)
        bool IsActive();

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
        void SetRTT(UTexture* RenerTexture);

private:
    void UpdateCaptureRecursive(FVector CameraRelativeLocation, FQuat CameraQuat, int Depth);
    void HideActorsNotVisible();
};
