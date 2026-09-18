#include "SecurityMonitor.h"
#include "SecurityCamera.h"
#include "CameraSystemComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

ASecurityMonitor::ASecurityMonitor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
	SetRootComponent(RootSceneComponent);

	MonitorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MonitorMesh"));
	MonitorMesh->SetupAttachment(RootSceneComponent);
}

void ASecurityMonitor::BeginPlay()
{
	Super::BeginPlay();

	OnRep_IsHorizontalSurface();
	RefreshTeamCameras();
}

void ASecurityMonitor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASecurityMonitor, TeamID);
	DOREPLIFETIME(ASecurityMonitor, CurrentCameraIndex);
	DOREPLIFETIME(ASecurityMonitor, bIsHorizontalSurface);
}

void ASecurityMonitor::OnRep_TeamID()
{
	RefreshTeamCameras();
}

void ASecurityMonitor::SetSurfaceType(bool bHorizontal)
{
	bIsHorizontalSurface = bHorizontal;
	OnRep_IsHorizontalSurface();
}

void ASecurityMonitor::OnRep_IsHorizontalSurface()
{
	UStaticMesh* MeshToUse = bIsHorizontalSurface ? DesktopStaticMesh : WallStaticMesh;
	if (MeshToUse && MonitorMesh)
	{
		MonitorMesh->SetStaticMesh(MeshToUse);
	}
}

void ASecurityMonitor::RefreshTeamCameras()
{
	TeamCameras.Empty();

	if (!GetWorld()) return;

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASecurityCamera::StaticClass(), FoundActors);

	// Only collect cameras that match THIS monitor's TeamID
	for (AActor* Actor : FoundActors)
	{
		ASecurityCamera* Cam = Cast<ASecurityCamera>(Actor);
		if (Cam && Cam->TeamID == TeamID)
		{
			TeamCameras.Add(Cam);
		}
	}

	UpdateActiveFeed();
}

void ASecurityMonitor::Server_CycleCameraFeed_Implementation(bool bNext)
{
	RefreshTeamCameras();

	if (TeamCameras.Num() == 0) return;

	if (bNext)
	{
		CurrentCameraIndex = (CurrentCameraIndex + 1) % TeamCameras.Num();
	}
	else
	{
		CurrentCameraIndex = (CurrentCameraIndex - 1 + TeamCameras.Num()) % TeamCameras.Num();
	}

	UpdateActiveFeed();
}

void ASecurityMonitor::OnRep_CurrentCameraIndex()
{
	UpdateActiveFeed();
}

void ASecurityMonitor::UpdateActiveFeed()
{
	if (!MonitorMesh) return;

	// Fetch local player's camera component to verify authorization
	APawn* LocalPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	UCameraSystemComponent* LocalCamComp = LocalPawn ? LocalPawn->FindComponentByClass<UCameraSystemComponent>() : nullptr;

	bool bIsTeammate = (LocalCamComp && LocalCamComp->TeamID == TeamID);

	// If player is NOT on the monitor's team, or no cameras exist for this team
	if (!bIsTeammate || !TeamCameras.IsValidIndex(CurrentCameraIndex))
	{
		// Blank out screen material parameter so enemy players see static/black
		UMaterialInstanceDynamic* DynMat = MonitorMesh->CreateAndSetMaterialInstanceDynamic(0);
		if (DynMat)
		{
			DynMat->SetTextureParameterValue(FName("CameraFeed"), nullptr);
		}
		return;
	}

	ASecurityCamera* ActiveCamera = TeamCameras[CurrentCameraIndex];
	if (ActiveCamera && ActiveCamera->TeamID == TeamID)
	{
		// Enable capture locally for teammate viewer
		ActiveCamera->SetCaptureActive(true);
		UTextureRenderTarget2D* RT = ActiveCamera->GetOrCreateLocalRenderTarget();

		if (RT)
		{
			UMaterialInstanceDynamic* DynMat = MonitorMesh->CreateAndSetMaterialInstanceDynamic(0);
			if (DynMat)
			{
				DynMat->SetTextureParameterValue(FName("CameraFeed"), Cast<UTexture>(RT));
			}
		}
	}
}