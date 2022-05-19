#pragma once

#include "CoreMinimal.h"
#include "Animation/MorphTarget.h"

class CHARACTERMERGER_API FCharacterMergerLibrary
{
public:
	
	static USkeletalMesh* MergeRequest(const TArray<USkeletalMesh*>& ComponentsToWeld, UPackage* Package = nullptr);
	static void ParseMorphs(USkeletalMesh* Source, USkeletalMesh* Target, int32 SourceInTargetSection = 0);

private:
	/** compare based on base mesh source vertex indices */
	struct FCompareMorphTargetDeltas
	{
		FORCEINLINE bool operator()( const FMorphTargetDelta& A, const FMorphTargetDelta& B ) const
		{
			return ((int32)A.SourceIdx - (int32)B.SourceIdx) < 0 ? true : false;
		}
	};
};
