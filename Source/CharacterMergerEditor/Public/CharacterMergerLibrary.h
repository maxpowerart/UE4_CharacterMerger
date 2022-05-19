#pragma once

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
		UPackage* Package = CreatePackage( *SaveName);
		
		TArray<USkeletalMesh*> MorphedMeshes;
		TMap<FName,TArray<FMorphTargetDelta>> MorphTargets;
		for(USkeletalMesh* MeshToDecompose : ComponentsToWeld)
		{
			USkeletalMesh* RecomposedMesh = NewObject<USkeletalMesh>();
			RecomposedMesh->PreEditChange(NULL);
			RecomposedMesh->SetRefSkeleton(MeshToDecompose->GetSkeleton()->GetReferenceSkeleton());
			RecomposedMesh->SetSkeleton(MeshToDecompose->GetSkeleton());
			
			/**Decompose meshes*/
			TArray<FMeshSurface> Surfaces;
			TArray<int32> SurfacesVertexOffsets;
			TArray<int32> SurfacesIndexOffsets;
			TArray<UMaterialInterface*> SurfacesMaterial;
			FRuntimeSkeletalMeshGenerator::DecomposeSkeletalMesh(MeshToDecompose, Surfaces,MorphTargets, SurfacesVertexOffsets, SurfacesIndexOffsets, SurfacesMaterial);
			FRuntimeSkeletalMeshGenerator::GenerateSkeletalMesh(RecomposedMesh, MeshToDecompose, Surfaces, SurfacesMaterial);
			MorphedMeshes.Add(RecomposedMesh);
		}

		USkeletalMesh* CompositeMesh = NewObject<USkeletalMesh>(Package, NAME_None, RF_Public | RF_Standalone);
		CompositeMesh->SetRefSkeleton(ComponentsToWeld[0]->GetSkeleton()->GetReferenceSkeleton());
		CompositeMesh->SetSkeleton(ComponentsToWeld[0]->GetSkeleton());
		
		TArray<FSkelMeshMergeSectionMapping> InForceSectionMapping;
		FSkeletalMeshMerge MeshMergeUtil(CompositeMesh, MorphedMeshes, InForceSectionMapping, 0);
		if (!MeshMergeUtil.DoMerge())
		{
			return;
		}
		
		ParseMorphs(CompositeMesh, MorphTargets);

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


	/** compare based on base mesh source vertex indices */
	struct FCompareMorphTargetDeltas
	{
		FORCEINLINE bool operator()( const FMorphTargetDelta& A, const FMorphTargetDelta& B ) const
		{
			return ((int32)A.SourceIdx - (int32)B.SourceIdx) < 0 ? true : false;
		}
	};
	
	static void ParseMorphs(USkeletalMesh* Target, TMap<FName,TArray<FMorphTargetDelta>>& MorphTargets)
	{
		FSkeletalMeshLODRenderData& RenderData = Target->GetResourceForRendering()->LODRenderData[0];

		//RenderData.MorphTargetVertexInfoBuffers.
		/**Create new morph target objects*/
		TArray<UMorphTarget*> MorphTargetObjects;
		for(const TTuple<FName, TArray<FMorphTargetDelta>>& MorphTarget : MorphTargets)
		{
			UMorphTarget* NewMorphTarget = NewObject<UMorphTarget>(Target, MorphTarget.Key);
			NewMorphTarget->MorphLODModels.AddDefaulted(1);

			// morph mesh data to modify
			FMorphTargetLODModel& MorphModel = NewMorphTarget->MorphLODModels[0];
			// copy the wedge point indices
			// for now just keep every thing 

			// set the original number of vertices
			MorphModel.NumBaseMeshVerts = MorphTarget.Value.Num();
			// empty morph mesh vertices first
			MorphModel.Vertices.Empty(MorphTarget.Value.Num());

			// mark if generated by reduction setting, so that we can remove them later if we want to
			// we don't want to delete if it has been imported
			MorphModel.bGeneratedByEngine = true;

			// Still keep this (could remove in long term due to incoming data)
			for (const FMorphTargetDelta& Delta : MorphTarget.Value)
			{
				if (Delta.PositionDelta.SizeSquared() > FMath::Square(THRESH_POINTS_ARE_NEAR))
				{
					MorphModel.Vertices.Add(Delta);
					for (int32 SectionIdx = 0; SectionIdx < RenderData.RenderSections.Num(); ++SectionIdx)
					{
						if (MorphModel.SectionIndices.Contains(SectionIdx))
						{
							continue;
						}
						
						/**Vertex mapping, if this is not a first section -> move it to NewSection.first value*/
						const uint32 BaseVertexBufferIndex = (uint32)(RenderData.RenderSections[SectionIdx].GetVertexBufferIndex());
						const uint32 LastVertexBufferIndex = (uint32)(BaseVertexBufferIndex + RenderData.RenderSections[SectionIdx].GetNumVertices());
						if (BaseVertexBufferIndex <= Delta.SourceIdx && Delta.SourceIdx < LastVertexBufferIndex)
						{
							MorphModel.SectionIndices.AddUnique(SectionIdx);
							break;
						}
					}
				}
			}

			// sort the array of vertices for this morph target based on the base mesh indices
			// that each vertex is associated with. This allows us to sequentially traverse the list
			// when applying the morph blends to each vertex.
			MorphModel.Vertices.Sort(FCompareMorphTargetDeltas());

			// remove array slack
			MorphModel.Vertices.Shrink();

			MorphTargetObjects.Add(NewMorphTarget);
		}
		Target->SetMorphTargets(MorphTargetObjects);
		Target->InitMorphTargets();
		Target->GetResourceForRendering()->ReleaseResources();
		Target->InitResources();
	}
};
