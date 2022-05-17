#pragma once

#include "CoreMinimal.h"
#include "AdvancedPreviewScene.h"
#include "EditorViewportClient.h"
#include "SEditorViewport.h"
#include "Scene/CharacterMergerPreviewScene.h"

//////////////////////////////////////////////////////////////////////////
// FCharacterMergerViewportClient

class FCharacterMergerViewportClient : public FEditorViewportClient
{
public:
	/** Constructor */
	explicit FCharacterMergerViewportClient(const TWeakPtr<class SEditorViewport>& InEditorViewportWidget,  FCharacterMergerPreviewScene& InPreviewScene);
	~FCharacterMergerViewportClient();
	
	/**Drop operations*/
	bool DropObjectsOnBackground( struct FViewportCursorLocation& Cursor, const TArray<UObject*>& DroppedObjects, EObjectFlags ObjectFlags,
		TArray<AActor*>& OutNewActors, bool bCreateDropPreview = false, bool bSelectActors = true, class UActorFactory* FactoryToUse = NULL);

	// FViewportClient interface
	virtual void Tick(float DeltaSeconds) override;
	// End of FViewportClient interface

	// FEditorViewportClient interface
	virtual FLinearColor GetBackgroundColor() const override;
	// End of FEditorViewportClient interface

	// FSerializableObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	// End of FSerializableObject interface
};
