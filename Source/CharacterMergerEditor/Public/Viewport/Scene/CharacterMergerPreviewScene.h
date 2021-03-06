// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PrettyPreviewScene.h: Pretty preview scene definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "InputCoreTypes.h"
#include "PreviewScene.h"
#include "TickableEditorObject.h"

class FViewport;
class UAssetViewerSettings;
class UMaterialInstanceConstant;
class UPostProcessComponent;
class USkyLightComponent;
class UStaticMeshComponent;
class USphereReflectionCaptureComponent;
struct FPreviewSceneProfile;
class FUICommandList;

class CHARACTERMERGEREDITOR_API FCharacterMergerPreviewScene : public FPreviewScene, public FTickableEditorObject
{
public:
	FCharacterMergerPreviewScene(ConstructionValues CVS, float InFloorOffset = 0.0f);
	~FCharacterMergerPreviewScene();

	virtual void AddComponent(UActorComponent* Component, const FTransform& LocalToWorld, bool bAttachToRoot = false) override;
	virtual void RemoveComponent(UActorComponent* Component) override;
	void UpdateScene(FPreviewSceneProfile& Profile, bool bUpdateSkyLight = true, bool bUpdateEnvironment = true, bool bUpdatePostProcessing = true, bool bUpdateDirectionalLight = true);

	USceneComponent* GetRoot() {return Root;}
	/** Begin FPreviewScene */
	virtual FLinearColor GetBackgroundColor() const override;
	/** End FPreviewScene */

	/* Begin FTickableEditorObject */
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
	virtual TStatId GetStatId() const override;
	/* End FTickableEditorObject */

	const bool HandleViewportInput(FViewport* InViewport, int32 ControllerId, FKey Key, float Delta, float DeltaTime, int32 NumSamples, bool bGamepad);
	const bool HandleInputKey(FViewport* InViewport, int32 ControllerId, FKey Key, EInputEvent Event, float AmountDepressed, bool Gamepad);

	void SetSkyRotation(const float SkyRotation);
	/* Sets the visiblity state for the floor/environment by storing it in the scene profile and refreshing the scene, in case bDirect is true it sets the visibility directly and leaves the profile untouched. */
	void SetFloorVisibility(const bool bVisible, const bool bDirect = false);
	void SetEnvironmentVisibility(const bool bVisible, const bool bDirect = false);
	void SetFloorOffset(const float InFloorOffset);
	void SetProfileIndex(const int32 InProfileIndex);

	const UStaticMeshComponent* GetFloorMeshComponent() const;
	const float GetSkyRotation() const;
	const int32 GetCurrentProfileIndex() const;
	const bool IsUsingPostProcessing() const;

protected:
	/** Bind our command bindings to handlers */
	void BindCommands();

	/** Toggle the sky sphere on and off */
	void HandleToggleEnvironment();

	/** Toggle the floor mesh on and off */
	void HandleToggleFloor();

	/** Toggle post processing on and off */
	void HandleTogglePostProcessing();

	/** Handle refreshing the scene when settings change */
	void OnAssetViewerSettingsRefresh(const FName& InPropertyName);

protected:
	UStaticMeshComponent* SkyComponent;
	UMaterialInstanceConstant* InstancedSkyMaterial;
	UPostProcessComponent* PostProcessComponent;
	UStaticMeshComponent* FloorMeshComponent;
	UAssetViewerSettings* DefaultSettings;	
	bool bRotateLighting;

	float CurrentRotationSpeed;
	float PreviousRotation;

	bool bSkyChanged;
	bool bPostProcessing;

	int32 CurrentProfileIndex;

	/** Command list for input handling */
	TSharedPtr<FUICommandList> UICommandList;

	/** Delegate handle used to refresh the scene when settings change */
	FDelegateHandle RefreshDelegate;

	USceneComponent* Root;
};
