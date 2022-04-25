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
    RecursionThreshold = 5;
}

// Called when the game starts or when spawned
void APortal::BeginPlay()
{
	Super::BeginPlay();
    CreateRenderTarget();
    CreateSceneCapture();
    SetRTT(RenderTarget);
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
        RenderTarget->SizeX = FMath::Clamp(int(1920 / 1.7), 128, 1920);
        RenderTarget->SizeY = FMath::Clamp(int(1080 / 1.7), 128, 1920);
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
    SceneCapture->bCaptureEveryFrame = true;
    SceneCapture->bCaptureOnMovement = true;
    SceneCapture->CompositeMode = ESceneCaptureCompositeMode::SCCM_Composite;
    SceneCapture->TextureTarget = RenderTarget;
    SceneCapture->bEnableClipPlane = true;
    SceneCapture->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDRNoAlpha;


    //Setup Post-Process of SceneCapture (optimization : disable Motion Blur, etc)
    FPostProcessSettings CaptureSettings;

    CaptureSettings.bOverride_AmbientOcclusionQuality = true;
    CaptureSettings.bOverride_MotionBlurAmount = true;
    CaptureSettings.bOverride_SceneFringeIntensity = true;
    CaptureSettings.bOverride_GrainIntensity = true;
    CaptureSettings.bOverride_ScreenSpaceReflectionQuality = true;

    CaptureSettings.AmbientOcclusionQuality = 0.0f; //0=lowest quality..100=maximum quality
    CaptureSettings.MotionBlurAmount = 0.0f; //0 = disabled
    CaptureSettings.SceneFringeIntensity = 0.0f; //0 = disabled
    CaptureSettings.GrainIntensity = 0.0f; //0 = disabled
    CaptureSettings.ScreenSpaceReflectionQuality = 0.0f; //0 = disabled

    CaptureSettings.bOverride_ScreenPercentage = true;
    CaptureSettings.ScreenPercentage = 100.0f;

    SceneCapture->PostProcessSettings = CaptureSettings;

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
    APlayerCameraManager* PlayerCamera = 
        GetWorld()->GetFirstPlayerController()->PlayerCameraManager;
    FVector CameraRelativeLocation = 
        PlayerCamera->GetCameraLocation() - GetActorLocation();

    if (FVector::DotProduct(
            CameraRelativeLocation, 
            PlayerCamera->GetActorForwardVector()
        ) > 0)
        return;

    SceneCapture->ClipPlaneNormal = 
        Link->GetActorForwardVector();
    SceneCapture->ClipPlaneBase = 
        Link->GetActorLocation();

    UpdateCaptureRecursive(
        CameraRelativeLocation, 
        FQuat(PlayerCamera->GetActorQuat()), 
        1
    );

    //SetRTT(RenderTarget);
}

void APortal::HideActorsNotVisible()
{
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), AllActors);

    for (AActor* Actor : AllActors)
    {
        FVector Origin;
        FVector Extent;
        Actor->GetActorBounds(false, Origin, Extent);

        FVector Distance = Origin - Link->GetActorLocation();

        if (Distance.Size() > Extent.Size() && FVector::DotProduct(Link->GetActorForwardVector(), Distance) < 0.f)
        {
            SceneCapture->HideActorComponents(Actor, true);
        }
    }
}

void APortal::UpdateCaptureRecursive(FVector CameraRelativeLocation, FQuat CameraQuat, int Depth)
{
    if (Depth > RecursionThreshold)
        return;

    FVector ConvertedCameraLocation =
        ConvertVectorToOppositeSpace(CameraRelativeLocation);

    FQuat ConvertedCameraQuat =
        ConvertQuatToOppositeSpace(CameraQuat);

    UpdateCaptureRecursive(
        Link->GetActorLocation() + ConvertedCameraLocation - GetActorLocation(), 
        ConvertedCameraQuat, 
        Depth + 1
    );
    SceneCapture->SetWorldLocation(Link->GetActorLocation() + ConvertedCameraLocation);
    SceneCapture->SetWorldRotation(ConvertedCameraQuat);
    HideActorsNotVisible();
    SceneCapture->CaptureScene();
    SceneCapture->ClearHiddenComponents();
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





