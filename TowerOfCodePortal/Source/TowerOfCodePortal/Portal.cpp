// Fill out your copyright notice in the Description page of Project Settings.


#include "Portal.h"
#include "DrawDebugHelpers.h"

void PrintMatrix(FMatrix matrix)
{
    for (int i = 3; i >= 0; i--)
        GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString::Printf(TEXT("%.2f, %.2f, %.2f, %.2f"), matrix.M[i][0], matrix.M[i][1], matrix.M[i][2], matrix.M[i][3]));
}

void PrintMessage(FString string)
{
    GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, string);
}

void PrintVector(FVector vector)
{
    GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString::Printf(TEXT("%.2f, %.2f, %.2f"), vector.X, vector.Y, vector.Z));
}

// Sets default values
APortal::APortal()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
    bIsActive = true;
}

// Called when the game starts or when spawned
void APortal::BeginPlay()
{
	Super::BeginPlay();
    CreateRenderTarget();
    CreateSceneCapture();
}

void APortal::CreateRenderTarget()
{
    if (RenderTarget == nullptr)
    {
        // Create new RTT
        RenderTarget = NewObject<UTextureRenderTarget2D>(
            this,
            UTextureRenderTarget2D::StaticClass(),
            *FString("PortalRenderTarget")
            );
        check(RenderTarget);

        RenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA16f;
        RenderTarget->Filter = TextureFilter::TF_Bilinear;
        RenderTarget->SizeX = 1920;
        RenderTarget->SizeY = 1080;
        RenderTarget->ClearColor = FLinearColor::Black;
        RenderTarget->TargetGamma = 2.2f;
        RenderTarget->bNeedsTwoCopies = false;
        RenderTarget->AddressX = TextureAddress::TA_Clamp;
        RenderTarget->AddressY = TextureAddress::TA_Clamp;

        // Not needed since the texture is displayed on screen directly
        // in some engine versions this can even lead to crashes (notably 4.24/4.25)
        RenderTarget->bAutoGenerateMips = false;

        // This force the engine to create the render target 
        // with the parameters we defined just above
        RenderTarget->UpdateResource();
    }
}

void APortal::CreateSceneCapture()
{
    SceneCapture = NewObject<USceneCaptureComponent2D>(this, USceneCaptureComponent2D::StaticClass(), *FString("PortalSceneCapture"));

    SceneCapture->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::SnapToTargetIncludingScale);
    SceneCapture->RegisterComponent();
    SceneCapture->FOVAngle = 105;
    SceneCapture->bCaptureEveryFrame = false;
    SceneCapture->bCaptureOnMovement = false;
    SceneCapture->LODDistanceFactor = 1; //Force bigger LODs for faster computations
    SceneCapture->TextureTarget = RenderTarget;
    SceneCapture->bEnableClipPlane = true;
    SceneCapture->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDRNoAlpha;
}

// Called every frame
void APortal::Tick(float DeltaTime)
{
    //if (!IsActive())
    //    return;
	Super::Tick(DeltaTime);
}

void APortal::UpdateCapture()
{
    //DrawDebugPoint(GetWorld(), GetWorld()->GetFirstPlayerController()->PlayerCameraManager->GetCameraLocation(), 5, FColor::Yellow, false, 60);
    APlayerCameraManager* PlayerCamera = 
        GetWorld()->GetFirstPlayerController()->PlayerCameraManager;
    FVector CameraRelativeLocation = 
        PlayerCamera->GetCameraLocation() - GetActorLocation();

    FVector ConvertedCameraLocation = 
        ConvertVectorToOppositeSpace(CameraRelativeLocation);
    
    AController* Controller = 
        UGameplayStatics::GetPlayerController(GetWorld(), 0);
    FQuat ControllerQuat = 
        ConvertQuatToOppositeSpace(FQuat(Controller->GetControlRotation()));

    //DrawDebugPoint(GetWorld(), Link->GetActorLocation() + ConvertedCameraLocation, 5, FColor::Red, false, 60);
    FQuat ConvertedCameraQuat = ConvertQuatToOppositeSpace(FQuat(PlayerCamera->GetActorQuat()));
    FRotator ConvertedCameraRot = ConvertedCameraQuat.Rotator();
    //PrintVector(PlayerCamera->GetCameraRotation().Vector());
   // PrintVector(PlayerCamera->GetCameraLocation());
    //PrintVector(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0)->GetActorLocation());

    //PrintMessage(FString::SanitizeFloat(ConvertedCameraRot.Roll));
    //ConvertedCameraRot.Roll -= 180;

    SceneCapture->SetWorldLocation(Link->GetActorLocation() + ConvertedCameraLocation);
    SceneCapture->SetWorldRotation(ControllerQuat);

    SceneCapture->ClipPlaneNormal = Link->GetActorForwardVector();
    SceneCapture->ClipPlaneBase = Link->GetActorLocation();
    SceneCapture->CaptureScene();
    SetRTT(RenderTarget);
}

void APortal::SetLink(APortal* Target)
{
	Link = Target;
}

APortal* APortal::GetLink()
{
	return Link;
}

bool APortal::IsPointBehindPortal(FVector Point, FVector PortalLocation, FVector PortalNormal, float Offset)
{
	return FVector::DotProduct(PortalNormal, Point - PortalLocation) < Offset;
}

void APortal::TeleportActor(AActor* Target, FVector Offset)
{
    // Convert position and velocity
    FVector ActorLocationOnOppositeSpace = ConvertVectorToOppositeSpace(
        Target->GetActorLocation() - GetActorLocation());
    FVector VelocityOnOppositeSpace = ConvertVectorToOppositeSpace(
        Target->GetVelocity());
    FVector OffsetOnOppositeSpace = ConvertVectorToOppositeSpace(Offset);
    
    // Convert rotation
    FQuat QuatOnOppositeSpace = ConvertQuatToOppositeSpace(
        Target->GetActorQuat());
    
    // Set changes
    Target->SetActorRotation(QuatOnOppositeSpace);
    Target->SetActorLocation(
        Link->GetActorLocation() 
        + ActorLocationOnOppositeSpace 
        + OffsetOnOppositeSpace);

    ACharacter* Character = Cast<ACharacter>(Target);
    if (Character)
    {
        Character->GetMovementComponent()->Velocity = 
            VelocityOnOppositeSpace;

        AController* Controller = Character->GetController();

        FQuat ControllerQuat = 
            ConvertQuatToOppositeSpace(
                FQuat(Controller->GetControlRotation()));

        Character
            ->GetController()
            ->SetControlRotation(ControllerQuat.Rotator());
    }
}


FVector APortal::ConvertVectorToOppositeSpace(const FVector Point)
{
    const FVector PortalNormal = GetActorForwardVector();
    const FVector PortalRight = GetActorRightVector();
    const FVector PortalUp = GetActorUpVector();

    const FMatrix PortalBasis = FMatrix(
        PortalNormal, 
        PortalRight, 
        PortalUp, 
        FVector(0, 0, 0));
    const FMatrix PortalBasisInverse = PortalBasis.Inverse();
    const FMatrix ActorLocationMatrix = FMatrix(
        Point, 
        FPlane(0, 0, 0, 0), 
        FPlane(0, 0, 0, 0), 
        FPlane(0, 0, 0, 0));
    const FMatrix Component = ActorLocationMatrix * PortalBasisInverse;

    const FVector OppositeNormal = -(Link->GetActorForwardVector());
    const FVector OppositeRight = -(Link->GetActorRightVector());
    const FVector OppositeUp = Link->GetActorUpVector();
    const FMatrix OppositeBasis = FMatrix(
        OppositeNormal, 
        OppositeRight, 
        OppositeUp, 
        FVector(0, 0, 0));

    const FMatrix Result = Component * OppositeBasis;
    return FVector(Result.M[0][0], Result.M[0][1], Result.M[0][2]);
}

FQuat APortal::ConvertQuatToOppositeSpace(FQuat Quat)
{
    FTransform SourceTransform = GetActorTransform();
    FTransform TargetTransform = Link->GetActorTransform();


    //FQuat Mirror = FQuat(Link->GetActorForwardVector().X, Link->GetActorForwardVector().Y, Link->GetActorForwardVector().Z, 0);
    //FQuat LocalQuat = (Mirror * SourceTransform.GetRotation() * Mirror).Inverse() * Quat;
    //FQuat LocalQuat = SourceTransform.GetRotation().Inverse() * Quat;
    //FQuat OppositeQuat = TargetTransform.GetRotation() * LocalQuat;

    FQuat Diff = TargetTransform.GetRotation()
        * SourceTransform.GetRotation().Inverse();
    FQuat Result = Diff * Quat;

    // Turn 180 degrees
    FVector UpVector = TargetTransform.GetRotation().GetUpVector();
    FQuat Rotator = FQuat(UpVector.X, UpVector.Y, UpVector.Z, 0);

    return Rotator * Result;
}

void APortal::SetActive(bool NewInput)
{
    bIsActive = NewInput;
}

bool APortal::IsActive()
{
    return bIsActive;
}

void APortal::SetRTT_Implementation(UTexture* RenerTexture)
{
}





