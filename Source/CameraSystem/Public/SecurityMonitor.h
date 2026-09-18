#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SecurityMonitor.generated.h"

class ASecurityCamera;

UCLASS()
class CAMERASYSTEM_API ASecurityMonitor : public AActor
{
	GENERATED_BODY()

public:
	ASecurityMonitor();

	// Sets surface orientation on Server and syncs to clients
	void SetSurfaceType(bool bHorizontal);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* RootSceneComponent;

	// The actual mesh component rendered in the world
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MonitorMesh;

	// Asset reference assigned in Blueprint defaults for Desktop
	UPROPERTY(EditDefaultsOnly, Category = "Monitor Setup")
	UStaticMesh* DesktopStaticMesh;

	// Asset reference assigned in Blueprint defaults for Wall
	UPROPERTY(EditDefaultsOnly, Category = "Monitor Setup")
	UStaticMesh* WallStaticMesh;

	// Replicated placement state
	UPROPERTY(ReplicatedUsing = OnRep_IsHorizontalSurface)
	bool bIsHorizontalSurface = true;

	UFUNCTION()
	void OnRep_IsHorizontalSurface();

public:
	// Team ownership
	UPROPERTY(ReplicatedUsing = OnRep_TeamID, EditInstanceOnly, BlueprintReadWrite, Category = "Monitor Setup")
	int32 TeamID = 0;

	UFUNCTION()
	void OnRep_TeamID();

	// Index of the camera feed currently displaying on screen
	UPROPERTY(ReplicatedUsing = OnRep_CurrentCameraIndex, BlueprintReadOnly, Category = "Monitor Setup")
	int32 CurrentCameraIndex = 0;

	// Cached list of active cameras on this team
	UPROPERTY(BlueprintReadOnly, Category = "Monitor Setup")
	TArray<ASecurityCamera*> TeamCameras;

	// Cycle to Next (+1) or Previous (-1) camera feed
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Monitor Setup")
	void Server_CycleCameraFeed(bool bNext);

	// Refresh the list of team cameras available in the level
	UFUNCTION(BlueprintCallable, Category = "Monitor Setup")
	void RefreshTeamCameras();

	// Updates active material feed
	UFUNCTION(BlueprintCallable, Category = "Monitor Setup")
	void UpdateActiveFeed();

protected:
	UFUNCTION()
	void OnRep_CurrentCameraIndex();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};