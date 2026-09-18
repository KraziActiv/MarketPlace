#include "CameraSystemComponent.h"
#include "SecurityCamera.h"
#include "SecurityMonitor.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

UCameraSystemComponent::UCameraSystemComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UCameraSystemComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner() && GetOwner()->HasAuthority() && TeamID == 0)
	{
		// Preserve the existing private-player behavior until the host game assigns a team.
		APawn* OwnerPawn = Cast<APawn>(GetOwner());
		if (OwnerPawn && OwnerPawn->GetController())
		{
			TeamID = OwnerPawn->GetController()->GetUniqueID() + 1;
		}
		else
		{
			TeamID = GetOwner()->GetUniqueID() + 1;
		}
	}
}

void UCameraSystemComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UCameraSystemComponent, TeamID);
}

int32 UCameraSystemComponent::GetPlayerTeamID_Implementation() const
{
	return TeamID;
}

void UCameraSystemComponent::SetTeamID(int32 NewTeamID)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || TeamID == NewTeamID)
	{
		return;
	}

	TeamID = NewTeamID;
	RefreshOwnedSecurityActorsTeam();
}

void UCameraSystemComponent::RefreshOwnedSecurityActorsTeam()
{
	if (!GetWorld()) return;

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	APlayerState* OwnerPlayerState = OwnerPawn ? OwnerPawn->GetPlayerState() : nullptr;
	if (!OwnerPlayerState) return;

	TArray<AActor*> FoundCameras;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASecurityCamera::StaticClass(), FoundCameras);
	for (AActor* Actor : FoundCameras)
	{
		if (ASecurityCamera* Camera = Cast<ASecurityCamera>(Actor))
		{
			if (Camera->PlacedByPlayer == OwnerPlayerState)
			{
				Camera->TeamID = TeamID;
				Camera->OnRep_TeamID();
			}
		}
	}

	TArray<AActor*> FoundMonitors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASecurityMonitor::StaticClass(), FoundMonitors);
	for (AActor* Actor : FoundMonitors)
	{
		if (ASecurityMonitor* Monitor = Cast<ASecurityMonitor>(Actor))
		{
			if (Monitor->PlacedByPlayer == OwnerPlayerState)
			{
				Monitor->TeamID = TeamID;
				Monitor->RefreshTeamCameras();
			}
		}
	}
}

void UCameraSystemComponent::TryPlaceCamera()
{
	AActor* Owner = GetOwner();
	if (!Owner || !GetWorld()) return;

	FVector Start;
	FRotator Rotation;
	Owner->GetActorEyesViewPoint(Start, Rotation);
	FHitResult HitResult;
	FCollisionQueryParams TraceParams;
	TraceParams.AddIgnoredActor(Owner);

	if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, Start + Rotation.Vector() * PlacementTraceDistance, ECC_Visibility, TraceParams) && HitResult.bBlockingHit)
	{
		const FRotator SpawnRotation = FRotationMatrix::MakeFromX(HitResult.ImpactNormal).Rotator();
		Server_SpawnCamera(FTransform(SpawnRotation, HitResult.ImpactPoint));
	}
}

void UCameraSystemComponent::TryPlaceMonitor()
{
	AActor* Owner = GetOwner();
	if (!Owner || !GetWorld()) return;

	FVector Start;
	FRotator Rotation;
	Owner->GetActorEyesViewPoint(Start, Rotation);
	FHitResult HitResult;
	FCollisionQueryParams TraceParams;
	TraceParams.AddIgnoredActor(Owner);

	if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, Start + Rotation.Vector() * PlacementTraceDistance, ECC_Visibility, TraceParams) && HitResult.bBlockingHit)
	{
		const bool bIsHorizontalSurface = FMath::IsNearlyEqual(FMath::Abs(HitResult.ImpactNormal.Z), 1.0f, 0.1f);
		FRotator SpawnRotation;
		if (bIsHorizontalSurface)
		{
			SpawnRotation = (Start - HitResult.ImpactPoint).GetSafeNormal2D().Rotation();
		}
		else
		{
			SpawnRotation = FRotationMatrix::MakeFromX(HitResult.ImpactNormal).Rotator();
		}
		Server_SpawnMonitor(FTransform(SpawnRotation, HitResult.ImpactPoint), bIsHorizontalSurface);
	}
}

void UCameraSystemComponent::TryCycleMonitor(ASecurityMonitor* Monitor, bool bNext)
{
	if (!Monitor || !GetOwner()) return;
	if (GetOwner()->HasAuthority())
	{
		Server_RequestCycleMonitor_Implementation(Monitor, bNext);
	}
	else
	{
		Server_RequestCycleMonitor(Monitor, bNext);
	}
}

void UCameraSystemComponent::Server_RequestCycleMonitor_Implementation(ASecurityMonitor* Monitor, bool bNext)
{
	APawn* Viewer = Cast<APawn>(GetOwner());
	if (!Viewer || !Monitor || !Monitor->CanPlayerView(Viewer)) return;
	if (FVector::DistSquared(Viewer->GetActorLocation(), Monitor->GetActorLocation()) > FMath::Square(MonitorInteractionDistance)) return;

	Monitor->CycleCameraFeed(bNext);
}

void UCameraSystemComponent::Server_SpawnCamera_Implementation(const FTransform& SpawnTransform)
{
	if (!CameraClassToSpawn || !GetWorld() || !CanSpawnCameraForTeam()) return;
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	SpawnParams.Owner = GetOwner();
	SpawnParams.Instigator = Cast<APawn>(GetOwner());

	if (ASecurityCamera* NewCamera = GetWorld()->SpawnActor<ASecurityCamera>(CameraClassToSpawn, SpawnTransform, SpawnParams))
	{
		NewCamera->TeamID = TeamID;
		NewCamera->PlacedByPlayer = Cast<APawn>(GetOwner()) ? Cast<APawn>(GetOwner())->GetPlayerState() : nullptr;
	}
}

void UCameraSystemComponent::Server_SpawnMonitor_Implementation(const FTransform& SpawnTransform, bool bIsHorizontal)
{
	if (!MonitorClass || !GetWorld() || !CanSpawnMonitorForTeam()) return;
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	SpawnParams.Owner = GetOwner();
	SpawnParams.Instigator = Cast<APawn>(GetOwner());

	if (ASecurityMonitor* NewMonitor = GetWorld()->SpawnActor<ASecurityMonitor>(MonitorClass, SpawnTransform, SpawnParams))
	{
		NewMonitor->TeamID = TeamID;
		NewMonitor->PlacedByPlayer = Cast<APawn>(GetOwner()) ? Cast<APawn>(GetOwner())->GetPlayerState() : nullptr;
		NewMonitor->SetSurfaceType(bIsHorizontal);
		NewMonitor->RefreshTeamCameras();
	}
}

bool UCameraSystemComponent::CanSpawnCameraForTeam() const
{
	if (!GetWorld()) return false;
	TArray<AActor*> FoundCameras;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASecurityCamera::StaticClass(), FoundCameras);
	int32 Count = 0;
	for (AActor* Actor : FoundCameras)
	{
		if (ASecurityCamera* Camera = Cast<ASecurityCamera>(Actor); Camera && Camera->TeamID == TeamID) ++Count;
	}
	return Count < MaxCamerasPerTeam;
}

bool UCameraSystemComponent::CanSpawnMonitorForTeam() const
{
	if (!GetWorld()) return false;
	TArray<AActor*> FoundMonitors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASecurityMonitor::StaticClass(), FoundMonitors);
	int32 Count = 0;
	for (AActor* Actor : FoundMonitors)
	{
		// Cast must receive the class type, not a pointer-to-pointer type.
		if (ASecurityMonitor* Monitor = Cast<ASecurityMonitor>(Actor); Monitor && Monitor->TeamID == TeamID) ++Count;
	}
	return Count < MaxMonitorsPerTeam;
}
