﻿#pragma once

#include "CoreMinimal.h"
#include "AssetToolsModule.h"
#include "FileHelpers.h"
#include "IAssetTools.h"
#include "SkeletalMeshMerge.h"
#include "Animation/MorphTarget.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "RuntimeSkeletalMeshGenerator/RuntimeSkeletalMeshGenerator.h"
#include "RuntimeSkeletalMeshGenerator/RuntimeSkeletonBoneTransformExtractor.h"

static FString DefaultRoot = "/Game/";
static FString DefaultSavePath = "MergedSource/";
static FString DefaultSaveName = "MergedStaticMesh";

struct FSkelMeshRenderSection;

struct FCMRefPoseOverride
{

	enum EBoneOverrideMode
	{
		BoneOnly,			// Override the bone only.
		ChildrenOnly,		// Override the bone's children only.
		BoneAndChildren,	// Override both the bone & children.
		MAX,
	};

	/**
	 * Constructs a FRefPoseOverride.
	 */
	FCMRefPoseOverride(const USkeletalMesh* ReferenceMesh)
	{
		this->SkeletalMesh = ReferenceMesh;
	}

	/**
	 * Adds a bone to the list of poses to override.
	 */
	void AddOverride(FName BoneName, EBoneOverrideMode OverrideMode = BoneOnly)
	{
		FCMBoneOverrideInfo OverrideInfo;
		OverrideInfo.BoneName = BoneName;
		OverrideInfo.OverrideMode = OverrideMode;

		Overrides.Add(OverrideInfo);
	}
	
	struct FCMBoneOverrideInfo
	{
		/** The names of the bone to override. */
		FName BoneName;

		/** Whether the override applies to the bone, bone's children, or both. */
		EBoneOverrideMode OverrideMode;
	};

	/** The skeletal mesh that contains the reference pose. */
	const USkeletalMesh* SkeletalMesh;

	/** The list of bone overrides. */
	TArray<FCMBoneOverrideInfo> Overrides;
};

/** 
* Info to map all the sections from a single source skeletal mesh to 
* a final section entry int he merged skeletal mesh
*/
struct FCMSkelMeshMergeSectionMapping
{
	/** indices to final section entries of the merged skel mesh */
	TArray<int32> SectionIDs;
};

/** 
* Info to map all the sections about how to transform their UVs
*/
struct FCMSkelMeshMergeUVTransforms
{
	/** For each UV channel on each mesh, how the UVS should be transformed. */
	TArray<TArray<FTransform>> UVTransformsPerMesh;
};

class FCMSkeletalMeshMerge
{
public:
	/**
	* Constructor
	* @param InMergeMesh - destination mesh to merge to
	* @param InSrcMeshList - array of source meshes to merge
	* @param InForceSectionMapping - optional array to map sections from the source meshes to merged section entries
	* @param StripTopLODs - number of high LODs to remove from input meshes
    * @param bMeshNeedsCPUAccess - (optional) if the resulting mesh needs to be accessed by the CPU for any reason (e.g. for spawning particle effects).
	* @param UVTransforms - optional array to transform the UVs in each mesh
	*/
	FCMSkeletalMeshMerge( 
		USkeletalMesh* InMergeMesh, 
		const TArray<USkeletalMesh*>& InSrcMeshList, 
		const TArray<FCMSkelMeshMergeSectionMapping>& InForceSectionMapping,
		int32 InStripTopLODs,
        EMeshBufferAccess InMeshBufferAccess=EMeshBufferAccess::Default,
		FCMSkelMeshMergeUVTransforms* InSectionUVTransforms = nullptr
		);

	/**
	 * Merge/Composite skeleton and meshes together from the list of source meshes.
	 * @param RefPoseOverrides - An optional override for the merged skeleton's reference pose.
	 * @return true if succeeded
	 */
	bool DoMerge(TArray<FCMRefPoseOverride>* RefPoseOverrides = nullptr);

	/**
	 * Create the 'MergedMesh' reference skeleton from the skeletons in the 'SrcMeshList'.
	 * Use when the reference skeleton is needed prior to finalizing the merged meshes (do not use with DoMerge()).
	 * @param RefPoseOverrides - An optional override for the merged skeleton's reference pose.
	 */
	void MergeSkeleton(const TArray<FCMRefPoseOverride>* RefPoseOverrides = nullptr);

	/**
	 * Creates the merged mesh from the 'SrcMeshList' (note, this should only be called after MergeSkeleton()).
 	 * Use when the reference skeleton is needed prior to finalizing the merged meshes (do not use with DoMerge()).
	 * @return 'true' if successful; 'false' otherwise.
	 */
	bool FinalizeMesh();

	/** Destination merged mesh */
	USkeletalMesh* MergeMesh;
	
	/** Array of source skeletal meshes  */
	TArray<USkeletalMesh*> SrcMeshList;

	TMap<FName, TMap<int32, FVector>> MergedMorphMap;

	/** Number of high LODs to remove from input meshes. */
	int32 StripTopLODs;

    /** Whether or not the resulting mesh needs to be accessed by the CPU (e.g. for particle spawning).*/
    EMeshBufferAccess MeshBufferAccess;

	/** Info about source mesh used in merge. */
	struct FCMMergeMeshInfo
	{
		/** Mapping from RefSkeleton bone index in source mesh to output bone index. */
		TArray<int32> SrcToDestRefSkeletonMap;

		TMap<FName, TMap<int32, FVector>> MorphMap;
	};

	/** Array of source mesh info structs. */
	TArray<FCMMergeMeshInfo> SrcMeshInfo;

	/** New reference skeleton, made from creating union of each part's skeleton. */
	FReferenceSkeleton NewRefSkeleton;

	/** array to map sections from the source meshes to merged section entries */
	const TArray<FCMSkelMeshMergeSectionMapping>& ForceSectionMapping;

	/** optional array to transform UVs in each source mesh */
	const FCMSkelMeshMergeUVTransforms* SectionUVTransforms;

	/** Matches the Materials array in the final mesh - used for creating the right number of Material slots. */
	TArray<int32>	MaterialIds;

	/** keeps track of an existing section that need to be merged with another */
	struct FCMMergeSectionInfo
	{
		/** ptr to source skeletal mesh for this section */
		const USkeletalMesh* SkelMesh;
		/** ptr to source section for merging */
		const FSkelMeshRenderSection* Section;
		/** mapping from the original BoneMap for this sections chunk to the new MergedBoneMap */
		TArray<FBoneIndexType> BoneMapToMergedBoneMap;
		/** transform from the original UVs */
		TArray<FTransform> UVTransforms;

		FCMMergeSectionInfo( const USkeletalMesh* InSkelMesh,const FSkelMeshRenderSection* InSection, TArray<FTransform> & InUVTransforms )
			:	SkelMesh(InSkelMesh)
			,	Section(InSection)
			,	UVTransforms(InUVTransforms)
		{}
	};

	/** info needed to create a new merged section */
	struct FCMNewSectionInfo
	{
		/** array of existing sections to merge */
		TArray<FCMMergeSectionInfo> MergeSections;
		/** merged bonemap */
		TArray<FBoneIndexType> MergedBoneMap;
		/** material for use by this section */
		UMaterialInterface* Material;
		
		/** 
		* if -1 then we use the Material* to match new section entries
		* otherwise the MaterialId is used to find new section entries
		*/
		int32 MaterialId;

		/** material slot name, if multiple section use the same material we use the first slot name found */
		FName SlotName;

		/** Default UVChannelData for new sections. Will be recomputed if necessary */
		FMeshUVChannelInfo UVChannelData;

		FCMNewSectionInfo( UMaterialInterface* InMaterial, int32 InMaterialId, FName InSlotName, const FMeshUVChannelInfo& InUVChannelData )
			:	Material(InMaterial)
			,	MaterialId(InMaterialId)
			,	SlotName(InSlotName)
			,	UVChannelData(InUVChannelData)
		{}
	};

	/**
	* Merge a bonemap with an existing bonemap and keep track of remapping
	* (a bonemap is a list of indices of bones in the USkeletalMesh::RefSkeleton array)
	* @param MergedBoneMap - out merged bonemap
	* @param BoneMapToMergedBoneMap - out of mapping from original bonemap to new merged bonemap 
	* @param BoneMap - input bonemap to merge
	*/
	void MergeBoneMap( TArray<FBoneIndexType>& MergedBoneMap, TArray<FBoneIndexType>& BoneMapToMergedBoneMap, const TArray<FBoneIndexType>& BoneMap );

	/**
	* Creates a new LOD model and adds the new merged sections to it. Modifies the MergedMesh.
	* @param LODIdx - current LOD to process
	*/
	template<typename VertexDataType>
	void GenerateLODModel( int32 LODIdx);
	
	/**
	* Generate the list of sections that need to be created along with info needed to merge sections
	* @param NewSectionArray - out array to populate
	* @param LODIdx - current LOD to process
	*/
	void GenerateNewSectionArray( TArray<FCMNewSectionInfo>& NewSectionArray, int32 LODIdx );

	/**
	* (Re)initialize and merge skeletal mesh info from the list of source meshes to the merge mesh
	* @return true if succeeded
	*/
	bool ProcessMergeMesh();

	/**
	 * Returns the number of LODs that can be supported by the meshes in 'SourceMeshList'.
	 */
	int32 CalculateLodCount(const TArray<USkeletalMesh*>& SourceMeshList) const;

	/**
	 * Builds a new 'RefSkeleton' from the reference skeletons in the 'SourceMeshList'.
	 */
	static void BuildReferenceSkeleton(const TArray<USkeletalMesh*>& SourceMeshList, FReferenceSkeleton& RefSkeleton, const USkeleton* SkeletonAsset);

	/**
	 * Overrides the 'TargetSkeleton' bone poses with the bone poses specified in the 'PoseOverrides' array.
	 */
	static void OverrideReferenceSkeletonPose(const TArray<FCMRefPoseOverride>& PoseOverrides, FReferenceSkeleton& TargetSkeleton, const USkeleton* SkeletonAsset);

	/**
	 * Override the 'TargetSkeleton' bone pose with the pose from from the 'SourceSkeleton'.
	 * @return 'true' if the override was successful; 'false' otherwise.
	 */
	static bool OverrideReferenceBonePose(int32 SourceBoneIndex, const FReferenceSkeleton& SourceSkeleton, FReferenceSkeletonModifier& TargetSkeleton);

	/**
	 * Releases any resources the 'MergeMesh' is currently holding.
	 */
	void ReleaseResources(int32 Slack = 0);

	/**
	 * Copies and adds the 'NewSocket' to the MergeMesh's MeshOnlySocketList only if the socket does not already exist.
	 * @return 'true' if the socket is added; 'false' otherwise.
	 */
	bool AddSocket(const USkeletalMeshSocket* NewSocket, bool bIsSkeletonSocket);

	/**
	 * Adds only the new sockets from the 'NewSockets' array to the 'ExistingSocketList'.
	 */
	void AddSockets(const TArray<USkeletalMeshSocket*>& NewSockets, bool bAreSkeletonSockets);

	/**
	 * Builds a new 'SocketList' from the sockets in the 'SourceMeshList'.
	 */
	void BuildSockets(const TArray<USkeletalMesh*>& SourceMeshList);

	//void OverrideSockets(const TArray<FRefPoseOverride>& PoseOverrides);

	/**
	 * Override the corresponding 'MergeMesh' socket with 'SourceSocket'.
	 */
	void OverrideSocket(const USkeletalMeshSocket* SourceSocket);

	/**
	 * Overrides the sockets attached to 'BoneName' with the corresponding socket in the 'SourceSocketList'.
	 */
	void OverrideBoneSockets(const FName& BoneName, const TArray<USkeletalMeshSocket*>& SourceSocketList);

	/**
	 * Overrides the sockets of overridden bones.
	 */
	void OverrideMergedSockets(const TArray<FCMRefPoseOverride>& PoseOverrides);

	/*
	 * Copy Vertex Buffer from Source LOD Model
	 */
	template<typename VertexDataType>
	void CopyVertexFromSource(VertexDataType& DestVert, const FSkeletalMeshLODRenderData& SrcLODData, int32 SourceVertIdx, const FCMMergeSectionInfo& MergeSectionInfo, int32 NumVertexes);
};

class FCharacterMergerLibrary
{
public:
	
	static void MergeRequest(TArray<USkeletalMesh*> ComponentsToWeld)
	{
		if (ComponentsToWeld.Num() == 0) return;

		FString SaveName = DefaultRoot + DefaultSavePath + DefaultSaveName;
		TArray<USkeletalMesh*>& SourceMeshList = ComponentsToWeld;
		UPackage* Package = CreatePackage( *SaveName);

		USkeletalMesh* CompositeMesh = NewObject<USkeletalMesh>();
		CompositeMesh->PreEditChange(NULL);
		CompositeMesh->SetRefSkeleton(ComponentsToWeld[0]->GetSkeleton()->GetReferenceSkeleton());
		CompositeMesh->SetSkeleton(ComponentsToWeld[0]->GetSkeleton());
		
		TArray<FSkelMeshMergeSectionMapping> InForceSectionMapping;
		FSkeletalMeshMerge MeshMergeUtil(CompositeMesh, SourceMeshList, InForceSectionMapping, 0);
		if (!MeshMergeUtil.DoMerge())
		{
			return;
		}
		// Retrieve the imported resource structure and allocate a new LOD model
		FSkeletalMeshModel* ImportedModel = CompositeMesh->GetImportedModel();
		check(ImportedModel->LODModels.Num() == 0);
		ImportedModel->LODModels.Empty();
		ImportedModel->EmptyOriginalReductionSourceMeshData();
		ImportedModel->LODModels.Add(new FSkeletalMeshLODModel());
		CompositeMesh->ResetLODInfo();
		CompositeMesh->AddLODInfo();

		
		/**Decompose meshes*/
		TArray<FMeshSurface> Surfaces;
		TArray<int32> SurfacesVertexOffsets;
		TArray<int32> SurfacesIndexOffsets;
		TArray<UMaterialInterface*> SurfacesMaterial;
		FRuntimeSkeletalMeshGenerator::DecomposeSkeletalMesh(CompositeMesh, Surfaces, SurfacesVertexOffsets, SurfacesIndexOffsets, SurfacesMaterial);
		ParseMorphs(ComponentsToWeld[0], Surfaces[0]);

		USkeletalMesh* RecompositeMesh = NewObject<USkeletalMesh>(Package, NAME_None, RF_Public | RF_Standalone);
		RecompositeMesh->SetRefSkeleton(ComponentsToWeld[0]->GetSkeleton()->GetReferenceSkeleton());
		RecompositeMesh->SetSkeleton(ComponentsToWeld[0]->GetSkeleton());

		/**Calc remapping for new skeleton*/
		TArray<int32> Remapping;
		if (CompositeMesh)
		{
			Remapping.AddUninitialized(CompositeMesh->GetRefSkeleton().GetRawBoneNum());

			for (int32 i = 0; i < CompositeMesh->GetRefSkeleton().GetRawBoneNum(); i++)
			{
				FName SrcBoneName = CompositeMesh->GetRefSkeleton().GetBoneName(i);
				int32 DestBoneIndex = RecompositeMesh->GetRefSkeleton().FindBoneIndex(SrcBoneName);

				if (DestBoneIndex == INDEX_NONE)
				{
					// Missing bones shouldn't be possible, but can happen with invalid meshes;
					// map any bone we are missing to the 'root'.

					DestBoneIndex = 0;
				}

				Remapping[i] = DestBoneIndex;
			}
		}
		
		FRuntimeSkeletalMeshGenerator::GenerateSkeletalMesh(RecompositeMesh, CompositeMesh, Surfaces, SurfacesMaterial, Remapping);

		/**Save*/
		Package->SetDirtyFlag(true);
		UEditorLoadingAndSavingUtils::SaveDirtyPackages(false, true);
	};

	static void CompareMeshRigging(USkeletalMesh* SourceMesh, USkeletalMesh* TargetMesh)
	{
		/**Compare skeleton mapping*/
				const auto& SourceWeights = SourceMesh->GetResourceForRendering()->LODRenderData[0].SkinWeightVertexBuffer;
		const auto& TargetWeights = TargetMesh->GetResourceForRendering()->LODRenderData[0].SkinWeightVertexBuffer;

		check(SourceMesh->GetResourceForRendering()->LODRenderData[0].RequiredBones.Num() ==
				TargetMesh->GetResourceForRendering()->LODRenderData[0].RequiredBones.Num());

		for(int32 Iterator = 0; Iterator < SourceMesh->GetResourceForRendering()->LODRenderData[0].RequiredBones.Num(); Iterator++)
		{
			if(SourceMesh->GetResourceForRendering()->LODRenderData[0].RequiredBones[Iterator] !=
				TargetMesh->GetResourceForRendering()->LODRenderData[0].RequiredBones[Iterator])
			{
				UE_LOG(LogTemp, Error, TEXT("Required bones definition at index %i"), Iterator);
			}
		}
		for(int32 Iterator = 0; Iterator < SourceMesh->GetResourceForRendering()->LODRenderData[0].ActiveBoneIndices.Num(); Iterator++)
		{
			if(SourceMesh->GetResourceForRendering()->LODRenderData[0].ActiveBoneIndices[Iterator] !=
				TargetMesh->GetResourceForRendering()->LODRenderData[0].ActiveBoneIndices[Iterator])
			{
				UE_LOG(LogTemp, Error, TEXT("Active bones definition at index %i"), Iterator);
			}
		}

		TArray<FSkinWeightInfo> SourceWeightArr;
		TArray<FSkinWeightInfo> TargetWeightArr;
		SourceWeights.GetSkinWeights(SourceWeightArr);
		TargetWeights.GetSkinWeights(TargetWeightArr);

		check(SourceWeightArr.Num() == TargetWeightArr.Num());
		int32 BonesDiff = 0;
		int32 WeightsDiff = 0;
		for(int32 Iterator = 0; Iterator < SourceWeightArr.Num(); Iterator++)
			{
				if(SourceWeightArr[Iterator].InfluenceBones != TargetWeightArr[Iterator].InfluenceBones ||
					SourceWeightArr[Iterator].InfluenceWeights != TargetWeightArr[Iterator].InfluenceWeights)
				{
					for(int32 InternalIt = 0; InternalIt < 12; InternalIt++)
					{
						if(SourceWeightArr[Iterator].InfluenceBones[InternalIt] != TargetWeightArr[Iterator].InfluenceBones[InternalIt])
						{
							FString SourceBoneName = SourceMesh->GetSkeleton()->GetReferenceSkeleton().GetBoneName(
								SourceWeightArr[Iterator].InfluenceBones[InternalIt]).ToString();
							FString TargetBoneName = TargetWeightArr[Iterator].InfluenceBones[InternalIt] == 65535 ?
								FString("which are invalid") :
								TargetMesh->GetSkeleton()->GetReferenceSkeleton().GetBoneName(TargetWeightArr[Iterator].InfluenceBones[InternalIt]).ToString();
							UE_LOG(LogTemp, Error, TEXT("Bone %s replaced with bone %s"), *SourceBoneName, *TargetBoneName);
							BonesDiff+=1;
						}
						if(SourceWeightArr[Iterator].InfluenceWeights[InternalIt] != TargetWeightArr[Iterator].InfluenceWeights[InternalIt])
						{
							UE_LOG(LogTemp, Error, TEXT("Influence weights definition at index %i on bone %i"), Iterator, InternalIt);
							WeightsDiff+=1;
						}
					}
				}
			}
		UE_LOG(LogTemp, Error, TEXT("Weights diff: %i Bones diff: %i"), WeightsDiff, BonesDiff);
		
		TMap<FName, TArray<int32>> SourceBoneToVertexMapping;
		TMap<FName, TArray<int32>> TargetBoneToVertexMapping;
		const int32 BoneNum = SourceMesh->GetSkeleton()->GetReferenceSkeleton().GetNum();
		for (int32 BoneIndex = 0; BoneIndex < BoneNum; BoneIndex++)
		{
			FName SourceName = SourceMesh->GetSkeleton()->GetReferenceSkeleton().GetBoneName(BoneIndex);
			FName TargetName = TargetMesh->GetSkeleton()->GetReferenceSkeleton().GetBoneName(BoneIndex);
			SourceBoneToVertexMapping.Add(SourceName);
			TargetBoneToVertexMapping.Add(TargetName);

			for(auto SourceWeightVertex : SourceWeightArr)
			{
				for(int32 Iterator = 0; Iterator < 12; Iterator++)
				{
					if(SourceWeightVertex.InfluenceBones[Iterator] == BoneIndex)
					{
						SourceBoneToVertexMapping[SourceName].Add(SourceWeightVertex.InfluenceBones[Iterator]);
					}
				}
			}

			for(auto TargetWeightVertex : TargetWeightArr)
			{
				for(int32 Iterator = 0; Iterator < 12; Iterator++)
				{
					if(TargetWeightVertex.InfluenceBones[Iterator] == BoneIndex)
					{
						TargetBoneToVertexMapping[SourceName].Add(TargetWeightVertex.InfluenceBones[Iterator]);
					}
				}
			}
		}
		check(SourceBoneToVertexMapping.Num() == TargetBoneToVertexMapping.Num());

		/**Compare render section*/
		auto& SourceRenderSection = SourceMesh->GetResourceForRendering()->LODRenderData[0].RenderSections;
		auto& TargetRenderSection = TargetMesh->GetResourceForRendering()->LODRenderData[0].RenderSections;

		check(SourceRenderSection.Num() == TargetRenderSection.Num());
		for(int32 Iterator = 0; Iterator < SourceRenderSection.Num(); Iterator++)
		{
			check(SourceRenderSection[Iterator].BoneMap == TargetRenderSection[Iterator].BoneMap);

			for(int32 InternalIterator = 0; InternalIterator < SourceRenderSection[Iterator].BoneMap.Num(); InternalIterator++)
			{
				if(SourceRenderSection[Iterator].BoneMap[InternalIterator] != TargetRenderSection[Iterator].BoneMap[InternalIterator])
				{
					UE_LOG(LogTemp, Error, TEXT("Diffs in render section. Source is %i, target is %i"), SourceRenderSection[Iterator].BoneMap[InternalIterator],
						TargetRenderSection[Iterator].BoneMap[InternalIterator]);
				}
			}
		}
	}
	
	static void ParseMorphs(USkeletalMesh* Source, FMeshSurface& Surfaces)
	{
		TArray<UMorphTarget*> MorphTargets = Source->GetMorphTargets();
		for(UMorphTarget* MTSource : MorphTargets)
		{
			if(MTSource->GetName() == FString("Size"))
			{
				int32 PopulatedDeltas = 0;
				FMorphTargetDelta* NewDelta = MTSource->GetMorphTargetDelta(0, PopulatedDeltas);
				TArray<FMorphTargetDelta> NewDeltas(NewDelta, PopulatedDeltas);

				for(int Iterator = 0; Iterator < NewDeltas.Num(); Iterator++)
				{
					Surfaces.Vertices[Iterator] += NewDeltas[Iterator].PositionDelta;
				}
				/**Take this vertex
				NewDeltas[0].SourceIdx

				/**Set this position
				+NewDeltas[0].PositionDelta * 1;

				/**Set this z delta
				NewDeltas[0].TangentZDelta*/
			}
			
			/*UMorphTarget* NewMT = NewObject<UMorphTarget>(Target);
			NewMT->BaseSkelMesh = Target;
			
			/**May be memcpy
			int32 PopulatedDeltas = 0;
			FMorphTargetDelta* NewDelta = MTSource->GetMorphTargetDelta(0, PopulatedDeltas);
			TArray<FMorphTargetDelta> NewDeltas(NewDelta, PopulatedDeltas);
			NewDeltas.Add(*NewDelta);
			
			TArray<FSkelMeshSection> Sections;
			for(int32 SectionID : MTSource->MorphLODModels[0].SectionIndices)
			{
				//FSkeletalMeshLODModel;
				Sections.Add(Target->GetImportedModel()->LODModels[0].Sections[SectionID]);
			}
			
			NewMT->PopulateDeltas(NewDeltas, 0, Sections);
			Target->RegisterMorphTarget(NewMT);
			Target->MarkPackageDirty();*/
		}
	}
};
