#pragma once

#include "CoreMinimal.h"
#include "FCharacterMergerViewportClient.h"
#include "SEditorViewport.h"

class CHARACTERMERGEREDITOR_API SCharacterMergerViewport : public SEditorViewport, public FGCObject
{
public:

	SLATE_BEGIN_ARGS(SCharacterMergerViewport)
		: _ViewportType(LVT_Perspective)
		, _Realtime(true)
	{
	}

	SLATE_ARGUMENT(TWeakPtr<class FEditorModeTools>, EditorModeTools)
		SLATE_ARGUMENT(TSharedPtr<class FEditorViewportLayout>, ParentLayout)
		SLATE_ARGUMENT(TSharedPtr<FEditorViewportClient>, EditorViewportClient)
		SLATE_ARGUMENT(ELevelViewportType, ViewportType)
		SLATE_ARGUMENT(bool, Realtime)
		SLATE_ARGUMENT(FName, ConfigKey)
		SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs);
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;
	~SCharacterMergerViewport();
	
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	
	TWeakPtr<FEditorViewportLayout> ParentLayout;

	TArray<UMeshComponent*> GetComponents() { return Components; }
	TArray<USkeletalMesh*> GetMeshes()
	{
		TArray<USkeletalMesh*> Meshes;
		for(auto Component : Components)
		{
			if(USkeletalMeshComponent* SKMeshComp = Cast<USkeletalMeshComponent>(Component))
			{
				Meshes.Add(SKMeshComp->SkeletalMesh);
			}
		}
		return Meshes;
	}
	TSharedPtr<class FCharacterMergerPreviewScene> GetPreviewScene() { return PreviewScene; }

protected:
	virtual void BindCommands() override;

	TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override 
	{
		if (!EditorViewportClient.IsValid())
		{
			EditorViewportClient = MakeShareable(new FCharacterMergerViewportClient(nullptr, *PreviewScene.Get()));
			
		}
		return EditorViewportClient.ToSharedRef();
	}
	
private:
	// Viewport client
	TSharedPtr<FCharacterMergerViewportClient> EditorViewportClient;

	/** Preview Scene - uses advanced preview settings */
	TSharedPtr<class FCharacterMergerPreviewScene> PreviewScene;

	/** Caching off of the DragDropEvent Local Mouse Position grabbed from OnDrop */
	FVector2D CachedOnDropLocalMousePos;

	TArray<UMeshComponent*> Components;
};