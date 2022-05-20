#pragma once

#include "CoreMinimal.h"

class CHARACTERMERGER_API FCharacterMergerLibrary
{
public:
	static USkeletalMesh* MergeRequest(const TArray<USkeletalMesh*>& ComponentsToWeld, UPackage* Package = nullptr);
};
