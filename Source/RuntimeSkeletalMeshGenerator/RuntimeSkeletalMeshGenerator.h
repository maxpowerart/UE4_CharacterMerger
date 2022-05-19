/******************************************************************************/
/* SkeletalMeshComponent Generator for UE4.27                                 */
/* -------------------------------------------------------------------------- */
/* License MIT                                                                */
/* Kindly sponsored by IMVU                                                   */
/* -------------------------------------------------------------------------- */
/* This is a header only library that simplify the process of creating a      */
/* `USkeletalMeshComponent`, with many surfaces, at runtime.                  */
/* You can just pass all the surfaces' data, this library will take care to   */
/* correctly populate the UE4 buffers, needed to have a fully working         */
/* `USkeletalMeshComponent`.                                                  */
/******************************************************************************/
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Rendering/SkeletalMeshLODImporterData.h"
#include "Rendering/SkeletalMeshRenderData.h"

/**
 * Struct for BoneInfluences.
 */
struct FRawBoneInfluence
{
	float Weight;
	int32 VertexIndex;
	int32 BoneIndex;
};

/**
 * This structure contains all the mesh surface info.
 */
struct FMeshSurface
{
	TArray<FVector> Vertices;
	TArray<FVector> Tangents;
	TArray<bool> FlipBinormalSigns;
	TArray<FVector> Normals;
	TArray<FColor> Colors;
	TArray<TArray<FVector2D>> Uvs;
	TArray<TArray<FRawBoneInfluence>> BoneInfluences;
	TArray<uint32> Indices;
};

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

class FRuntimeSkeletalMeshGeneratorModule : public IModuleInterface
{
public: // ------------------------------------- IModuleInterface implementation
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

class FRuntimeSkeletalMeshGenerator
{
public: // ----------------------------------------------------------------- API
	/**
	 * Generate the `SkeletalMesh` for the given surfaces.
	 */
	static void GenerateSkeletalMesh(
		USkeletalMesh* SkeletalMesh,
		USkeletalMesh* SourceMesh,
		TArray<FMeshSurface>& Surfaces,
		const TArray<UMaterialInterface*>& SurfacesMaterial,
		const TMap<FName, FTransform>& BoneTransformsOverride = TMap<FName, FTransform>());

	/**
	 * Decompose the `USkeletalMesh` in `Surfaces`.
	 */
	static bool DecomposeSkeletalMesh(
		/// The `SkeletalMesh` to decompose
		const USkeletalMesh* SkeletalMesh,
		/// Out Surfaces.
		TArray<FMeshSurface>& OutSurfaces,
		/// Out morphs
		TMap<FName, TArray<FMorphTargetDelta>>& OutMorphMap,
		/// The vertex offsets for each surface, relative to the passed `SkeletalMesh`
		TArray<int32>& OutSurfacesVertexOffsets,
		/// The index offsets for each surface, relative to the passed `SkeletalMesh`
		TArray<int32>& OutSurfacesIndexOffsets,
		/// Out Materials used.
		TArray<UMaterialInterface*>& OutSurfacesMaterial);

private:
	/** 
	* Info to map all the sections from a single source skeletal mesh to 
	* a final section entry int he merged skeletal mesh
	*/
	struct FCMSkelMeshMergeSectionMapping
	{
		/** indices to final section entries of the merged skel mesh */
		TArray<int32> SectionIDs;
	};
	/* Info to map all the sections about how to transform their UVs */
	struct FCMSkelMeshMergeUVTransforms
	{
		/** For each UV channel on each mesh, how the UVS should be transformed. */
		TArray<TArray<FTransform>> UVTransformsPerMesh;
	};
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

	template <typename VertexDataType>
	static void GenerateLODModel(USkeletalMesh* MergeMesh, USkeletalMesh* SourceMesh, const TArray<FMeshSurface>& Surfaces, TArray<int32> SectionRemapping, int32 LODIdx);

	static void MergeBoneMap(TArray<FBoneIndexType>& MergedBoneMap, TArray<FBoneIndexType>& BoneMapToMergedBoneMap, const TArray<FBoneIndexType>& BoneMap);
	static void BoneMapToNewRefSkel(const TArray<FBoneIndexType>& InBoneMap, const TArray<int32>& SrcToDestRefSkeletonMap, TArray<FBoneIndexType>& OutBoneMap);
	static void GenerateNewSectionArray(USkeletalMesh* SrcMesh, TArray<int32>& SectionRemapingContainer, TArray<FCMNewSectionInfo>& NewSectionArray, int32 LODIdx);

	template <typename VertexDataType>
	static void CopyVertexFromSource(VertexDataType& DestVert, const FSkeletalMeshLODRenderData& SrcLODData, const TArray<FMeshSurface>& Surfaces, int32 SourceVertIdx,
		const FCMMergeSectionInfo& MergeSectionInfo);
	
	static void InitializeSkeleton(USkeletalMesh* MergeMesh, USkeletalMesh* SrcMesh);
};
