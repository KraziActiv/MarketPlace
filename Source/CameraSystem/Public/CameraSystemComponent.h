#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CameraSystemInterface.h"
#include "CameraSystemComponent.generated.h"

class ASecurityCamera;
class ASecurityMonitor;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent), Blueprintable)
class CAMERASYSTEM_API UCameraSystemComponent : public UActorComponent, public ICameraSystemInterface
{
	GENERATED_BODY()

public:
	UCameraSystemComponent();

	// Team ID supplied by the host game's team system. Zero means no team assigned.
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "Camera System|Config")
	int32 TeamID = 0;

	virtual int32 GetPlayerTeamID_Implementation() const override;

	// Must be called by the authoritative game/team system when this player's team changes.
	UFUNCTION(BlueprintCallable, Category = "Camera System")
	void SetTeamID(int32 NewTeamID);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera System|Config")
	TSubclassOf<ASecurityCamera> CameraClassToSpawn;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera System|Config")
	TSubclassOf<ASecurityMonitor> MonitorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera System|Config")
	float PlacementTraceDistance = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera System|Config")
	float MonitorInteractionDistance = 300.0f;

	UFUNCTION(BlueprintCallable, Category = "Camera System")
	void TryPlaceCamera();

	UFUNCTION(BlueprintCallable, Category = "Camera System")
	void TryPlaceMonitor();

	UFUNCTION(BlueprintCallable, Category = "Camera System")
	void TryCycleMonitor(ASecurityMonitor* Monitor, bool bNext = true);

	UFUNCTION(Server, Reliable, Category = "Camera System")
	void Server_SpawnCamera(const FTransform& SpawnTransform);

	UFUNCTION(Server, Reliable, Category = "Camera System")
	void Server_SpawnMonitor(const FTransform& SpawnTransform, bool bIsHorizontal);

	UFUNCTION(Server, Reliable, Category = "Camera System")
	void Server_RequestCycleMonitor(ASecurityMonitor* Monitor, bool bNext);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera System|Config")
	int32 MaxCamerasPerTeam = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera System|Config")
	int32 MaxMonitorsPerTeam = 2;

	bool CanSpawnCameraForTeam() const;
	bool CanSpawnMonitorForTeam() const;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void RefreshOwnedSecurityActorsTeam();
};
