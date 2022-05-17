// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

static TSharedPtr<FTabManager> CharacterMergerTabManager;
class FCharacterMergerCommands : public TCommands<FCharacterMergerCommands>
{
public:

	FCharacterMergerCommands()
		: TCommands<FCharacterMergerCommands>(TEXT("CharacterMergerCommangs"), NSLOCTEXT("Contexts", "CharacterMerger", "CharacterMerger Plugin"), NAME_None, FEditorStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenPluginWindow;
	TSharedPtr< FUICommandInfo > MergeContent;
};