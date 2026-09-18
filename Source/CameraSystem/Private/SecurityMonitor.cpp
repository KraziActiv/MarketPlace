#include "SecurityMonitor.h"
#include "SecurityCamera.h"
#include "CameraSystemComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
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
	DOREPLIFETIME(ASecurityMonitor, PlacedByPlayer);
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
	if (MeshToUse && MonitorMesh) MonitorMesh->SetStaticMesh(MeshToUse);
}

bool ASecurityMonitor::CanPlayerView(APawn* Viewer) const
{
	if (!Viewer) return false;
	APlayerState* ViewerPlayerState = Viewer->GetPlayerState();
	if (PlacedByPlayer && ViewerPlayerState == PlacedByPlayer) return true;

	UCameraSystemComponent* ViewerComponent = Viewer->FindComponentByClass<UCameraSystemComponent>();
	return TeamID != 0 && ViewerComponent && ViewerComponent->TeamID == TeamID;
}

void ASecurityMonitor::RefreshTeamCameras()
{
	TeamCameras.Empty();
	if (!GetWorld()) return;

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASecurityCamera::StaticClass(), FoundActors);
	for (AActor* Actor : FoundActors)
	{
		if (ASecurityCamera* Camera = Cast<ASecurityCamera>(Actor))
		{
			if (Camera->TeamID == TeamID) TeamCameras.Add(Camera);
		}
	}

	if (TeamCameras.Num() > 0)
	{
		CurrentCameraIndex = FMath::Clamp(CurrentCameraIndex, 0, TeamCameras.Num() - 1);
	}
	else
	{
		CurrentCameraIndex = 0;
	}
	UpdateActiveFeed();
}

void ASecurityMonitor::Server_CycleCameraFeed_Implementation(bool bNext)
{
	APawn* Viewer = nullptr;
	if (APlayerController* Controller = GetNetOwningPlayer()) Viewer = Controller->GetPawn();
	if (!CanPlayerView(Viewer)) return;
	CycleCameraFeed(bNext);
}

void ASecurityMonitor::CycleCameraFeed(bool bNext)
{
	RefreshTeamCameras();
	if (TeamCameras.Num() == 0) return;

	CurrentCameraIndex = bNext
		? (CurrentCameraIndex + 1) % TeamCameras.Num()
		: (CurrentCameraIndex - 1 + TeamCameras.Num()) % TeamCameras.Num();
	UpdateActiveFeed();
}

void ASecurityMonitor::OnRep_CurrentCameraIndex()
{
	UpdateActiveFeed();
}

void ASecurityMonitor::UpdateActiveFeed()
{
	if (!MonitorMesh || !GetWorld()) return;
	APawn* LocalPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!CanPlayerView(LocalPawn) || !TeamCameras.IsValidIndex(CurrentCameraIndex))
	{
		if (UMaterialInstanceDynamic* DynMat = MonitorMesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			DynMat->SetTextureParameterValue(FName("CameraFeed"), nullptr);
		}
		return;
	}

	ASecurityCamera* ActiveCamera = TeamCameras[CurrentCameraIndex];
	if (!ActiveCamera || ActiveCamera->TeamID != TeamID) return;
	ActiveCamera->SetCaptureActive(true);
	if (UTextureRenderTarget2D* RT = ActiveCamera->GetOrCreateLocalRenderTarget())
	{
		if (UMaterialInstanceDynamic* DynMat = MonitorMesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			DynMat->SetTextureParameterValue(FName("CameraFeed"), Cast<UTexture>(RT));
		}
	}
}
