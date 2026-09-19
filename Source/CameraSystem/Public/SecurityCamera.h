#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SecurityCamera.generated.h"

class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UStaticMeshComponent;
class APlayerState;

UCLASS()
class CAMERASYSTEM_API ASecurityCamera : public AActor
{
	GENERATED_BODY()

public:
	ASecurityCamera();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* RootSceneComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* CameraBaseMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* CameraMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneCaptureComponent2D* SceneCapture;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Camera Ownership")
	TObjectPtr<APlayerState> PlacedByPlayer;

	UPROPERTY(ReplicatedUsing = OnRep_TeamID, BlueprintReadOnly, Category = "Camera Ownership")
	int32 TeamID = 0;

	UFUNCTION()
	void OnRep_TeamID();

	UPROPERTY(BlueprintReadOnly, Category = "Camera System")
	UTextureRenderTarget2D* LocalRenderTarget;

	UPROPERTY(EditDefaultsOnly, Category = "Camera System|Config")
	int32 RenderTargetResolution = 512;

	UPROPERTY(EditDefaultsOnly, Category = "Camera System|Config")
	int32 RenderTargetWidth = 1024;

	UPROPERTY(EditDefaultsOnly, Category = "Camera System|Config")
	int32 RenderTargetHeight = 552;

	UFUNCTION(BlueprintCallable, Category = "Camera System")
	UTextureRenderTarget2D* GetOrCreateLocalRenderTarget();

	UFUNCTION(BlueprintCallable, Category = "Camera System")
	void SetCaptureActive(bool bActive);

	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Automation")
	bool bIsScanning = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Automation")
	float SweepSpeed = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Automation")
	float MaxYawAngle = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Automation")
	float DefaultPitch = 20.0f;

private:
	float CurrentScanYaw = 0.0f;
	bool bSweepingRight = true;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
