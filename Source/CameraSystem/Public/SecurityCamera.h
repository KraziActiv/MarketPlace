#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SecurityCamera.generated.h"

class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UStaticMeshComponent;

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

	// Team ID assigned when spawned (Replicated across network)
	UPROPERTY(ReplicatedUsing = OnRep_TeamID, EditInstanceOnly, BlueprintReadWrite, Category = "Camera Setup")
	int32 TeamID = 0;

	UFUNCTION()
	void OnRep_TeamID();

	// Local Render Target generated only on clients viewing the feed
	UPROPERTY(BlueprintReadOnly, Category = "Camera System")
	UTextureRenderTarget2D* LocalRenderTarget;

	// Render target resolution (Width & Height)
	UPROPERTY(EditDefaultsOnly, Category = "Camera System|Config")
	int32 RenderTargetResolution = 512;

	// Generates dynamic RT and assigns it to SceneCapture
	UFUNCTION(BlueprintCallable, Category = "Camera System")
	UTextureRenderTarget2D* GetOrCreateLocalRenderTarget();

	// Toggles scene capture rendering on/off to save performance
	UFUNCTION(BlueprintCallable, Category = "Camera System")
	void SetCaptureActive(bool bActive);

	virtual void Tick(float DeltaTime) override;

	// Scanning Settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Automation")
	bool bIsScanning = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Automation")
	float SweepSpeed = 30.0f; // Degrees per second

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Automation")
	float MaxYawAngle = 45.0f; // Max left/right angle

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Automation")
	float DefaultPitch = 20.0f; // Default downward tilt

private:
	float CurrentScanYaw = 0.0f;
	bool bSweepingRight = true;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};