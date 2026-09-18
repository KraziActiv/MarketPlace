#include "CameraSystemComponent.h"
#include "SecurityCamera.h"
#include "SecurityMonitor.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"

UCameraSystemComponent::UCameraSystemComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UCameraSystemComponent::BeginPlay()
{
	Super::BeginPlay();

	// Server handles team assignment and replicates it to clients
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		// If TeamID is unassigned (0), generate an ID based on network identity or unique object ID
		if (TeamID == 0)
		{
			APawn* OwnerPawn = Cast<APawn>(GetOwner());
			if (OwnerPawn && OwnerPawn->GetController())
			{
				// Shift by +1 so team IDs are always positive, distinct numbers
				TeamID = OwnerPawn->GetController()->GetUniqueID() + 1;
			}
			else
			{
				// Fallback to Actor's unique runtime ID if no Controller exists yet
				TeamID = GetOwner()->GetUniqueID() + 1;
			}
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
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		TeamID = NewTeamID;
	}
}

void UCameraSystemComponent::TryPlaceCamera()
{
	AActor* Owner = GetOwner();
	if (!Owner || !GetWorld()) return;

	FVector Start;
	FRotator Rotation;
	Owner->GetActorEyesViewPoint(Start, Rotation);

	FVector End = Start + (Rotation.Vector() * PlacementTraceDistance);

	FHitResult HitResult;
	FCollisionQueryParams TraceParams;
	TraceParams.AddIgnoredActor(Owner);

	bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, TraceParams);

	if (bHit && HitResult.bBlockingHit)
	{
		FRotator SpawnRotation = FRotationMatrix::MakeFromX(HitResult.ImpactNormal).Rotator();
		FTransform SpawnTransform(SpawnRotation, HitResult.ImpactPoint);

		Server_SpawnCamera(SpawnTransform);
	}
}

void UCameraSystemComponent::TryPlaceMonitor()
{
	AActor* Owner = GetOwner();
	if (!Owner || !GetWorld()) return;

	FVector Start;
	FRotator Rotation;
	Owner->GetActorEyesViewPoint(Start, Rotation);

	FVector End = Start + (Rotation.Vector() * PlacementTraceDistance);

	FHitResult HitResult;
	FCollisionQueryParams TraceParams;
	TraceParams.AddIgnoredActor(Owner);

	bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, TraceParams);

	if (bHit && HitResult.bBlockingHit)
	{
		FRotator SpawnRotation;

		bool bIsHorizontalSurface = FMath::IsNearlyEqual(FMath::Abs(HitResult.ImpactNormal.Z), 1.0f, 0.1f);

		if (bIsHorizontalSurface)
		{
			FVector PlayerFacing = (Start - HitResult.ImpactPoint).GetSafeNormal2D();
			SpawnRotation = PlayerFacing.Rotation();
		}
		else
		{
			SpawnRotation = FRotationMatrix::MakeFromX(HitResult.ImpactNormal).Rotator();
		}

		FTransform SpawnTransform(SpawnRotation, HitResult.ImpactPoint);

		Server_SpawnMonitor(SpawnTransform, bIsHorizontalSurface);
	}
}

void UCameraSystemComponent::Server_SpawnCamera_Implementation(const FTransform& SpawnTransform)
{
	if (!CameraClassToSpawn || !GetWorld() || !CanSpawnCameraForTeam()) return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	SpawnParams.Owner = GetOwner();
	SpawnParams.Instigator = Cast<APawn>(GetOwner());

	ASecurityCamera* NewCamera = GetWorld()->SpawnActor<ASecurityCamera>(CameraClassToSpawn, SpawnTransform, SpawnParams);
	if (NewCamera)
	{
		NewCamera->TeamID = TeamID;
	}
}

void UCameraSystemComponent::Server_SpawnMonitor_Implementation(const FTransform& SpawnTransform, bool bIsHorizontal)
{
	if (!MonitorClass || !GetWorld() || !CanSpawnMonitorForTeam()) return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	SpawnParams.Owner = GetOwner();
	SpawnParams.Instigator = Cast<APawn>(GetOwner());

	ASecurityMonitor* NewMonitor = GetWorld()->SpawnActor<ASecurityMonitor>(MonitorClass, SpawnTransform, SpawnParams);
	if (NewMonitor)
	{
		NewMonitor->TeamID = TeamID;
		NewMonitor->SetSurfaceType(bIsHorizontal);

		// Count existing team monitors to offset starting camera index
		TArray<AActor*> FoundMonitors;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASecurityMonitor::StaticClass(), FoundMonitors);

		int32 MonitorCount = 0;
		for (AActor* Actor : FoundMonitors)
		{
			if (ASecurityMonitor* Mon = Cast<ASecurityMonitor>(Actor))
			{
				if (Mon->TeamID == TeamID) MonitorCount++;
			}
		}

		// Stagger camera feed starting index across monitors (Monitor 1 = Cam 0, Monitor 2 = Cam 1, etc.)
		NewMonitor->CurrentCameraIndex = FMath::Max(0, MonitorCount - 1);
		NewMonitor->RefreshTeamCameras();
	}
}

bool UCameraSystemComponent::CanSpawnCameraForTeam() const
{
	if (!GetWorld()) return false;

	TArray<AActor*> FoundCameras;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASecurityCamera::StaticClass(), FoundCameras);

	int32 CurrentTeamCameraCount = 0;
	for (AActor* Actor : FoundCameras)
	{
		if (ASecurityCamera* Cam = Cast<ASecurityCamera>(Actor))
		{
			if (Cam->TeamID == TeamID)
			{
				CurrentTeamCameraCount++;
			}
		}
	}

	return CurrentTeamCameraCount < MaxCamerasPerTeam;
}

bool UCameraSystemComponent::CanSpawnMonitorForTeam() const
{
	if (!GetWorld()) return false;

	TArray<AActor*> FoundMonitors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASecurityMonitor::StaticClass(), FoundMonitors);

	int32 CurrentTeamMonitorCount = 0;
	for (AActor* Actor : FoundMonitors)
	{
		if (ASecurityMonitor* Mon = Cast<ASecurityMonitor>(Actor))
		{
			if (Mon->TeamID == TeamID)
			{
				CurrentTeamMonitorCount++;
			}
		}
	}

	return CurrentTeamMonitorCount < MaxMonitorsPerTeam;
}