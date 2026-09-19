#include "SecurityCamera.h"
#include "SecurityMonitor.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"

ASecurityCamera::ASecurityCamera()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
	SetRootComponent(RootSceneComponent);

	CameraBaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CameraBaseMesh"));
	CameraBaseMesh->SetRelativeRotation(FRotator(-90, 0, 0));
	CameraBaseMesh->SetupAttachment(RootSceneComponent);

	CameraMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CameraMesh"));
	CameraMesh->SetupAttachment(CameraBaseMesh);

	SceneCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SceneCapture"));
	SceneCapture->SetupAttachment(CameraMesh);
	SceneCapture->SetRelativeLocation(FVector(14, 0, 21));
	
	// Disable capture by default to prevent performance drops on untracked cameras
	SceneCapture->bCaptureEveryFrame = false;
	SceneCapture->bCaptureOnMovement = false;
}

void ASecurityCamera::BeginPlay()
{
	Super::BeginPlay();

	FRotator InitialRotation = FRotator(DefaultPitch, 0.0f, 0.0f);
	CameraMesh->SetRelativeRotation(InitialRotation);
}

void ASecurityCamera::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bIsScanning) return;

	// Calculate Yaw movement
	float YawDelta = SweepSpeed * DeltaTime * (bSweepingRight ? 1.0f : -1.0f);
	CurrentScanYaw += YawDelta;

	// Reverse direction at boundaries
	if (FMath::Abs(CurrentScanYaw) >= MaxYawAngle)
	{
		CurrentScanYaw = FMath::Clamp(CurrentScanYaw, -MaxYawAngle, MaxYawAngle);
		bSweepingRight = !bSweepingRight;
	}

	// Apply updated rotation (Pitch remains constant at DefaultPitch)
	FRotator NewArmRotation = FRotator(DefaultPitch, CurrentScanYaw, 0.0f);
	CameraMesh->SetRelativeRotation(NewArmRotation);
}

void ASecurityCamera::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASecurityCamera, TeamID);
}

UTextureRenderTarget2D* ASecurityCamera::GetOrCreateLocalRenderTarget()
{
	if (!LocalRenderTarget)
	{
		LocalRenderTarget = NewObject<UTextureRenderTarget2D>(this);
		LocalRenderTarget->InitCustomFormat(
			1024,
			552,
			PF_B8G8R8A8,
			false
		);
		LocalRenderTarget->ClearColor = FLinearColor::Black;
		LocalRenderTarget->UpdateResourceImmediate(true);

		SceneCapture->TextureTarget = LocalRenderTarget;
	}
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Camera render target size: %dx%d"),
		LocalRenderTarget->SizeX,
		LocalRenderTarget->SizeY
	);

	return LocalRenderTarget;
}

void ASecurityCamera::OnRep_TeamID()
{
	// Refresh all monitors in the world when a camera receives its replicated TeamID
	TArray<AActor*> FoundMonitors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASecurityMonitor::StaticClass(), FoundMonitors);
	for (AActor* Actor : FoundMonitors)
	{
		if (ASecurityMonitor* Monitor = Cast<ASecurityMonitor>(Actor))
		{
			Monitor->RefreshTeamCameras();
		}
	}
}

void ASecurityCamera::SetCaptureActive(bool bActive)
{
	if (SceneCapture)
	{
		if (bActive)
		{
			GetOrCreateLocalRenderTarget();
			SceneCapture->bCaptureEveryFrame = true;
		}
		else
		{
			SceneCapture->bCaptureEveryFrame = false;
		}
	}
}