// Copyright Epic Games, Inc. All Rights Reserved.

#include "CharacterMergerEditor.h"

#include "CharacterMergerCommands.h"
#include "CharacterMergerLibrary.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "LevelEditor.h"
#include "Viewport/SCharacterMergerViewport.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "FCharacterMergerModule"

static const FName CharacterMergerTabName("CharacterMerger");

void FCharacterMergerEditorModule::StartupModule()
{
	/**Register toolbar click*/
	FCharacterMergerCommands::Register();
	PluginCommands = MakeShareable(new FUICommandList);
	
	PluginCommands->MapAction(FCharacterMergerCommands::Get().OpenPluginWindow,
		FExecuteAction::CreateRaw(this, &FCharacterMergerEditorModule::PluginButtonClicked),
		FCanExecuteAction());

	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	{
		TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());
		MenuExtender->AddMenuExtension("WindowLayout", EExtensionHook::First, PluginCommands, FMenuExtensionDelegate::CreateRaw(this, &FCharacterMergerEditorModule::AddMenuExtension));

		LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
	}
	{
		TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
		ToolbarExtender->AddToolBarExtension("Settings", EExtensionHook::After, PluginCommands, FToolBarExtensionDelegate::CreateRaw(this, &FCharacterMergerEditorModule::AddToolbarExtension));
		
		LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);
	}
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(CharacterMergerTabName, FOnSpawnTab::CreateRaw(this, &FCharacterMergerEditorModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("FGMergeUtilsTabTitle", "GMergeUtils"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}
void FCharacterMergerEditorModule::ShutdownModule()
{
	FCharacterMergerCommands::Unregister();
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(CharacterMergerTabName);
}

void FCharacterMergerEditorModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(CharacterMergerTabName);
}

FReply FCharacterMergerEditorModule::CompareRig()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	TArray<FAssetData> SelectedAssets;
	ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);
	auto Source = Cast<USkeletalMesh>(SelectedAssets[0].GetAsset());
	auto Target = Cast<USkeletalMesh>(SelectedAssets[1].GetAsset());
	FCharacterMergerLibrary::CompareMeshRigging(Source, Target);
	return FReply::Handled();
}

FReply FCharacterMergerEditorModule::OnMergeRequested()
{
	TArray<USkeletalMesh*> Components = ViewportPtr->GetMeshes();
	if(Components.Num() >= 2)
	{
		FCharacterMergerLibrary::MergeRequest(Components);
	}
	return FReply::Handled();
}

void FCharacterMergerEditorModule::OnExistRequest()
{
	if(ViewportPtr)
	{
		ViewportPtr.Reset();
		GEngine->OnEditorClose().RemoveAll(this);
	}
}
void FCharacterMergerEditorModule::AddToolbarExtension(FToolBarBuilder& Builder)
{
	Builder.AddToolBarButton(FCharacterMergerCommands::Get().OpenPluginWindow);
}
void FCharacterMergerEditorModule::AddMenuExtension(FMenuBuilder& Builder)
{
	Builder.AddMenuEntry(FCharacterMergerCommands::Get().OpenPluginWindow);
}

TSharedRef<SDockTab> FCharacterMergerEditorModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	ViewportPtr = TSharedPtr<SCharacterMergerViewport>(SNew(SCharacterMergerViewport));

	TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout( "CharacterMergerLayout_v1" )
	->AddArea
	(
		FTabManager::NewPrimaryArea()
		->SetOrientation(Orient_Vertical)
		->Split
		(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.1f)
			->AddTab("Toolbar", ETabState::OpenedTab) 
			->SetHideTabWell(true) 
		)
		->Split
		(
			FTabManager::NewSplitter()
			->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.7f)
				->AddTab("Hierarchy", ETabState::OpenedTab)
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.7f)
				->AddTab("Viewport", ETabState::OpenedTab)
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.7f)
				->AddTab("Details", ETabState::OpenedTab)
			)
		)
	);

	/**TODO build blocks*/
	
	TSharedRef<SDockTab> CharacterMergerTab =
		SNew(SDockTab)
		. TabRole( ETabRole::MajorTab )
		. Label( LOCTEXT("CharacterMergerTab", "Character merger") )
		. ToolTipText( LOCTEXT( "CharacterMergerTabTooltip", "Character Merger" ) );

	GEngine->OnEditorClose().AddRaw(this, &FCharacterMergerEditorModule::OnExistRequest);

	CharacterMergerTabManager = FGlobalTabmanager::Get()->NewTabManager(CharacterMergerTab);
	SDockTab::FOnTabClosedCallback Delegate;
	Delegate.BindLambda([&](TSharedRef<SDockTab> Tab)
	{
		OnExistRequest();
	});
	CharacterMergerTab->SetOnTabClosed(Delegate);

	CharacterMergerTabManager->RegisterTabSpawner("Viewport", FOnSpawnTab::CreateRaw(this, &FCharacterMergerEditorModule::SpawnTab_Viewport))
		.SetDisplayName( LOCTEXT("ViewportTab", "Viewport"));
	CharacterMergerTabManager->RegisterTabSpawner("Toolbar", FOnSpawnTab::CreateRaw(this, &FCharacterMergerEditorModule::SpawnTab_Toolbar))
		.SetDisplayName( LOCTEXT("ToolbarTab", "Toolbar"));
	/*
	 * Example
	 * 
	 * TestSuite1TabManager->RegisterTabSpawner( "LayoutExampleTab", FOnSpawnTab::CreateStatic( &SpawnTab, FName("LayoutExampleTab") ) )
		.SetDisplayName( NSLOCTEXT("TestSuite1", "LayoutExampleTab", "Layout Example") )
		.SetGroup(TestSuiteMenu::SuiteTabs);
	*
	* Where SpawnTab is a method, which contains all block logic
	*
	*/
	
	CharacterMergerTab->SetContent
	(
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.FillHeight( 1.0f )
		[
			CharacterMergerTabManager->RestoreFrom(Layout, SpawnTabArgs.GetOwnerWindow()).ToSharedRef()
		]
	);

	return CharacterMergerTab;
}

TSharedRef<SDockTab> FCharacterMergerEditorModule::SpawnTab_Toolbar(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("ToolbarTab_Title", "Toolbar"))
		[
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Text(FText::FromString("Merge"))
				.OnClicked_Raw(this, &FCharacterMergerEditorModule::OnMergeRequested)
			]

			+SHorizontalBox::Slot()
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Text(FText::FromString("Compare two selected SK meshes' skinning"))
				.OnClicked_Raw(this, &FCharacterMergerEditorModule::CompareRig)
			]
		];
}

TSharedRef<SDockTab> FCharacterMergerEditorModule::SpawnTab_Viewport(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("ViewportTab_Title", "Viewport"))
		[
			SNew(SOverlay)

			// The sprite editor viewport
			+SOverlay::Slot()
			[
				ViewportPtr.ToSharedRef()
			]
		];
}
TSharedRef<SDockTab> FCharacterMergerEditorModule::SpawnTab_Details(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("ViewportTab_Title", "Viewport"))
		[
			SNew(SOverlay)

			// The sprite editor viewport
			+SOverlay::Slot()
			[
				ViewportPtr.ToSharedRef()
			]
		];
}
TSharedRef<SDockTab> FCharacterMergerEditorModule::SpawnTab_Hierarchy(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("ViewportTab_Title", "Viewport"))
		[
			SNew(SOverlay)

			// The sprite editor viewport
			+SOverlay::Slot()
			[
				ViewportPtr.ToSharedRef()
			]
		];
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FCharacterMergerEditorModule, CharacterMergerEditor)