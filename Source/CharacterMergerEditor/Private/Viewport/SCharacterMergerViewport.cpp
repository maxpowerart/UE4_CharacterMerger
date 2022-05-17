#include "Viewport/SCharacterMergerViewport.h"

#include "AssetSelection.h"
#include "LevelUtils.h"
#include "AdvancedPreviewScene/Public/AdvancedPreviewScene.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "Engine/StaticMeshActor.h"

void SCharacterMergerViewport::Construct(const FArguments& InArgs)
{
	PreviewScene = MakeShareable(new FCharacterMergerPreviewScene(FPreviewScene::ConstructionValues()));

	// restore last used feature level
	UWorld* PreviewWorld = PreviewScene->GetWorld();
	if (PreviewWorld != nullptr)
	{
		PreviewWorld->ChangeFeatureLevel(GWorld->FeatureLevel);
	}
	
	SEditorViewport::Construct( SEditorViewport::FArguments() );
}

void SCharacterMergerViewport::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects(Components);
}

SCharacterMergerViewport::~SCharacterMergerViewport()
{
	if (EditorViewportClient.IsValid())
	{
		EditorViewportClient->Viewport = NULL;
	}
}

FReply SCharacterMergerViewport::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	bool bValidDrop = false;
	TArray<FAssetData> SelectedAssetDatas;
	TSharedPtr< FDragDropOperation > Operation = DragDropEvent.GetOperation();
	if (!Operation.IsValid())
	{
		return FReply::Unhandled();
	}

	if (Operation->IsOfType<FAssetDragDropOp>())
	{
		TArray<FAssetData> DroppedAssetDatas = AssetUtil::ExtractAssetDataFromDrag(DragDropEvent);
		
		for (int32 AssetIdx = 0; AssetIdx < DroppedAssetDatas.Num(); ++AssetIdx)
		{
			const FAssetData& AssetData = DroppedAssetDatas[AssetIdx];
			UObject* Asset = AssetData.GetAsset();
			if ( Asset != nullptr)
			{
				if(UStaticMesh* SMAsset = Cast<UStaticMesh>(Asset))
				{
					UStaticMeshComponent* SMComponent = NewObject<UStaticMeshComponent>(GetTransientPackage());
					SMComponent->SetStaticMesh(SMAsset);
					Components.Add(SMComponent);
					PreviewScene->AddComponent(SMComponent, FTransform::Identity, true);
				}
				else if(USkeletalMesh* SKAsset = Cast<USkeletalMesh>(Asset))
				{
					USkeletalMeshComponent* SKComponent = NewObject<USkeletalMeshComponent>(GetTransientPackage());
					SKComponent->SetSkeletalMesh(SKAsset);
					Components.Add(SKComponent);
					PreviewScene->AddComponent(SKComponent, FTransform::Identity, true);
				}
			}
		}
	}

	return FReply::Handled();
	//return HandlePlaceDraggedObjects(MyGeometry, DragDropEvent, /*bCreateDropPreview=*/false) ? FReply::Handled() : FReply::Unhandled();
}

FReply SCharacterMergerViewport::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return SEditorViewport::OnMouseButtonDown(MyGeometry, MouseEvent);
}

void SCharacterMergerViewport::BindCommands()
{
	SEditorViewport::BindCommands();

	/**On-viewport actions*/
}
