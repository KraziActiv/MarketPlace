// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "CameraSystemInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UCameraSystemInterface : public UInterface
{
	GENERATED_BODY()
};

class CAMERASYSTEM_API ICameraSystemInterface
{
	GENERATED_BODY()

public:
	
	// Returns the Team ID assigned to the player or actor
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Camera System")
	int32 GetPlayerTeamID() const;

};
