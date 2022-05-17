#include "CharacterMergerCommands.h"

#define LOCTEXT_NAMESPACE "FCharacterMergerModule"

void FCharacterMergerCommands::RegisterCommands()
{
	UI_COMMAND(OpenPluginWindow, "CharacterMerger", "Bring up CharacterMerger window", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(MergeContent, "CharacterMerger", "Merge all content in viewport into asset", EUserInterfaceActionType::Button, FInputGesture());
}
