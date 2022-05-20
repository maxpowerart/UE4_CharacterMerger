#include "CharacterMergerLibrary.h"
#include "CMCharacterMerger.h"
#include "Rendering/SkeletalMeshRenderData.h"

USkeletalMesh* FCharacterMergerLibrary::MergeRequest(const TArray<USkeletalMesh*>& ComponentsToWeld, UPackage* Package)
{
	if (ComponentsToWeld.Num() == 0) return nullptr;

	USkeletalMesh* CompositeMesh = IsValid(Package) ? NewObject<USkeletalMesh>(Package, NAME_None, RF_Public | RF_Standalone) : NewObject<USkeletalMesh>();
	CompositeMesh->SetRefSkeleton(ComponentsToWeld[0]->GetSkeleton()->GetReferenceSkeleton());
	CompositeMesh->SetSkeleton(ComponentsToWeld[0]->GetSkeleton());
		
	TArray<FCMSkelMeshMergeSectionMapping> InForceSectionMapping;
	FCMSkeletalMeshMerge MeshMergeUtil(CompositeMesh, ComponentsToWeld, InForceSectionMapping, 0);
	if (!MeshMergeUtil.DoMerge())
	{
		check(0 && "Something went wrong");
		return nullptr;
	}

	/*for(int32 It = 0; It < ComponentsToWeld.Num(); It++)
	{
		ParseMorphs(ComponentsToWeld[It], CompositeMesh, It);
	}*/

	/**Wait until render thread complete commands*/
	/*FlushRenderingCommands();
	CompositeMesh->ReleaseResources();
	CompositeMesh->InitMorphTargets();
	CompositeMesh->InitResources();*/
	
	return CompositeMesh;
}
