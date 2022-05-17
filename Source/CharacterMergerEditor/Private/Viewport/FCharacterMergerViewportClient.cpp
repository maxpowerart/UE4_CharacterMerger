#include "Viewport/FCharacterMergerViewportClient.h"

#include "AssetEditorModeManager.h"
#include "EngineUtils.h"
#include "HModel.h"
#include "IPlacementModeModule.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/Selection.h"

FCharacterMergerViewportClient::FCharacterMergerViewportClient(const TWeakPtr<SEditorViewport>& InEditorViewportWidget, FCharacterMergerPreviewScene& InPreviewScene) :
		FEditorViewportClient(new FAssetEditorModeManager(), &InPreviewScene, InEditorViewportWidget)
{
	
}
FCharacterMergerViewportClient::~FCharacterMergerViewportClient()
{
}

bool FCharacterMergerViewportClient::DropObjectsOnBackground(FViewportCursorLocation& Cursor,
	const TArray<UObject*>& DroppedObjects, EObjectFlags ObjectFlags, TArray<AActor*>& OutNewActors,
	bool bCreateDropPreview, bool bSelectActors, UActorFactory* FactoryToUse)
{
	if (DroppedObjects.Num() == 0)
	{
		return false;
	}

	bool bSuccess = false;

	const bool bTransacted = !bCreateDropPreview;

	// Create a transaction if not a preview drop
	if (bTransacted)
	{
		GEditor->BeginTransaction(NSLOCTEXT("UnrealEd", "CreateActors", "Create Actors"));
	}

	for ( int32 DroppedObjectsIdx = 0; DroppedObjectsIdx < DroppedObjects.Num(); ++DroppedObjectsIdx )
	{
		UObject* AssetObj = DroppedObjects[DroppedObjectsIdx];
		ensure( AssetObj );

		// Attempt to create actors from the dropped object
		/*TArray<AActor*> NewActors = TryPlacingActorFromObject(GetWorld()->GetCurrentLevel(), AssetObj, bSelectActors, ObjectFlags, FactoryToUse, NAME_None, &Cursor);

		if ( NewActors.Num() > 0 )
		{
			OutNewActors.Append(NewActors);
			bSuccess = true;
		}*/
	}

	if (bTransacted)
	{
		GEditor->EndTransaction();
	}

	return bSuccess;
}

void FCharacterMergerViewportClient::Tick(float DeltaSeconds)
{
	FEditorViewportClient::Tick(DeltaSeconds);
}

FLinearColor FCharacterMergerViewportClient::GetBackgroundColor() const
{
	return FEditorViewportClient::GetBackgroundColor();
}

void FCharacterMergerViewportClient::AddReferencedObjects(FReferenceCollector& Collector)
{
	FEditorViewportClient::AddReferencedObjects(Collector);
}
