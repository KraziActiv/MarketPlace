#include "SecurityMonitor.h"
#include "SecurityCamera.h"
#include "CameraSystemComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Controller.h"
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
	if (!Viewer)
	{
		UE_LOG(LogTemp, Warning, TEXT("CameraSystem: Viewer is null"));
		return false;
	}

	APlayerState* ViewerPlayerState = Viewer->GetPlayerState();
	UCameraSystemComponent* ViewerComponent =
		Viewer->FindComponentByClass<UCameraSystemComponent>();

	const int32 ViewerTeamID = ViewerComponent ? ViewerComponent->TeamID : -1;
	const bool bIsPlacer = PlacedByPlayer && ViewerPlayerState == PlacedByPlayer;
	const bool bIsTeammate = TeamID != 0 && ViewerTeamID == TeamID;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("CameraSystem Access Check | MonitorTeam=%d ViewerTeam=%d IsPlacer=%s IsTeammate=%s"),
		TeamID,
		ViewerTeamID,
		bIsPlacer ? TEXT("true") : TEXT("false"),
		bIsTeammate ? TEXT("true") : TEXT("false")
	);

	return bIsPlacer || bIsTeammate;
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
	// A monitor spawned by a pawn is owned by that pawn. Resolve its controller
	// through the owning pawn instead of casting GetNetOwningPlayer() to a controller.
	APawn* Viewer = nullptr;
	if (APawn* OwningPawn = Cast<APawn>(GetOwner()))
	{
		Viewer = OwningPawn;
	}
	else if (AController* OwningController = Cast<AController>(GetOwner()))
	{
		Viewer = OwningController->GetPawn();
	}

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
