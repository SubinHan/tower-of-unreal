// Copyright Epic Games, Inc. All Rights Reserved.

#include "TowerOfCodeThrowingCharacter.h"
#include "TowerOfCodeThrowingProjectile.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/InputSettings.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "MotionControllerComponent.h"
#include "XRMotionControllerBase.h" // for FXRMotionControllerBase::RightHandSourceId

DEFINE_LOG_CATEGORY_STATIC(LogFPChar, Warning, All);

//////////////////////////////////////////////////////////////////////////
// ATowerOfCodeThrowingCharacter

ATowerOfCodeThrowingCharacter::ATowerOfCodeThrowingCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Create a CameraComponent	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(-39.56f, 1.75f, 64.f)); // Position the camera
	FirstPersonCameraComponent->bUsePawnControlRotation = true;

	// Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetupAttachment(FirstPersonCameraComponent);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	Mesh1P->SetRelativeRotation(FRotator(1.9f, -19.19f, 5.2f));
	Mesh1P->SetRelativeLocation(FVector(-0.5f, -4.4f, -155.7f));

	// Create a gun mesh component
	FP_Gun = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FP_Gun"));
	FP_Gun->SetOnlyOwnerSee(false);			// otherwise won't be visible in the multiplayer
	FP_Gun->bCastDynamicShadow = false;
	FP_Gun->CastShadow = false;
	// FP_Gun->SetupAttachment(Mesh1P, TEXT("GripPoint"));
	FP_Gun->SetupAttachment(RootComponent);

	FP_MuzzleLocation = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzleLocation"));
	FP_MuzzleLocation->SetupAttachment(FP_Gun);
	FP_MuzzleLocation->SetRelativeLocation(FVector(0.2f, 48.4f, -10.6f));

	// Default offset from the character location for projectiles to spawn
	GunOffset = FVector(100.0f, 0.0f, 10.0f);

	// Note: The ProjectileClass and the skeletal mesh/anim blueprints for Mesh1P, FP_Gun, and VR_Gun 
	// are set in the derived blueprint asset named MyCharacter to avoid direct content references in C++.

	// Create VR Controllers.
	R_MotionController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("R_MotionController"));
	R_MotionController->MotionSource = FXRMotionControllerBase::RightHandSourceId;
	R_MotionController->SetupAttachment(RootComponent);
	L_MotionController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("L_MotionController"));
	L_MotionController->SetupAttachment(RootComponent);

	// Create a gun and attach it to the right-hand VR controller.
	// Create a gun mesh component
	VR_Gun = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("VR_Gun"));
	VR_Gun->SetOnlyOwnerSee(false);			// otherwise won't be visible in the multiplayer
	VR_Gun->bCastDynamicShadow = false;
	VR_Gun->CastShadow = false;
	VR_Gun->SetupAttachment(R_MotionController);
	VR_Gun->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

	VR_MuzzleLocation = CreateDefaultSubobject<USceneComponent>(TEXT("VR_MuzzleLocation"));
	VR_MuzzleLocation->SetupAttachment(VR_Gun);
	VR_MuzzleLocation->SetRelativeLocation(FVector(0.000004, 53.999992, 10.000000));
	VR_MuzzleLocation->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));		// Counteract the rotation of the VR gun model.


	 // beamcomponent
	BeamComp = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("BeamComp"));
	BeamComp->SetupAttachment(FP_Gun);
	BeamComp->bAutoActivate = false;

	// Uncomment the following line to turn motion controllers on by default:
	//bUsingMotionControllers = true;

	IsPredicting = false;
}

void ATowerOfCodeThrowingCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	//Attach gun mesh component to Skeleton, doing it here because the skeleton is not yet created in the constructor
	FP_Gun->AttachToComponent(Mesh1P, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true), TEXT("GripPoint"));

	// Show or hide the two versions of the gun based on whether or not we're using motion controllers.
	if (bUsingMotionControllers)
	{
		VR_Gun->SetHiddenInGame(false, true);
		Mesh1P->SetHiddenInGame(true, true);
	}
	else
	{
		VR_Gun->SetHiddenInGame(true, true);
		Mesh1P->SetHiddenInGame(false, true);
	}
}

//////////////////////////////////////////////////////////////////////////
// Input

void ATowerOfCodeThrowingCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// set up gameplay key bindings
	check(PlayerInputComponent);

	// Bind jump events
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	// Bind fire event
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &ATowerOfCodeThrowingCharacter::OnFire);

	// Bind predict event
	PlayerInputComponent->BindAction("Predict", IE_Pressed, this, &ATowerOfCodeThrowingCharacter::OnPredictPressed);
	PlayerInputComponent->BindAction("Predict", IE_Released, this, &ATowerOfCodeThrowingCharacter::OnPredictReleased);

	// Enable touchscreen input
	EnableTouchscreenMovement(PlayerInputComponent);

	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &ATowerOfCodeThrowingCharacter::OnResetVR);

	// Bind movement events
	PlayerInputComponent->BindAxis("MoveForward", this, &ATowerOfCodeThrowingCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ATowerOfCodeThrowingCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &ATowerOfCodeThrowingCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &ATowerOfCodeThrowingCharacter::LookUpAtRate);
}

void ATowerOfCodeThrowingCharacter::OnFire()
{
	// try and fire a projectile
	if (ProjectileClass != nullptr)
	{
		UWorld* const World = GetWorld();
		if (World != nullptr)
		{
			if (bUsingMotionControllers)
			{
				const FRotator SpawnRotation = VR_MuzzleLocation->GetComponentRotation();
				const FVector SpawnLocation = VR_MuzzleLocation->GetComponentLocation();
				World->SpawnActor<ATowerOfCodeThrowingProjectile>(ProjectileClass, SpawnLocation, SpawnRotation);
			}
			else
			{
				const FRotator SpawnRotation = GetControlRotation();
				// MuzzleOffset is in camera space, so transform it to world space before offsetting from the character location to find the final muzzle position
				const FVector SpawnLocation = ((FP_MuzzleLocation != nullptr) ? FP_MuzzleLocation->GetComponentLocation() : GetActorLocation()) + SpawnRotation.RotateVector(GunOffset);

				//Set Spawn Collision Handling Override
				FActorSpawnParameters ActorSpawnParams;
				ActorSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

				// spawn the projectile at the muzzle
				World->SpawnActor<ATowerOfCodeThrowingProjectile>(ProjectileClass, SpawnLocation, SpawnRotation, ActorSpawnParams);
			}
		}
	}

	// try and play the sound if specified
	if (FireSound != nullptr)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, GetActorLocation());
	}

	// try and play a firing animation if specified
	if (FireAnimation != nullptr)
	{
		// Get the animation object for the arms mesh
		UAnimInstance* AnimInstance = Mesh1P->GetAnimInstance();
		if (AnimInstance != nullptr)
		{
			AnimInstance->Montage_Play(FireAnimation, 1.f);
		}
	}
}


void ATowerOfCodeThrowingCharacter::DrawTrajectory(const FVector InitialLocation, const FVector InitialVelocity, const FVector Gravity, float Duration) 
{
	bool bObjectHit;
	FHitResult ObjectTraceHit(NoInit);
	FVector StartLocation = InitialLocation;
	FVector StartVelocity = InitialVelocity;
	FVector TraceStart;
	FVector TraceEnd;
	TraceStart = TraceEnd = CalculateProjectileLocationOnTime(StartLocation, StartVelocity, Gravity, 0.0f);
	UWorld const* const World = GetWorld();
	const float ProjectileRadius = ProjectileClass.GetDefaultObject()->GetSimpleCollisionRadius();
	FCollisionQueryParams QueryParams(NAME_None, false, NULL);
	FCollisionObjectQueryParams ObjQueryParams;
	const float MaxSimTime = 2.0f;
	const int MaxSimBounce = 2;
	const float SimFrequency = 1.e-2f;
	float SimTime = 0.f;
	int SimBounce = 0;
	float Friction = ProjectileClass.GetDefaultObject()->GetProjectileMovement()->Friction;
	float Bounciness = ProjectileClass.GetDefaultObject()->GetProjectileMovement()->Bounciness;
	FVector CurrentVelocity = StartVelocity;

	// to ignore the projectiles
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ProjectileClass.GetDefaultObject()->StaticClass(), FoundActors);
	QueryParams.AddIgnoredActors(FoundActors);

	//ClearBeams();
	DestroyTrajectory();

	while (SimTime < MaxSimTime) 
	{
		bObjectHit = World->SweepSingleByObjectType(ObjectTraceHit, TraceStart, TraceEnd,
			FQuat::Identity, ObjQueryParams, FCollisionShape::MakeSphere(ProjectileRadius), QueryParams);

		FVector StartTangent = CurrentVelocity;
		FVector EndTangent = StartVelocity + Gravity * SimTime;
		StartTangent.Normalize();
		EndTangent.Normalize();

		USplineMeshComponent* pSplineMesh = NewObject<USplineMeshComponent>(this, USplineMeshComponent::StaticClass());
		pSplineMesh->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		pSplineMesh->SetStartScale(FVector2D(3, 3));
		pSplineMesh->SetEndScale(FVector2D(3, 3));
		pSplineMesh->SetStaticMesh(MyMesh);
		pSplineMesh->SetStartAndEnd(TraceStart, StartTangent, TraceEnd, EndTangent);

		//AddNewBeam(TraceStart, TraceEnd);
		CurrentVelocity = StartVelocity + Gravity * SimTime;

		if (bObjectHit) 
		{
			SimBounce++;
			bObjectHit = false;


			UStaticMeshComponent* pHittedMesh = NewObject<UStaticMeshComponent>(this);
			pHittedMesh->SetStaticMesh(HittedMesh);
			FTransform Transform;
			Transform.SetLocation(ObjectTraceHit.Location);
			Transform.SetScale3D(FVector(1.f, 1.f, 1.f));
			//pHittedMesh->SetWorldLocation(ObjectTraceHit.Location);	
			pHittedMesh->SetWorldTransform(Transform);

			HittedMeshArray.Add(pHittedMesh);

			CurrentVelocity = StartVelocity + Gravity * (SimTime - SimFrequency * (1 - ObjectTraceHit.Time));

			FVector ComponentOfImpactNormal = ObjectTraceHit.ImpactNormal * FVector::DotProduct(ObjectTraceHit.ImpactNormal, CurrentVelocity);
			FVector FrictionVector = (CurrentVelocity - ComponentOfImpactNormal) * Friction;
			FVector BouncinessVector = ComponentOfImpactNormal * Bounciness;
			StartVelocity = CurrentVelocity - ComponentOfImpactNormal - BouncinessVector - FrictionVector;
			StartVelocity = GetReflectedVector(ObjectTraceHit.ImpactNormal, CurrentVelocity, Friction, Bounciness);
			StartLocation = TraceEnd = ObjectTraceHit.Location;

			SimTime = 0.0005f;
			TraceEnd = CalculateProjectileLocationOnTime(StartLocation, StartVelocity
				, Gravity, SimTime);
			CurrentVelocity = StartVelocity + Gravity * (SimTime - SimFrequency * (1 - ObjectTraceHit.Time));

			ProjectileClass.GetDefaultObject()->GetCollisionComp();

			if (SimBounce >= MaxSimBounce) {
				break;
			}
		}
		TraceStart = TraceEnd;
		SimTime += SimFrequency;
		TraceEnd = CalculateProjectileLocationOnTime(StartLocation, StartVelocity, Gravity, SimTime);

	}
	RegisterAllComponents();
}

void ATowerOfCodeThrowingCharacter::DestroyTrajectory()
{
	TArray<UActorComponent*> FoundSplines;
	FoundSplines = this->GetComponentsByClass(USplineMeshComponent::StaticClass());
	for (UActorComponent* It : FoundSplines) {
		It->UnregisterComponent();
		It->DestroyComponent();
	}

	for (UStaticMeshComponent* pHittedMesh : HittedMeshArray) {
		pHittedMesh->UnregisterComponent();
		pHittedMesh->DestroyComponent();
	}
}

void ATowerOfCodeThrowingCharacter::Tick(float DeltaSeconds) {
	if (IsPredicting) {
		UProjectileMovementComponent* Movement = ProjectileClass.GetDefaultObject()->GetProjectileMovement();

		const FRotator SpawnRotation = GetControlRotation();
		FQuat Quat = FQuat(SpawnRotation);

		FVector ForwardVector = Quat.GetForwardVector();
		const FVector LocationVector = ((FP_MuzzleLocation != nullptr) ? FP_MuzzleLocation->GetComponentLocation() : GetActorLocation()) + SpawnRotation.RotateVector(GunOffset);

		float Speed = Movement->InitialSpeed;
		const float Gravity = UPhysicsSettings::Get()->DefaultGravityZ;
		FVector InitialVelocity = ForwardVector * Speed;
		ClearBeams();
		DrawTrajectory(LocationVector, InitialVelocity, FVector(0, 0, Gravity), DeltaSeconds + .01f);
	}
}

void ATowerOfCodeThrowingCharacter::OnResetVR()
{
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}

void ATowerOfCodeThrowingCharacter::OnPredictPressed()
{
	GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::White, TEXT("Predict Pressed"));
	IsPredicting = true;

	//UGameplayStatics::PredictProjectilePath();

}

void ATowerOfCodeThrowingCharacter::OnPredictReleased()
{
	GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::White, TEXT("Predict Released"));
	IsPredicting = false;
	DestroyTrajectory();
	//ClearBeams();
}

FVector ATowerOfCodeThrowingCharacter::CalculateProjectileLocationOnTime(const FVector StartLocation,
	const FVector InitialVelocity, const FVector Gravity, const float Time)
{
	FVector ResultVector;
	ResultVector = StartLocation + InitialVelocity * Time + 0.5f * Gravity * Time * Time;
	return ResultVector;
}

FVector ATowerOfCodeThrowingCharacter::GetReflectedVector(const FVector ImpactNormal, const FVector Velocity, float Friction, float Bounciness)
{
	FVector ComponentAlongImpactNormal = ImpactNormal * FVector::DotProduct(ImpactNormal, Velocity);
	FVector FrictionVector = (Velocity - ComponentAlongImpactNormal) * Friction;
	FVector BouncinessVector = ComponentAlongImpactNormal * Bounciness;

	FVector ResultVector = Velocity - ComponentAlongImpactNormal - BouncinessVector - FrictionVector;

	return ResultVector;
}


void ATowerOfCodeThrowingCharacter::BeginTouch(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	if (TouchItem.bIsPressed == true)
	{
		return;
	}
	if ((FingerIndex == TouchItem.FingerIndex) && (TouchItem.bMoved == false))
	{
		OnFire();
	}
	TouchItem.bIsPressed = true;
	TouchItem.FingerIndex = FingerIndex;
	TouchItem.Location = Location;
	TouchItem.bMoved = false;
}

void ATowerOfCodeThrowingCharacter::EndTouch(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	if (TouchItem.bIsPressed == false)
	{
		return;
	}
	TouchItem.bIsPressed = false;
}

void ATowerOfCodeThrowingCharacter::ClearBeams() {
	for (UParticleSystemComponent* beam : BeamArray) {
		beam->DestroyComponent();
	}
	BeamArray.Empty();
}

void ATowerOfCodeThrowingCharacter::AddNewBeam(FVector Point1, FVector Point2)
{

	BeamComp = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), BeamFX, Point1, FRotator::ZeroRotator, true);
	BeamArray.Add(BeamComp);

	BeamComp->SetBeamSourcePoint(0, Point1, 0);
	BeamComp->SetBeamTargetPoint(0, Point2, 0);
}

//Commenting this section out to be consistent with FPS BP template.
//This allows the user to turn without using the right virtual joystick

//void ATowerOfCodeThrowingCharacter::TouchUpdate(const ETouchIndex::Type FingerIndex, const FVector Location)
//{
//	if ((TouchItem.bIsPressed == true) && (TouchItem.FingerIndex == FingerIndex))
//	{
//		if (TouchItem.bIsPressed)
//		{
//			if (GetWorld() != nullptr)
//			{
//				UGameViewportClient* ViewportClient = GetWorld()->GetGameViewport();
//				if (ViewportClient != nullptr)
//				{
//					FVector MoveDelta = Location - TouchItem.Location;
//					FVector2D ScreenSize;
//					ViewportClient->GetViewportSize(ScreenSize);
//					FVector2D ScaledDelta = FVector2D(MoveDelta.X, MoveDelta.Y) / ScreenSize;
//					if (FMath::Abs(ScaledDelta.X) >= 4.0 / ScreenSize.X)
//					{
//						TouchItem.bMoved = true;
//						float Value = ScaledDelta.X * BaseTurnRate;
//						AddControllerYawInput(Value);
//					}
//					if (FMath::Abs(ScaledDelta.Y) >= 4.0 / ScreenSize.Y)
//					{
//						TouchItem.bMoved = true;
//						float Value = ScaledDelta.Y * BaseTurnRate;
//						AddControllerPitchInput(Value);
//					}
//					TouchItem.Location = Location;
//				}
//				TouchItem.Location = Location;
//			}
//		}
//	}
//}

void ATowerOfCodeThrowingCharacter::MoveForward(float Value)
{
	if (Value != 0.0f)
	{
		// add movement in that direction
		AddMovementInput(GetActorForwardVector(), Value);
	}
}

void ATowerOfCodeThrowingCharacter::MoveRight(float Value)
{
	if (Value != 0.0f)
	{
		// add movement in that direction
		AddMovementInput(GetActorRightVector(), Value);
	}
}

void ATowerOfCodeThrowingCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void ATowerOfCodeThrowingCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

bool ATowerOfCodeThrowingCharacter::EnableTouchscreenMovement(class UInputComponent* PlayerInputComponent)
{
	if (FPlatformMisc::SupportsTouchInput() || GetDefault<UInputSettings>()->bUseMouseForTouch)
	{
		PlayerInputComponent->BindTouch(EInputEvent::IE_Pressed, this, &ATowerOfCodeThrowingCharacter::BeginTouch);
		PlayerInputComponent->BindTouch(EInputEvent::IE_Released, this, &ATowerOfCodeThrowingCharacter::EndTouch);

		//Commenting this out to be more consistent with FPS BP template.
		//PlayerInputComponent->BindTouch(EInputEvent::IE_Repeat, this, &ATowerOfCodeThrowingCharacter::TouchUpdate);
		return true;
	}

	return false;
}
