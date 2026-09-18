#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SecurityMonitor.generated.h"

class ASecurityCamera;
class APlayerState;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

UCLASS()
class CAMERASYSTEM_API ASecurityMonitor : public AActor
{
	GENERATED_BODY()

public:
	ASecurityMonitor();

	void SetSurfaceType(bool bHorizontal);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* RootSceneComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MonitorMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Monitor Setup")
	UStaticMesh* DesktopStaticMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Monitor Setup")
	UStaticMesh* WallStaticMesh;

	UPROPERTY(ReplicatedUsing = OnRep_IsHorizontalSurface)
	bool bIsHorizontalSurface = true;

	UFUNCTION()
	void OnRep_IsHorizontalSurface();

public:
	UPROPERTY(ReplicatedUsing = OnRep_TeamID, EditInstanceOnly, BlueprintReadWrite, Category = "Monitor Setup")
	int32 TeamID = 0;

	// The player who placed this monitor. Team membership can change later.
	UPROPERTY(ReplicatedUsing = OnRep_TeamID, BlueprintReadOnly, Category = "Monitor Setup")
	TObjectPtr<APlayerState> PlacedByPlayer;

	UFUNCTION()
	void OnRep_TeamID();

	UPROPERTY(ReplicatedUsing = OnRep_CurrentCameraIndex, BlueprintReadOnly, Category = "Monitor Setup")
	int32 CurrentCameraIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Monitor Setup")
	TArray<ASecurityCamera*> TeamCameras;

	// Retained for direct Blueprint use; the player-owned component is preferred.
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Monitor Setup")
	void Server_CycleCameraFeed(bool bNext);

	// Server-only implementation used after the component validates the requester.
	void CycleCameraFeed(bool bNext);

	UFUNCTION(BlueprintCallable, Category = "Monitor Setup")
	void RefreshTeamCameras();

	UFUNCTION(BlueprintCallable, Category = "Monitor Setup")
	void UpdateActiveFeed();

	// Checks placer-or-team access for a specific viewer.
	bool CanPlayerView(APawn* Viewer) const;

protected:
	UFUNCTION()
	void OnRep_CurrentCameraIndex();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
