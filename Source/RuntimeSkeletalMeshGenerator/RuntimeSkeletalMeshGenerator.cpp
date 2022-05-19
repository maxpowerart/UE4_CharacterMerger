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
#include "RuntimeSkeletalMeshGenerator.h"

#include "CharacterMergerEditor/Public/CharacterMergerLibrary.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"

void FRuntimeSkeletalMeshGeneratorModule::StartupModule()
{
}
void FRuntimeSkeletalMeshGeneratorModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FRuntimeSkeletalMeshGeneratorModule, RuntimeSkeletalMeshGenerator)

static void MergeBoneMap(TArray<FBoneIndexType>& MergedBoneMap, TArray<FBoneIndexType>& BoneMapToMergedBoneMap, const TArray<FBoneIndexType>& BoneMap)
{
	BoneMapToMergedBoneMap.AddUninitialized( BoneMap.Num() );
	for( int32 IdxB=0; IdxB < BoneMap.Num(); IdxB++ )
	{
		BoneMapToMergedBoneMap[IdxB] = MergedBoneMap.AddUnique( BoneMap[IdxB] );
	}
}
static void BoneMapToNewRefSkel(const TArray<FBoneIndexType>& InBoneMap, const TArray<int32>& SrcToDestRefSkeletonMap, TArray<FBoneIndexType>& OutBoneMap)
{
	OutBoneMap.Empty();
	OutBoneMap.AddUninitialized(InBoneMap.Num());

	for(int32 i=0; i<InBoneMap.Num(); i++)
	{
		check(InBoneMap[i] < SrcToDestRefSkeletonMap.Num());
		OutBoneMap[i] = SrcToDestRefSkeletonMap[InBoneMap[i]];
	}
}
static void GenerateNewSectionArray(USkeletalMesh* SrcMesh, TArray<int32>& SectionRemapingContainer, TArray<FCMSkeletalMeshMerge::FCMNewSectionInfo>& NewSectionArray,
	int32 LODIdx)
{
	const int32 MaxGPUSkinBones = FGPUBaseSkinVertexFactory::GetMaxGPUSkinBones();

	NewSectionArray.Empty();
	if( SrcMesh )
		{
			FSkeletalMeshRenderData* SrcResource = SrcMesh->GetResourceForRendering();
			int32 SourceLODIdx = FMath::Min(LODIdx, SrcResource->LODRenderData.Num()-1);
			FSkeletalMeshLODRenderData& SrcLODData = SrcResource->LODRenderData[SourceLODIdx];
			FSkeletalMeshLODInfo& SrcLODInfo = *(SrcMesh->GetLODInfo(SourceLODIdx));

			// iterate over each section of this LOD
			for( int32 SectionIdx=0; SectionIdx < SrcLODData.RenderSections.Num(); SectionIdx++ )
			{
				int32 MaterialId = -1;
				FSkelMeshRenderSection& Section = SrcLODData.RenderSections[SectionIdx];

				// Convert Chunk.BoneMap from src to dest bone indices
				TArray<FBoneIndexType> DestChunkBoneMap = Section.BoneMap;
				BoneMapToNewRefSkel(Section.BoneMap, SectionRemapingContainer, DestChunkBoneMap);

				// get the material for this section
				int32 MaterialIndex = Section.MaterialIndex;
				// use the remapping of material indices if there is a valid value
				if(SrcLODInfo.LODMaterialMap.IsValidIndex(SectionIdx) && SrcLODInfo.LODMaterialMap[SectionIdx] != INDEX_NONE && SrcMesh->GetMaterials().Num() > 0)
				{
					MaterialIndex = FMath::Clamp<int32>( SrcLODInfo.LODMaterialMap[SectionIdx], 0, SrcMesh->GetMaterials().Num() - 1);
				}

				const FSkeletalMaterial& SkeletalMaterial = SrcMesh->GetMaterials()[MaterialIndex];
				UMaterialInterface* Material = SkeletalMaterial.MaterialInterface;

				// see if there is an existing entry in the array of new sections that matches its material
				// if there is a match then the source section can be added to its list of sections to merge 
				int32 FoundIdx = INDEX_NONE;
				for( int32 Idx=0; Idx < NewSectionArray.Num(); Idx++ )
				{
					FCMSkeletalMeshMerge::FCMNewSectionInfo& NewSectionInfo = NewSectionArray[Idx];
					// check for a matching material or a matching material index id if it is valid
					if( (MaterialId == -1 && Material == NewSectionInfo.Material) ||
						(MaterialId != -1 && MaterialId == NewSectionInfo.MaterialId) )
					{
						check(NewSectionInfo.MergeSections.Num());

						// merge the bonemap from the source section with the existing merged bonemap
						TArray<FBoneIndexType> TempMergedBoneMap(NewSectionInfo.MergedBoneMap);
						TArray<FBoneIndexType> TempBoneMapToMergedBoneMap;									
						MergeBoneMap(TempMergedBoneMap,TempBoneMapToMergedBoneMap,DestChunkBoneMap);

						// check to see if the newly merged bonemap is still within the bone limit for GPU skinning
						if( TempMergedBoneMap.Num() <= MaxGPUSkinBones )
						{
							TArray<FTransform> SrcUVTransform;
							
							// add the source section as a new merge entry
							FCMSkeletalMeshMerge::FCMMergeSectionInfo& MergeSectionInfo = *new(NewSectionInfo.MergeSections) FCMSkeletalMeshMerge::FCMMergeSectionInfo(
								SrcMesh,
								&SrcLODData.RenderSections[SectionIdx],
								SrcUVTransform
								);
							// keep track of remapping for the existing chunk's bonemap 
							// so that the bone matrix indices can be updated for the vertices
							MergeSectionInfo.BoneMapToMergedBoneMap = TempBoneMapToMergedBoneMap;

							// use the updated bonemap for this new section
							NewSectionInfo.MergedBoneMap = TempMergedBoneMap;

							// keep track of the entry that was found
							FoundIdx = Idx;
							break;
						}
					}
				}

				// new section entries will be created if the material for the source section was not found 
				// or merging it with an existing entry would go over the bone limit for GPU skinning
				if( FoundIdx == INDEX_NONE )
				{
					// create a new section entry
					const FName& MaterialSlotName = SkeletalMaterial.MaterialSlotName;
					const FMeshUVChannelInfo& UVChannelData = SkeletalMaterial.UVChannelData;
					FCMSkeletalMeshMerge::FCMNewSectionInfo& NewSectionInfo = *new(NewSectionArray) FCMSkeletalMeshMerge::FCMNewSectionInfo(Material, MaterialId, MaterialSlotName, UVChannelData);
					// initialize the merged bonemap to simply use the original chunk bonemap
					NewSectionInfo.MergedBoneMap = DestChunkBoneMap;

					TArray<FTransform> SrcUVTransform;
					
					// add a new merge section entry
					FCMSkeletalMeshMerge::FCMMergeSectionInfo& MergeSectionInfo = *new(NewSectionInfo.MergeSections) FCMSkeletalMeshMerge::FCMMergeSectionInfo(
						SrcMesh,
						&SrcLODData.RenderSections[SectionIdx],
						SrcUVTransform);
					// since merged bonemap == chunk.bonemap then remapping is just pass-through
					MergeSectionInfo.BoneMapToMergedBoneMap.Empty( DestChunkBoneMap.Num() );
					for( int32 i=0; i < DestChunkBoneMap.Num(); i++ )
					{
						MergeSectionInfo.BoneMapToMergedBoneMap.Add((FBoneIndexType)i);
					}
				}
			}
		}
}

template <typename VertexDataType>
void CopyVertexFromSource(VertexDataType& DestVert, const FSkeletalMeshLODRenderData& SrcLODData, int32 SourceVertIdx,
	const FCMSkeletalMeshMerge::FCMMergeSectionInfo& MergeSectionInfo)
{
	/**Find vertex right index */
	FVector Offset = FVector::ZeroVector;
	DestVert.Position = SrcLODData.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(SourceVertIdx) + Offset;
	DestVert.TangentX = SrcLODData.StaticVertexBuffers.StaticMeshVertexBuffer.VertexTangentX(SourceVertIdx);
	DestVert.TangentZ = SrcLODData.StaticVertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(SourceVertIdx);

	// Copy all UVs that are available
	uint32 LODNumTexCoords = SrcLODData.StaticVertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords();
	const uint32 ValidLoopCount = FMath::Min(VertexDataType::NumTexCoords, LODNumTexCoords);
	for (uint32 UVIndex = 0; UVIndex < ValidLoopCount; ++UVIndex)
	{
		FVector2D UVs = SrcLODData.StaticVertexBuffers.StaticMeshVertexBuffer.GetVertexUV_Typed<VertexDataType::StaticMeshVertexUVType>(SourceVertIdx, UVIndex);
		if (UVIndex < (uint32)MergeSectionInfo.UVTransforms.Num())
		{
			FVector Transformed = MergeSectionInfo.UVTransforms[UVIndex].TransformPosition(FVector(UVs, 1.f));
			UVs = FVector2D(Transformed.X, Transformed.Y);
		}
		DestVert.UVs[UVIndex] = UVs;
	}
	
	// now just fill up zero value if we didn't reach till end
	for (uint32 UVIndex = ValidLoopCount; UVIndex < VertexDataType::NumTexCoords; ++UVIndex)
	{
		DestVert.UVs[UVIndex] = FVector2D::ZeroVector;
	}
}

template <typename VertexDataType>
static void GenerateLODModel(USkeletalMesh* MergeMesh, USkeletalMesh* SourceMesh, TArray<int32> SectionRemapping, int32 LODIdx)
{
	// add the new LOD model entry
	FSkeletalMeshRenderData* MergeResource = MergeMesh->GetResourceForRendering();
	check(MergeResource);

	FSkeletalMeshLODRenderData& MergeLODData = *new FSkeletalMeshLODRenderData;
	MergeResource->LODRenderData.Add(&MergeLODData);
	// add the new LOD info entry
	FSkeletalMeshLODInfo& MergeLODInfo = MergeMesh->AddLODInfo();
	MergeLODInfo.ScreenSize = MergeLODInfo.LODHysteresis = MAX_FLT;

	// generate an array with info about new sections that need to be created
	TArray<FCMSkeletalMeshMerge::FCMNewSectionInfo> NewSectionArray;
	GenerateNewSectionArray(SourceMesh, SectionRemapping, NewSectionArray, LODIdx );

	uint32 MaxIndex = 0;

	// merged vertex buffer
	TArray< VertexDataType > MergedVertexBuffer;
	// merged skin weight buffer
	TArray< FSkinWeightInfo > MergedSkinWeightBuffer;
	// merged vertex color buffer
	TArray< FColor > MergedColorBuffer;
	// merged index buffer
	TArray<uint32> MergedIndexBuffer;

	// The total number of UV sets for this LOD model
	uint32 TotalNumUVs = 0;

	uint32 SourceMaxBoneInfluences = 0;
	bool bSourceUse16BitBoneIndex = false;

	for( int32 CreateIdx=0; CreateIdx < NewSectionArray.Num(); CreateIdx++ )
	{
		FCMSkeletalMeshMerge::FCMNewSectionInfo& NewSectionInfo = NewSectionArray[CreateIdx];

		// ActiveBoneIndices contains all the bones used by the verts from all the sections of this LOD model
		// Add the bones used by this new section
		for( int32 Idx=0; Idx < NewSectionInfo.MergedBoneMap.Num(); Idx++ )
		{
			MergeLODData.ActiveBoneIndices.AddUnique( NewSectionInfo.MergedBoneMap[Idx] );
		}

		// add the new section entry
		FSkelMeshRenderSection& Section = *new(MergeLODData.RenderSections) FSkelMeshRenderSection;

		// set the new bonemap from the merged sections
		// these are the bones that will be used by this new section
		Section.BoneMap = NewSectionInfo.MergedBoneMap;

		// init vert totals
		Section.NumVertices = 0;

		// keep track of the current base vertex for this section in the merged vertex buffer
		Section.BaseVertexIndex = MergedVertexBuffer.Num();
		
		// init tri totals
		Section.NumTriangles = 0;
		// keep track of the current base index for this section in the merged index buffer
		Section.BaseIndex = MergedIndexBuffer.Num();

		FMeshUVChannelInfo& MergedUVData = MergeMesh->GetMaterials()[Section.MaterialIndex].UVChannelData;

		// iterate over all of the sections that need to be merged together
		for( int32 MergeIdx=0; MergeIdx < NewSectionInfo.MergeSections.Num(); MergeIdx++ )
		{
			FCMSkeletalMeshMerge::FCMMergeSectionInfo& MergeSectionInfo = NewSectionInfo.MergeSections[MergeIdx];
			int32 SourceLODIdx = FMath::Min(LODIdx, MergeSectionInfo.SkelMesh->GetResourceForRendering()->LODRenderData.Num()-1);

			// Take the max UV density for each UVChannel between all sections that are being merged.
			const int32 NewSectionMatId = MergeSectionInfo.Section->MaterialIndex;
			if(MergeSectionInfo.SkelMesh->GetMaterials().IsValidIndex(NewSectionMatId))
			{
				const FMeshUVChannelInfo& NewSectionUVData = MergeSectionInfo.SkelMesh->GetMaterials()[NewSectionMatId].UVChannelData;
				for (int32 i = 0; i < MAX_TEXCOORDS; i++)
				{
					const float NewSectionUVDensity = NewSectionUVData.LocalUVDensities[i];
					float& UVDensity = MergedUVData.LocalUVDensities[i];

					UVDensity = FMath::Max(UVDensity, NewSectionUVDensity);
				}
			}

			// get the source skel LOD info from this merge entry
			const FSkeletalMeshLODInfo& SrcLODInfo = *(MergeSectionInfo.SkelMesh->GetLODInfo(SourceLODIdx));

			// keep track of the lowest LOD displayfactor and hysteresis
			MergeLODInfo.ScreenSize.Default = FMath::Min<float>(MergeLODInfo.ScreenSize.Default, SrcLODInfo.ScreenSize.Default);
#if WITH_EDITORONLY_DATA
			for(const TPair<FName, float>& PerPlatform : SrcLODInfo.ScreenSize.PerPlatform)
			{
				float* Value = MergeLODInfo.ScreenSize.PerPlatform.Find(PerPlatform.Key);
				if(Value)
				{
					*Value = FMath::Min<float>(PerPlatform.Value, *Value);	
				}
				else
				{
					MergeLODInfo.ScreenSize.PerPlatform.Add(PerPlatform.Key, PerPlatform.Value);
				}
			}
#endif
			MergeLODInfo.BuildSettings.bUseFullPrecisionUVs |= SrcLODInfo.BuildSettings.bUseFullPrecisionUVs;
			MergeLODInfo.BuildSettings.bUseHighPrecisionTangentBasis |= SrcLODInfo.BuildSettings.bUseHighPrecisionTangentBasis;

			MergeLODInfo.LODHysteresis = FMath::Min<float>(MergeLODInfo.LODHysteresis,SrcLODInfo.LODHysteresis);

			// get the source skel LOD model from this merge entry
			const FSkeletalMeshLODRenderData& SrcLODData = MergeSectionInfo.SkelMesh->GetResourceForRendering()->LODRenderData[SourceLODIdx];

			// add required bones from this source model entry to the merge model entry
			for( int32 Idx=0; Idx < SrcLODData.RequiredBones.Num(); Idx++ )
			{
				FName SrcLODBoneName = MergeSectionInfo.SkelMesh->GetRefSkeleton().GetBoneName(SrcLODData.RequiredBones[Idx] );
				int32 MergeBoneIndex = MergeMesh->GetRefSkeleton().FindBoneIndex(SrcLODBoneName);
				
				if (MergeBoneIndex != INDEX_NONE)
				{
					MergeLODData.RequiredBones.AddUnique(MergeBoneIndex);
				}
			}

			// update vert total
			Section.NumVertices += MergeSectionInfo.Section->NumVertices;

			// update total number of vertices 
			int32 NumTotalVertices = MergeSectionInfo.Section->NumVertices;

			// add the vertices from the original source mesh to the merged vertex buffer					
			int32 MaxVertIdx = FMath::Min<int32>( 
				MergeSectionInfo.Section->BaseVertexIndex + NumTotalVertices,
				SrcLODData.StaticVertexBuffers.PositionVertexBuffer.GetNumVertices()
				);

			int32 MaxColorIdx = SrcLODData.StaticVertexBuffers.ColorVertexBuffer.GetNumVertices();

			// keep track of the current base vertex index before adding any new vertices
			// this will be needed to remap the index buffer values to the new range
			int32 CurrentBaseVertexIndex = MergedVertexBuffer.Num();
			const uint32 MaxBoneInfluences = SrcLODData.GetSkinWeightVertexBuffer()->GetMaxBoneInfluences();
			const bool bUse16BitBoneIndex = SrcLODData.GetSkinWeightVertexBuffer()->Use16BitBoneIndex();
			for( int32 VertIdx=MergeSectionInfo.Section->BaseVertexIndex; VertIdx < MaxVertIdx; VertIdx++ )
			{
				// add the new vertex
				VertexDataType& DestVert = MergedVertexBuffer[MergedVertexBuffer.AddUninitialized()];
				FSkinWeightInfo& DestWeight = MergedSkinWeightBuffer[MergedSkinWeightBuffer.AddUninitialized()];

				CopyVertexFromSource<VertexDataType>(DestVert, SrcLODData, VertIdx, MergeSectionInfo);

				SourceMaxBoneInfluences = FMath::Max(SourceMaxBoneInfluences, MaxBoneInfluences);
				bSourceUse16BitBoneIndex |= bUse16BitBoneIndex;
				DestWeight = SrcLODData.GetSkinWeightVertexBuffer()->GetVertexSkinWeights(VertIdx);

				// if the mesh uses vertex colors, copy the source color if possible or default to white
				if( MergeMesh->GetHasVertexColors() )
				{
					if( VertIdx < MaxColorIdx )
					{
						const FColor& SrcColor = SrcLODData.StaticVertexBuffers.ColorVertexBuffer.VertexColor(VertIdx);
						MergedColorBuffer.Add(SrcColor);
					}
					else
					{
						const FColor ColorWhite(255, 255, 255);
						MergedColorBuffer.Add(ColorWhite);
					}
				}

				uint32 LODNumTexCoords = SrcLODData.StaticVertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords();
				if( TotalNumUVs < LODNumTexCoords )
				{
					TotalNumUVs = LODNumTexCoords;
				}

				// remap the bone index used by this vertex to match the mergedbonemap 
				for( uint32 Idx=0; Idx < MAX_TOTAL_INFLUENCES; Idx++ )
				{
					if (DestWeight.InfluenceWeights[Idx] > 0)
					{
						checkSlow(MergeSectionInfo.BoneMapToMergedBoneMap.IsValidIndex(DestWeight.InfluenceBones[Idx]));
						DestWeight.InfluenceBones[Idx] = (uint8)MergeSectionInfo.BoneMapToMergedBoneMap[DestWeight.InfluenceBones[Idx]];
					}
				}
			}

			// update total number of triangles
			Section.NumTriangles += MergeSectionInfo.Section->NumTriangles;

			// add the indices from the original source mesh to the merged index buffer					
			int32 MaxIndexIdx = FMath::Min<int32>( 
				MergeSectionInfo.Section->BaseIndex + MergeSectionInfo.Section->NumTriangles * 3, 
				SrcLODData.MultiSizeIndexContainer.GetIndexBuffer()->Num()
				);
            for (int32 IndexIdx = MergeSectionInfo.Section->BaseIndex; IndexIdx < MaxIndexIdx; IndexIdx++)
            {
                uint32 SrcIndex = SrcLODData.MultiSizeIndexContainer.GetIndexBuffer()->Get(IndexIdx);

                // add offset to each index to match the new entries in the merged vertex buffer
                checkSlow(SrcIndex >= MergeSectionInfo.Section->BaseVertexIndex);
                uint32 DstIndex = SrcIndex - MergeSectionInfo.Section->BaseVertexIndex + CurrentBaseVertexIndex;
                checkSlow(DstIndex < (uint32)MergedVertexBuffer.Num());

                // add the new index to the merged vertex buffer
                MergedIndexBuffer.Add(DstIndex);
                if (MaxIndex < DstIndex)
                {
                    MaxIndex = DstIndex;
                }

            }

            {
                if (MergeSectionInfo.Section->DuplicatedVerticesBuffer.bHasOverlappingVertices)
                {
                    if (Section.DuplicatedVerticesBuffer.bHasOverlappingVertices)
                    {
                        // Merge
                        int32 StartIndex = Section.DuplicatedVerticesBuffer.DupVertData.Num();
                        int32 StartVertex = Section.DuplicatedVerticesBuffer.DupVertIndexData.Num();
                        Section.DuplicatedVerticesBuffer.DupVertData.ResizeBuffer(StartIndex + MergeSectionInfo.Section->DuplicatedVerticesBuffer.DupVertData.Num());
                        Section.DuplicatedVerticesBuffer.DupVertIndexData.ResizeBuffer(Section.NumVertices);
                    
                        uint8* VertData = Section.DuplicatedVerticesBuffer.DupVertData.GetDataPointer();
                        uint8* IndexData = Section.DuplicatedVerticesBuffer.DupVertIndexData.GetDataPointer();
                        for (int32 i = StartIndex; i < Section.DuplicatedVerticesBuffer.DupVertData.Num(); ++i)
                        {
                            *((uint32*)(VertData + i * sizeof(uint32))) += CurrentBaseVertexIndex - MergeSectionInfo.Section->BaseVertexIndex;
                        }
                        for (uint32 i = StartVertex; i < Section.NumVertices; ++i)
                        {
                            ((FIndexLengthPair*)(IndexData + i * sizeof(FIndexLengthPair)))->Index += StartIndex;
                        }
                    }
                    else
                    {
                        Section.DuplicatedVerticesBuffer.DupVertData = MergeSectionInfo.Section->DuplicatedVerticesBuffer.DupVertData;
                        Section.DuplicatedVerticesBuffer.DupVertIndexData = MergeSectionInfo.Section->DuplicatedVerticesBuffer.DupVertIndexData;
                        uint8* VertData = Section.DuplicatedVerticesBuffer.DupVertData.GetDataPointer();
                        for (int32 i = 0; i < Section.DuplicatedVerticesBuffer.DupVertData.Num(); ++i)
                        {
                            *((uint32*)(VertData + i * sizeof(uint32))) += CurrentBaseVertexIndex - MergeSectionInfo.Section->BaseVertexIndex;
                        }
                    }
                    Section.DuplicatedVerticesBuffer.bHasOverlappingVertices = true;
                }
                else
                {
                    Section.DuplicatedVerticesBuffer.DupVertData.ResizeBuffer(1);
                    Section.DuplicatedVerticesBuffer.DupVertIndexData.ResizeBuffer(Section.NumVertices);

                    uint8* VertData = Section.DuplicatedVerticesBuffer.DupVertData.GetDataPointer();
                    uint8* IndexData = Section.DuplicatedVerticesBuffer.DupVertIndexData.GetDataPointer();
                    
                    FMemory::Memzero(IndexData, Section.NumVertices * sizeof(FIndexLengthPair));
                    FMemory::Memzero(VertData, sizeof(uint32));
                }
            }
		}
	}

    const bool bNeedsCPUAccess = true;

	// sort required bone array in strictly increasing order
	MergeLODData.RequiredBones.Sort();
	MergeMesh->GetRefSkeleton().EnsureParentsExistAndSort(MergeLODData.ActiveBoneIndices);
	
	// copy the new vertices and indices to the vertex buffer for the new model
	MergeLODData.StaticVertexBuffers.StaticMeshVertexBuffer.SetUseFullPrecisionUVs(MergeLODInfo.BuildSettings.bUseFullPrecisionUVs);

	MergeLODData.StaticVertexBuffers.PositionVertexBuffer.Init(MergedVertexBuffer.Num(), bNeedsCPUAccess);
	MergeLODData.StaticVertexBuffers.StaticMeshVertexBuffer.Init(MergedVertexBuffer.Num(), TotalNumUVs, bNeedsCPUAccess);

	for (int i = 0; i < MergedVertexBuffer.Num(); i++)
	{
		MergeLODData.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(i) = MergedVertexBuffer[i].Position;
		MergeLODData.StaticVertexBuffers.StaticMeshVertexBuffer.SetVertexTangents(i, MergedVertexBuffer[i].TangentX.ToFVector(), MergedVertexBuffer[i].GetTangentY(), MergedVertexBuffer[i].TangentZ.ToFVector());
		for (uint32 j = 0; j < TotalNumUVs; j++)
		{
			MergeLODData.StaticVertexBuffers.StaticMeshVertexBuffer.SetVertexUV(i, j, MergedVertexBuffer[i].UVs[j]);
		}
	}

	MergeLODData.SkinWeightVertexBuffer.SetMaxBoneInfluences(SourceMaxBoneInfluences);
	MergeLODData.SkinWeightVertexBuffer.SetUse16BitBoneIndex(bSourceUse16BitBoneIndex);
	MergeLODData.SkinWeightVertexBuffer.SetNeedsCPUAccess(bNeedsCPUAccess);

	// copy vertex resource arrays
	MergeLODData.SkinWeightVertexBuffer = MergedSkinWeightBuffer;

	if( MergeMesh->GetHasVertexColors() )
	{
		MergeLODData.StaticVertexBuffers.ColorVertexBuffer.InitFromColorArray(MergedColorBuffer);
	}

	
	const uint8 DataTypeSize = (MaxIndex < MAX_uint16) ? sizeof(uint16) : sizeof(uint32);
	MergeLODData.MultiSizeIndexContainer.RebuildIndexBuffer(DataTypeSize, MergedIndexBuffer);
}

static void InitializeSkeleton(USkeletalMesh* MergeMesh, USkeletalMesh* SrcMesh)
{
	FReferenceSkeleton RefSkeleton;

	// Iterate through all the source mesh reference skeletons and compose the merged reference skeleton.
	FReferenceSkeletonModifier RefSkelModifier(RefSkeleton, MergeMesh->GetSkeleton());
	if (!SrcMesh)
	{
		return;
	}

	// Initialise new RefSkeleton with first mesh.
	if (RefSkeleton.GetRawBoneNum() == 0)
	{
		RefSkeleton = SrcMesh->GetRefSkeleton();
	}

	MergeMesh->SetRefSkeleton(RefSkeleton);
	MergeMesh->GetRefBasesInvMatrix().Empty();
	MergeMesh->CalculateInvRefMatrices();
}

void FRuntimeSkeletalMeshGenerator::GenerateSkeletalMesh(
	USkeletalMesh* SkeletalMesh,
	USkeletalMesh* SourceMesh,
	TArray<FMeshSurface>& Surfaces,
	const TArray<UMaterialInterface*>& SurfacesMaterial,
	const TMap<FName, FTransform>& BoneTransformsOverride)
{
	// Waits the rendering thread has done.
	FlushRenderingCommands();

	InitializeSkeleton(SkeletalMesh, SourceMesh);

	constexpr int32 LODIndex = 0;

#if WITH_EDITORONLY_DATA
	FSkeletalMeshImportData ImportedModelData;
#endif

	TArray<uint32> SurfaceVertexOffsets;
	TArray<uint32> SurfaceIndexOffsets;
	SurfaceVertexOffsets.SetNum(Surfaces.Num());
	SurfaceIndexOffsets.SetNum(Surfaces.Num());

	bool bUse16BitBoneIndex = false;
	int32 MaxBoneInfluences = 0;
	const int32 UVCount = Surfaces.Num() > 0 ? (Surfaces[0].Uvs.Num() > 0 ? Surfaces[0].Uvs[0].Num() : 0) : 0;

	// Collect all the vertices and index for each surface.
	TArray<FStaticMeshBuildVertex> StaticVertices;
	TArray<FVector> Vertices;
	TArray<uint32> Indices;
	TArray<uint32> VertexSurfaceIndex;
	{
		int MaxBoneIndex = 0;
		// First count all the vertices.
		uint32 VerticesCount = 0;
		uint32 IndicesCount = 0;
		for (const FMeshSurface& Surface : Surfaces)
		{
			VerticesCount += Surface.Vertices.Num();
			IndicesCount += Surface.Indices.Num();

			for (const auto& Influences : Surface.BoneInfluences)
			{
				MaxBoneInfluences = FMath::Max(Influences.Num(), MaxBoneInfluences);
				for (const auto& Influence : Influences)
				{
					MaxBoneIndex = FMath::Max(Influence.BoneIndex, MaxBoneIndex);
				}
			}

#if WITH_EDITOR
			// Make sure all the surfaces and all the vertices have the same amount of
			// UVs.
			for (const TArray<FVector2D> Uvs : Surface.Uvs)
			{
				check(UVCount == Uvs.Num());
			}
#endif
		}

		bUse16BitBoneIndex = MaxBoneIndex <= MAX_uint16;

		StaticVertices.SetNum(VerticesCount);
		Vertices.SetNum(VerticesCount);
		VertexSurfaceIndex.SetNum(VerticesCount);
		Indices.SetNum(IndicesCount);

		uint32 VerticesOffset = 0;
		uint32 IndicesOffset = 0;
		for (int32 I = 0; I < Surfaces.Num(); I++)
		{
			const FMeshSurface& Surface = Surfaces[I];

			for (int VertexIndex = 0; VertexIndex < Surface.Vertices.Num(); VertexIndex += 1)
			{
				if (Surface.Colors.Num() > 0)
				{
					StaticVertices[VerticesOffset + VertexIndex].Color = Surface.Colors[VertexIndex];
				}
				StaticVertices[VerticesOffset + VertexIndex].Position = Surface.Vertices[VertexIndex];
				StaticVertices[VerticesOffset + VertexIndex].TangentX = Surface.Tangents[VertexIndex];
				StaticVertices[VerticesOffset + VertexIndex].TangentY = FVector::CrossProduct(Surface.Normals[VertexIndex], Surface.Tangents[VertexIndex]) * (Surface.FlipBinormalSigns[VertexIndex] ? -1.0 : 1.0);
				StaticVertices[VerticesOffset + VertexIndex].TangentZ = Surface.Normals[VertexIndex];
				FMemory::Memcpy(
					StaticVertices[VerticesOffset + VertexIndex].UVs,
					Surface.Uvs[VertexIndex].GetData(),
					sizeof(FVector2D) * UVCount);

				VertexSurfaceIndex[VerticesOffset + VertexIndex] = I;
			}

			FMemory::Memcpy(
				Vertices.GetData() + VerticesOffset,
				Surface.Vertices.GetData(),
				sizeof(FVector) * Surface.Vertices.Num());

			// Convert the Indices to Global.
			for (int32 x = 0; x < Surface.Indices.Num(); x++)
			{
				Indices[IndicesOffset + x] = Surface.Indices[x] + VerticesOffset;
			}

			SurfaceVertexOffsets[I] = VerticesOffset;
			VerticesOffset += Surface.Vertices.Num();

			SurfaceIndexOffsets[I] = IndicesOffset;
			IndicesOffset += Surface.Indices.Num();
		}
	}

#if WITH_EDITORONLY_DATA
	// Initialize the `ImportModel` this is used by the editor during reload time.
	ImportedModelData.Points = Vertices;

	// Existing points map 1:1
	ImportedModelData.PointToRawMap.AddUninitialized(ImportedModelData.Points.Num());
	for (int32 i = 0; i < ImportedModelData.Points.Num(); i++)
	{
		ImportedModelData.PointToRawMap[i] = i;
	}
	check(ImportedModelData.PointToRawMap.Num() == Vertices.Num());

	for (int32 I = 0; I < Surfaces.Num(); I++)
	{
		SkeletalMeshImportData::FMaterial& Mat = ImportedModelData.Materials.AddDefaulted_GetRef();
		Mat.Material = SurfacesMaterial[I];
		Mat.MaterialImportName = SurfacesMaterial[I]->GetFullName();
	}

	ImportedModelData.Faces.SetNum(Indices.Num() / 3);
	for (int32 FaceIndex = 0; FaceIndex < ImportedModelData.Faces.Num(); FaceIndex += 1)
	{
		SkeletalMeshImportData::FTriangle& Triangle = ImportedModelData.Faces[FaceIndex];

		const int32 VertexIndex0 = Indices[FaceIndex * 3 + 0];
		const int32 VertexIndex1 = Indices[FaceIndex * 3 + 1];
		const int32 VertexIndex2 = Indices[FaceIndex * 3 + 2];
		Triangle.WedgeIndex[0] = FaceIndex * 3 + 0;
		Triangle.WedgeIndex[1] = FaceIndex * 3 + 1;
		Triangle.WedgeIndex[2] = FaceIndex * 3 + 2;

		Triangle.TangentX[0] = StaticVertices[VertexIndex0].TangentX;
		Triangle.TangentY[0] = StaticVertices[VertexIndex0].TangentY;
		Triangle.TangentZ[0] = StaticVertices[VertexIndex0].TangentZ;

		Triangle.TangentX[1] = StaticVertices[VertexIndex1].TangentX;
		Triangle.TangentY[1] = StaticVertices[VertexIndex1].TangentY;
		Triangle.TangentZ[1] = StaticVertices[VertexIndex1].TangentZ;

		Triangle.TangentX[2] = StaticVertices[VertexIndex2].TangentX;
		Triangle.TangentY[2] = StaticVertices[VertexIndex2].TangentY;
		Triangle.TangentZ[2] = StaticVertices[VertexIndex2].TangentZ;

		Triangle.MatIndex = VertexSurfaceIndex[VertexIndex0];
		Triangle.AuxMatIndex = 0;
		Triangle.SmoothingGroups = 1; // TODO Calculate the smoothing group correctly, otherwise everything will be smooth
	}

	ImportedModelData.Wedges.SetNum(ImportedModelData.Faces.Num() * 3);
	for (int32 FaceIndex = 0; FaceIndex < ImportedModelData.Faces.Num(); FaceIndex += 1)
	{
		for (int32 i = 0; i < 3; i += 1)
		{
			const int32 WedgeIndex = FaceIndex * 3 + i;
			const int32 VertexIndex = Indices[WedgeIndex];

			ImportedModelData.Wedges[WedgeIndex].VertexIndex = VertexIndex;
			for (int32 UVIndex = 0; UVIndex < FMath::Min(MAX_TEXCOORDS, MAX_STATIC_TEXCOORDS); UVIndex += 1)
			{
				ImportedModelData.Wedges[WedgeIndex].UVs[UVIndex] = StaticVertices[VertexIndex].UVs[UVIndex];
			}
			ImportedModelData.Wedges[WedgeIndex].MatIndex = VertexSurfaceIndex[VertexIndex];
			ImportedModelData.Wedges[WedgeIndex].Color = StaticVertices[VertexIndex].Color;
			ImportedModelData.Wedges[WedgeIndex].Reserved = 0;
		}
	}

	{
		const int32 BoneNum = SkeletalMesh->Skeleton->GetReferenceSkeleton().GetNum();
		SkeletalMeshImportData::FBone DefaultBone;
		DefaultBone.Name = FString(TEXT(""));
		DefaultBone.Flags = 0;
		DefaultBone.NumChildren = 0;
		DefaultBone.ParentIndex = INDEX_NONE;
		DefaultBone.BonePos.Transform.SetIdentity();
		DefaultBone.BonePos.Length = 0.0;
		DefaultBone.BonePos.XSize = 1.0;
		DefaultBone.BonePos.YSize = 1.0;
		DefaultBone.BonePos.ZSize = 1.0;
		ImportedModelData.RefBonesBinary.Init(DefaultBone, BoneNum);
		for (int32 i = 0; i < BoneNum; i += 1)
		{
			ImportedModelData.RefBonesBinary[i].Name = SkeletalMesh->Skeleton->GetReferenceSkeleton().GetBoneName(i).ToString();
			ImportedModelData.RefBonesBinary[i].ParentIndex = SkeletalMesh->Skeleton->GetReferenceSkeleton().GetParentIndex(i);
		}

		// At this point it's sure all the bones are initialized, finish the process
		// by setting the local transform.
		for (int32 i = 0; i < BoneNum; i += 1)
		{
			// Relative to its parent.
			const FTransform* TransformOverride = BoneTransformsOverride.Find(SkeletalMesh->Skeleton->GetReferenceSkeleton().GetBoneName(i));
			if (TransformOverride != nullptr)
			{
				ImportedModelData.RefBonesBinary[i].BonePos.Transform = *TransformOverride;
			}
			else
			{
				ImportedModelData.RefBonesBinary[i].BonePos.Transform = SkeletalMesh->Skeleton->GetReferenceSkeleton().GetRawRefBonePose()[i];
			}
			// Set the Bone Length.
			ImportedModelData.RefBonesBinary[i].BonePos.Length = ImportedModelData.RefBonesBinary[i].BonePos.Transform.GetLocation().Size();
		}
	}
#endif

	// Unreal doesn't support more than `MAX_TOTAL_INFLUENCES` BoneInfluences.
	check(MaxBoneInfluences <= MAX_TOTAL_INFLUENCES);

	// Unreal doesn't support more than `MAX_STATIC_TEXCOORDS`.
	check(UVCount <= MAX_STATIC_TEXCOORDS);

	// Populate Arrays step.
	SkeletalMesh->AllocateResourceForRendering();
	FSkeletalMeshRenderData* MeshRenderData = SkeletalMesh->GetResourceForRendering();
	if (MeshRenderData)
	{
		MeshRenderData->LODRenderData.Empty(1);
	}

	SkeletalMesh->ResetLODInfo();
	SkeletalMesh->GetMaterials().Empty();

	// Set the default Material.
	for (auto Material : SurfacesMaterial)
	{
		SkeletalMesh->GetMaterials().Add(Material);
	}

	/**Create remap*/
	TArray<int32> RemapingBones;
	if (SkeletalMesh)
	{
		RemapingBones.AddUninitialized(SkeletalMesh->GetRefSkeleton().GetRawBoneNum());

		for (int32 i = 0; i < SourceMesh->GetRefSkeleton().GetRawBoneNum(); i++)
		{
			FName SrcBoneName = SourceMesh->GetRefSkeleton().GetBoneName(i);
			int32 DestBoneIndex = SkeletalMesh->GetRefSkeleton().FindBoneIndex(SrcBoneName);

			if (DestBoneIndex == INDEX_NONE)
			{
				// Missing bones shouldn't be possible, but can happen with invalid meshes;
				// map any bone we are missing to the 'root'.

				DestBoneIndex = 0;
			}

			RemapingBones[i] = DestBoneIndex;
		}
	}

	// Array of per-lod number of UV sets
	TArray<uint32> PerLODNumUVSets;
	TArray<uint32> PerLODMaxBoneInfluences;
	TArray<bool> PerLODUse16BitBoneIndex;
	PerLODNumUVSets.AddZeroed(1);
	PerLODMaxBoneInfluences.AddZeroed(1);
	PerLODUse16BitBoneIndex.AddZeroed(1);

	// Get the number of UV sets for each LOD.
	FSkeletalMeshRenderData* SrcResource = SourceMesh->GetResourceForRendering();
	for (int32 LODIdx = 0; LODIdx < 1; LODIdx++)
	{
		if (SrcResource->LODRenderData.IsValidIndex(LODIdx))
		{
			uint32& NumUVSets = PerLODNumUVSets[LODIdx];
			NumUVSets = FMath::Max(NumUVSets, SrcResource->LODRenderData[LODIdx].GetNumTexCoords());

			PerLODMaxBoneInfluences[LODIdx] = FMath::Max(PerLODMaxBoneInfluences[LODIdx], SrcResource->LODRenderData[LODIdx].GetVertexBufferMaxBoneInfluences());
			PerLODUse16BitBoneIndex[LODIdx] |= SrcResource->LODRenderData[LODIdx].DoesVertexBufferUse16BitBoneIndex();
		}
	}

	// process each LOD for the new merged mesh
	SkeletalMesh->AllocateResourceForRendering();
	for (int32 LODIdx = 0; LODIdx < 1; LODIdx++)
	{
		const FSkeletalMeshLODInfo* LODInfoPtr = SkeletalMesh->GetLODInfo(LODIdx);
		bool bUseFullPrecisionUVs = LODInfoPtr ? LODInfoPtr->BuildSettings.bUseFullPrecisionUVs : GVertexElementTypeSupport.IsSupported(VET_Half2) ? false : true;
		if (!bUseFullPrecisionUVs)
		{
			switch( PerLODNumUVSets[0])
			{
			case 1:
				GenerateLODModel< TGPUSkinVertexFloat16Uvs<1> >( SkeletalMesh, SourceMesh, RemapingBones, LODIdx );
				break;
			case 2:
				GenerateLODModel< TGPUSkinVertexFloat16Uvs<2> >( SkeletalMesh, SourceMesh, RemapingBones, LODIdx );
				break;
			case 3:
				GenerateLODModel< TGPUSkinVertexFloat16Uvs<3> >( SkeletalMesh, SourceMesh, RemapingBones, LODIdx );
				break;
			case 4:
				GenerateLODModel< TGPUSkinVertexFloat16Uvs<4> >( SkeletalMesh, SourceMesh, RemapingBones, LODIdx );
				break;
			default:
				checkf(false, TEXT("Invalid number of UV sets.  Must be between 0 and 4") );
				break;
			}
		}
		else
		{
			switch( PerLODNumUVSets[0])
			{
			case 1:
				GenerateLODModel< TGPUSkinVertexFloat32Uvs<1> >( SkeletalMesh, SourceMesh, RemapingBones, LODIdx );
				break;
			case 2:
				GenerateLODModel< TGPUSkinVertexFloat32Uvs<2> >( SkeletalMesh, SourceMesh, RemapingBones, LODIdx );
				break;
			case 3:
				GenerateLODModel< TGPUSkinVertexFloat32Uvs<3> >( SkeletalMesh, SourceMesh, RemapingBones, LODIdx );
				break;
			case 4:
				GenerateLODModel< TGPUSkinVertexFloat32Uvs<4> >( SkeletalMesh, SourceMesh, RemapingBones, LODIdx );
				break;
			default:
				checkf(false, TEXT("Invalid number of UV sets.  Must be between 0 and 4") );
				break;
			}
		}
	}

	SkeletalMesh->GetSkelMirrorTable().Empty();
	if(SourceMesh)
	{
		// initialize the merged mesh with the first src mesh entry used
		SkeletalMesh->SetImportedBounds(SourceMesh->GetImportedBounds());

		SkeletalMesh->SetSkelMirrorAxis(SourceMesh->GetSkelMirrorAxis());
		SkeletalMesh->SetSkelMirrorFlipAxis(SourceMesh->GetSkelMirrorFlipAxis());
	}

	// Rebuild inverse ref pose matrices.
	SkeletalMesh->GetRefBasesInvMatrix().Empty();
	SkeletalMesh->CalculateInvRefMatrices();
	
	// If in game, streaming must be disabled as there are no files to stream from.
	// In editor, the engine can stream from the DDC and create the required files on cook.
	if (!GIsEditor)
	{
		SkeletalMesh->NeverStream = true;
	}

	// Reinitialize the mesh's render resources.
	SkeletalMesh->InitResources();
}

bool FRuntimeSkeletalMeshGenerator::DecomposeSkeletalMesh(
	/// The `SkeletalMesh` to decompose
	const USkeletalMesh* SkeletalMesh,
	/// Out Surfaces.
	TArray<FMeshSurface>& OutSurfaces,
	/// The vertex offsets for each surface, relative to the passed `SkeletalMesh`
	TArray<int32>& OutSurfacesVertexOffsets,
	/// The index offsets for each surface, relative to the passed `SkeletalMesh`
	TArray<int32>& OutSurfacesIndexOffsets,
	/// Out Materials used.
	TArray<UMaterialInterface*>& OutSurfacesMaterial)
{
	OutSurfaces.Empty();
	OutSurfacesVertexOffsets.Empty();
	OutSurfacesIndexOffsets.Empty();
	OutSurfacesMaterial.Empty();

	// Assume only the LOD 0 exist.
	constexpr int32 LODIndex = 0;

	// Extract the Surfaces from the `SourceMesh`
	const int32 RenderSectionsNum = SkeletalMesh->GetResourceForRendering()->LODRenderData[LODIndex].RenderSections.Num();;

	OutSurfaces.SetNum(RenderSectionsNum);
	OutSurfacesVertexOffsets.SetNum(RenderSectionsNum);
	OutSurfacesIndexOffsets.SetNum(RenderSectionsNum);

	const FSkeletalMeshLODRenderData& RenderData = SkeletalMesh->GetResourceForRendering()->LODRenderData[LODIndex];

	TArray<uint32> IndexBuffer;
	RenderData.MultiSizeIndexContainer.GetIndexBuffer(IndexBuffer);

	for (int32 SectionIndex = 0; SectionIndex < RenderSectionsNum; SectionIndex += 1)
	{
		FMeshSurface& Surface = OutSurfaces[SectionIndex];

		const FSkelMeshRenderSection& RenderSection = RenderData.RenderSections[SectionIndex];

		const uint32 VertexIndexOffset = RenderSection.BaseVertexIndex;
		const uint32 VertexNum = RenderSection.NumVertices;

		OutSurfacesVertexOffsets[SectionIndex] = VertexIndexOffset;

		Surface.Vertices.SetNum(VertexNum);
		Surface.Normals.SetNum(VertexNum);
		Surface.Tangents.SetNum(VertexNum);
		Surface.FlipBinormalSigns.SetNum(VertexNum);
		Surface.Uvs.SetNum(VertexNum);
		Surface.Colors.SetNum(VertexNum);
		Surface.BoneInfluences.SetNum(VertexNum);

		for (uint32 i = 0; i < VertexNum; i += 1)
		{
			const uint32 VertexIndex = VertexIndexOffset + i;

			Surface.Vertices[i] = RenderData.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(VertexIndex);

			Surface.Normals[i] = RenderData.StaticVertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(VertexIndex);
			Surface.Tangents[i] = RenderData.StaticVertexBuffers.StaticMeshVertexBuffer.VertexTangentX(VertexIndex);
			// Check if the Binormal Sign is flipped.
			const FVector& ActualBinormal = RenderData.StaticVertexBuffers.StaticMeshVertexBuffer.VertexTangentY(VertexIndex);
			const FVector CalculatedBinormal = FVector::CrossProduct(Surface.Normals[i], Surface.Tangents[i]);
			// If the Binormal points toward different location, the `FlipBinormalSign`
			// must be `false`. Check how the `VertexTangentY` is computed above.
			Surface.FlipBinormalSigns[i] = FVector::DotProduct(ActualBinormal, CalculatedBinormal) < 0.99;

			Surface.Uvs[i].SetNum(RenderData.StaticVertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords());
			for (uint32 UVIndex = 0; UVIndex < RenderData.StaticVertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords(); UVIndex += 1)
			{
				Surface.Uvs[i][UVIndex] = RenderData.StaticVertexBuffers.StaticMeshVertexBuffer.GetVertexUV(VertexIndex, UVIndex);
			}

			if (VertexIndex < RenderData.StaticVertexBuffers.ColorVertexBuffer.GetNumVertices())
			{
				// Make sure the color is set. Not all meshes have vertex colors.
				Surface.Colors[i] = RenderData.StaticVertexBuffers.ColorVertexBuffer.VertexColor(VertexIndex);
			}

			checkf(int32(RenderData.SkinWeightVertexBuffer.GetMaxBoneInfluences()) >= RenderSection.MaxBoneInfluences, TEXT("These two MUST be the same."));

			Surface.BoneInfluences[i].SetNum(RenderSection.MaxBoneInfluences);
			for (
				int32 BoneInfluenceIndex = 0;
				BoneInfluenceIndex < RenderSection.MaxBoneInfluences;
				BoneInfluenceIndex += 1)
			{
				Surface.BoneInfluences[i][BoneInfluenceIndex].VertexIndex = i;
				Surface.BoneInfluences[i][BoneInfluenceIndex].BoneIndex =
					RenderData.SkinWeightVertexBuffer.GetBoneIndex(VertexIndex, BoneInfluenceIndex);
				Surface.BoneInfluences[i][BoneInfluenceIndex].Weight =
					FMath::Clamp(static_cast<float>(RenderData.SkinWeightVertexBuffer.GetBoneWeight(VertexIndex, BoneInfluenceIndex)) / 255.0, 0.0, 1.0);
			}
		}

		const uint32 IndexIndexOffset = RenderSection.BaseIndex;
		const uint32 IndexCount = RenderSection.NumTriangles * 3;

		OutSurfacesIndexOffsets[SectionIndex] = IndexIndexOffset;
		Surface.Indices.SetNum(IndexCount);

		for (uint32 i = 0; i < IndexCount; i += 1)
		{
			const uint32 Index = i + IndexIndexOffset;
			// Note: Subtracting the `VertexIndexOffset` to obtain the index relative
			// on the surface.
			Surface.Indices[i] = IndexBuffer[Index] - VertexIndexOffset;
		}
	}

	OutSurfacesMaterial.Reserve(SkeletalMesh->Materials.Num());
	for (const FSkeletalMaterial& Material : SkeletalMesh->Materials)
	{
		OutSurfacesMaterial.Add(Material.MaterialInterface);
	}

	return true;
}
