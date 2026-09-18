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

	// Team / Guild / Clan ID assigned to this player component
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Camera System|Config")
	int32 TeamID = 0;

	// ICameraSystemInterface Implementation
	virtual int32 GetPlayerTeamID_Implementation() const override;

	// Helper function to set team at runtime
	UFUNCTION(BlueprintCallable, Category = "Camera System")
	void SetTeamID(int32 NewTeamID);

	// Class of camera actor to spawn
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera System|Config")
	TSubclassOf<ASecurityCamera> CameraClassToSpawn;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera System|Config")
	TSubclassOf<ASecurityMonitor> MonitorClass;

	// Max distance for placement line trace
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera System|Config")
	float PlacementTraceDistance = 1000.0f;

	// Main trigger function to place a camera
	UFUNCTION(BlueprintCallable, Category = "Camera System")
	void TryPlaceCamera();

	UFUNCTION(BlueprintCallable, Category = "Camera System")
	void TryPlaceMonitor();

	// Server RPC to execute replicated spawn
	UFUNCTION(Server, Reliable, Category = "Camera System")
	void Server_SpawnCamera(const FTransform& SpawnTransform);

	// Server RPC to execute replicated monitor spawn
	UFUNCTION(Server, Reliable, Category = "Camera System")
	void Server_SpawnMonitor(const FTransform& SpawnTransform, bool bIsHorizontal);

	// Maximum cameras allowed per team
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera System|Config")
	int32 MaxCamerasPerTeam = 4;

	// Maximum monitors allowed per team
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera System|Config")
	int32 MaxMonitorsPerTeam = 2;

	// Helper checks
	bool CanSpawnCameraForTeam() const;
	bool CanSpawnMonitorForTeam() const;

protected:

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};