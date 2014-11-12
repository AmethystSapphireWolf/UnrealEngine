// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "Paper2DEditorPrivatePCH.h"
#include "TileMapEditor.h"
#include "SSingleObjectDetailsPanel.h"
#include "SceneViewport.h"

#include "GraphEditor.h"

#include "TileMapEditorViewportClient.h"
#include "TileMapEditorCommands.h"
#include "SEditorViewport.h"
#include "WorkspaceMenuStructureModule.h"
#include "Paper2DEditorModule.h"
#include "STileMapEditorViewportToolbar.h"
#include "SDockTab.h"

#define LOCTEXT_NAMESPACE "TileMapEditor"

//////////////////////////////////////////////////////////////////////////

const FName TileMapEditorAppName = FName(TEXT("TileMapEditorApp"));

//////////////////////////////////////////////////////////////////////////

struct FTileMapEditorTabs
{
	// Tab identifiers
	static const FName DetailsID;
	static const FName ViewportID;
	static const FName ToolboxHostID;
};

//////////////////////////////////////////////////////////////////////////

const FName FTileMapEditorTabs::DetailsID(TEXT("Details"));
const FName FTileMapEditorTabs::ViewportID(TEXT("Viewport"));
const FName FTileMapEditorTabs::ToolboxHostID(TEXT("Toolbox"));

//////////////////////////////////////////////////////////////////////////
// STileMapEditorViewport

class STileMapEditorViewport : public SEditorViewport, public ICommonEditorViewportToolbarInfoProvider
{
public:
	SLATE_BEGIN_ARGS(STileMapEditorViewport) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<FTileMapEditor> InTileMapEditor);

	// SEditorViewport interface
	virtual void BindCommands() override;
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual TSharedPtr<SWidget> MakeViewportToolbar() override;
	virtual EVisibility GetTransformToolbarVisibility() const override;
	// End of SEditorViewport interface

	// ICommonEditorViewportToolbarInfoProvider interface
	virtual TSharedRef<class SEditorViewport> GetViewportWidget() override;
	virtual TSharedPtr<FExtender> GetExtenders() const override;
	virtual void OnFloatingButtonClicked() override;
	// End of ICommonEditorViewportToolbarInfoProvider interface

	// Invalidate any references to the tile map being edited; it has changed
	void NotifyTileMapBeingEditedHasChanged()
	{
		EditorViewportClient->NotifyTileMapBeingEditedHasChanged();
	}
private:
	// Pointer back to owning tile map editor instance (the keeper of state)
	TWeakPtr<class FTileMapEditor> TileMapEditorPtr;

	// Viewport client
	TSharedPtr<FTileMapEditorViewportClient> EditorViewportClient;

private:
	// Returns true if the viewport is visible
	bool IsVisible() const;
};

void STileMapEditorViewport::Construct(const FArguments& InArgs, TSharedPtr<FTileMapEditor> InTileMapEditor)
{
	TileMapEditorPtr = InTileMapEditor;

	SEditorViewport::Construct(SEditorViewport::FArguments());
}

void STileMapEditorViewport::BindCommands()
{
	SEditorViewport::BindCommands();

	const FTileMapEditorCommands& Commands = FTileMapEditorCommands::Get();

	TSharedRef<FTileMapEditorViewportClient> EditorViewportClientRef = EditorViewportClient.ToSharedRef();

	// Show toggles
	CommandList->MapAction(
		Commands.SetShowCollision,
		FExecuteAction::CreateSP(EditorViewportClientRef, &FEditorViewportClient::SetShowCollision),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(EditorViewportClientRef, &FEditorViewportClient::IsSetShowCollisionChecked));

	CommandList->MapAction(
		Commands.SetShowPivot,
		FExecuteAction::CreateSP(EditorViewportClientRef, &FTileMapEditorViewportClient::ToggleShowPivot),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(EditorViewportClientRef, &FTileMapEditorViewportClient::IsShowPivotChecked));

	// Misc. actions
	CommandList->MapAction(
		Commands.FocusOnTileMap,
		FExecuteAction::CreateSP(EditorViewportClientRef, &FTileMapEditorViewportClient::FocusOnTileMap));
}

TSharedRef<FEditorViewportClient> STileMapEditorViewport::MakeEditorViewportClient()
{
	EditorViewportClient = MakeShareable(new FTileMapEditorViewportClient(TileMapEditorPtr, SharedThis(this)));

	EditorViewportClient->VisibilityDelegate.BindSP(this, &STileMapEditorViewport::IsVisible);

	return EditorViewportClient.ToSharedRef();
}

bool STileMapEditorViewport::IsVisible() const
{
	return true;//@TODO: Determine this better so viewport ticking optimizations can take place
}

TSharedPtr<SWidget> STileMapEditorViewport::MakeViewportToolbar()
{
	return SNew(STileMapEditorViewportToolbar, SharedThis(this));
}

EVisibility STileMapEditorViewport::GetTransformToolbarVisibility() const
{
	return EVisibility::Visible;
}

TSharedRef<class SEditorViewport> STileMapEditorViewport::GetViewportWidget()
{
	return SharedThis(this);
}

TSharedPtr<FExtender> STileMapEditorViewport::GetExtenders() const
{
	TSharedPtr<FExtender> Result(MakeShareable(new FExtender));
	return Result;
}

void STileMapEditorViewport::OnFloatingButtonClicked()
{
}

/////////////////////////////////////////////////////
// STileMapPropertiesTabBody

class STileMapPropertiesTabBody : public SSingleObjectDetailsPanel
{
public:
	SLATE_BEGIN_ARGS(STileMapPropertiesTabBody) {}
	SLATE_END_ARGS()

private:
	// Pointer back to owning TileMap editor instance (the keeper of state)
	TWeakPtr<class FTileMapEditor> TileMapEditorPtr;
public:
	void Construct(const FArguments& InArgs, TSharedPtr<FTileMapEditor> InTileMapEditor)
	{
		TileMapEditorPtr = InTileMapEditor;

		SSingleObjectDetailsPanel::Construct(SSingleObjectDetailsPanel::FArguments());
	}

	// SSingleObjectDetailsPanel interface
	virtual UObject* GetObjectToObserve() const override
	{
		return TileMapEditorPtr.Pin()->GetTileMapBeingEdited();
	}

	virtual TSharedRef<SWidget> PopulateSlot(TSharedRef<SWidget> PropertyEditorWidget) override
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(1)
			[
				PropertyEditorWidget
			];
	}
	// End of SSingleObjectDetailsPanel interface
};

//////////////////////////////////////////////////////////////////////////
// FTileMapEditor

TSharedRef<SDockTab> FTileMapEditor::SpawnTab_Viewport(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("ViewportTab_Title", "Viewport"))
		[
			SNew(SOverlay)

			// The TileMap editor viewport
			+ SOverlay::Slot()
			[
				ViewportPtr.ToSharedRef()
			]

			// Bottom-right corner text indicating the preview nature of the tile map editor
			+ SOverlay::Slot()
				.Padding(10)
				.VAlign(VAlign_Bottom)
				.HAlign(HAlign_Right)
				[
					SNew(STextBlock)
					.Visibility(EVisibility::HitTestInvisible)
					.TextStyle(FEditorStyle::Get(), "Graph.CornerText")
					.Text(LOCTEXT("TileMapEditorViewportExperimentalWarning", "Experimental!"))
				]
		];
}

TSharedRef<SDockTab> FTileMapEditor::SpawnTab_ToolboxHost(const FSpawnTabArgs& Args)
{
	TSharedPtr<FTileMapEditor> TileMapEditorPtr = SharedThis(this);

	// Spawn the tab
	return SNew(SDockTab)
		.Icon(FEditorStyle::GetBrush("LevelEditor.Tabs.Modes"))
		.Label(LOCTEXT("ToolboxHost_Title", "Toolbox"))
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ToolkitControlsGohere", "Toolkit controls go here"))
		];
}

TSharedRef<SDockTab> FTileMapEditor::SpawnTab_Details(const FSpawnTabArgs& Args)
{
	TSharedPtr<FTileMapEditor> TileMapEditorPtr = SharedThis(this);

	// Spawn the tab
	return SNew(SDockTab)
		.Icon(FEditorStyle::GetBrush("LevelEditor.Tabs.Details"))
		.Label(LOCTEXT("DetailsTab_Title", "Details"))
		[
			SNew(STileMapPropertiesTabBody, TileMapEditorPtr)
		];
}

void FTileMapEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{
	WorkspaceMenuCategory = TabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_TileMapEditor", "Tile Map Editor"));
	auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(TabManager);

	TabManager->RegisterTabSpawner(FTileMapEditorTabs::ViewportID, FOnSpawnTab::CreateSP(this, &FTileMapEditor::SpawnTab_Viewport))
		.SetDisplayName(LOCTEXT("ViewportTab", "Viewport"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Viewports"));

	TabManager->RegisterTabSpawner(FTileMapEditorTabs::ToolboxHostID, FOnSpawnTab::CreateSP(this, &FTileMapEditor::SpawnTab_ToolboxHost))
		.SetDisplayName(LOCTEXT("ToolboxHostLabel", "Toolbox"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Modes"));

	TabManager->RegisterTabSpawner(FTileMapEditorTabs::DetailsID, FOnSpawnTab::CreateSP(this, &FTileMapEditor::SpawnTab_Details))
		.SetDisplayName(LOCTEXT("DetailsTabLabel", "Details"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details"));
}

void FTileMapEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(TabManager);

	TabManager->UnregisterTabSpawner(FTileMapEditorTabs::ViewportID);
	TabManager->UnregisterTabSpawner(FTileMapEditorTabs::ToolboxHostID);
	TabManager->UnregisterTabSpawner(FTileMapEditorTabs::DetailsID);
}

void FTileMapEditor::InitTileMapEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, class UPaperTileMap* InitTileMap)
{
	FAssetEditorManager::Get().CloseOtherEditors(InitTileMap, this);
	TileMapBeingEdited = InitTileMap;

	FTileMapEditorCommands::Register();

	BindCommands();

	ViewportPtr = SNew(STileMapEditorViewport, SharedThis(this));

	// Default layout
	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_TileMapEditor_Layout_v2")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.1f)
				->SetHideTabWell(true)
				->AddTab(GetToolbarTabId(), ETabState::OpenedTab)
			)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
				->SetSizeCoefficient(0.9f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.3f)
					->SetHideTabWell(true)
					->AddTab(FTileMapEditorTabs::ToolboxHostID, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.8f)
					->SetHideTabWell(true)
					->AddTab(FTileMapEditorTabs::ViewportID, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.2f)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.75f)
						->AddTab(FTileMapEditorTabs::DetailsID, ETabState::OpenedTab)
					)
				)
			)
		);

	// Initialize the asset editor and spawn the layout above
	InitAssetEditor(Mode, InitToolkitHost, TileMapEditorAppName, StandaloneDefaultLayout, /*bCreateDefaultStandaloneMenu=*/ true, /*bCreateDefaultToolbar=*/ true, InitTileMap);

	// Extend things
	ExtendMenu();
	ExtendToolbar();
	RegenerateMenusAndToolbars();

	// Activate the tile map edit mode
	EditorModeToolsInstance.SetDefaultMode(FEdModeTileMap::EM_TileMap);
	EditorModeToolsInstance.ActivateDefaultMode();

	//@TODO: Need to be able to pass ToolkitHost.Get() into EditorModeToolsInstance, and have it in turn pass it into Enter/Leave on the individual modes I think
	//@TODO: Need to be able to register the widget in the toolbox panel with ToolkitHost, so it can instance the ed mode widgets into it
}

void FTileMapEditor::BindCommands()
{
	// 	const FTileMapEditorCommands& Commands = FTileMapEditorCommands::Get();
	// 
	// 	const TSharedRef<FUICommandList>& UICommandList = GetToolkitCommands();
	// 
	// 	UICommandList->MapAction( FGenericCommands::Get().Delete,
	// 		FExecuteAction::CreateSP( this, &FStaticMeshEditor::DeleteSelectedSockets ),
	// 		FCanExecuteAction::CreateSP( this, &FStaticMeshEditor::HasSelectedSockets ));
	// 
	// 	UICommandList->MapAction( FGenericCommands::Get().Undo, 
	// 		FExecuteAction::CreateSP( this, &FStaticMeshEditor::UndoAction ) );
	// 
	// 	UICommandList->MapAction( FGenericCommands::Get().Redo, 
	// 		FExecuteAction::CreateSP( this, &FStaticMeshEditor::RedoAction ) );
	// 
	// 	UICommandList->MapAction(
	// 		FGenericCommands::Get().Duplicate,
	// 		FExecuteAction::CreateSP(this, &FStaticMeshEditor::DuplicateSelectedSocket),
	// 		FCanExecuteAction::CreateSP(this, &FStaticMeshEditor::HasSelectedSockets));
}

FName FTileMapEditor::GetToolkitFName() const
{
	return FName("TileMapEditor");
}

FText FTileMapEditor::GetBaseToolkitName() const
{
	return LOCTEXT("TileMapEditorAppLabelBase", "Tile Map Editor");
}

FText FTileMapEditor::GetToolkitName() const
{
	const bool bDirtyState = TileMapBeingEdited->GetOutermost()->IsDirty();

	FFormatNamedArguments Args;
	Args.Add(TEXT("TileMapName"), FText::FromString(TileMapBeingEdited->GetName()));
	Args.Add(TEXT("DirtyState"), bDirtyState ? FText::FromString(TEXT("*")) : FText::GetEmpty());
	return FText::Format(LOCTEXT("TileMapEditorAppLabel", "{TileMapName}{DirtyState}"), Args);
}

FString FTileMapEditor::GetWorldCentricTabPrefix() const
{
	return TEXT("TileMapEditor");
}

FString FTileMapEditor::GetDocumentationLink() const
{
	return TEXT("Engine/Paper2D/TileMapEditor");
}

FLinearColor FTileMapEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor::White;
}

void FTileMapEditor::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(TileMapBeingEdited);
}

void FTileMapEditor::ExtendMenu()
{
}

void FTileMapEditor::ExtendToolbar()
{
	struct Local
	{
		static void FillToolbar(FToolBarBuilder& ToolbarBuilder)
		{
			// 			ToolbarBuilder.BeginSection("Realtime");
			// 			{
			// 				ToolbarBuilder.AddToolBarButton(FEditorViewportCommands::Get().ToggleRealTime);
			// 			}
			// 			ToolbarBuilder.EndSection();

// 			ToolbarBuilder.BeginSection("Command");
// 			{
// 				ToolbarBuilder.AddToolBarButton(FTileMapEditorCommands::Get().SetShowSourceTexture);
// 			}
// 			ToolbarBuilder.EndSection();
// 
// 			ToolbarBuilder.BeginSection("Tools");
// 			{
// 				ToolbarBuilder.AddToolBarButton(FTileMapEditorCommands::Get().AddPolygon);
// 				ToolbarBuilder.AddToolBarButton(FTileMapEditorCommands::Get().SnapAllVertices);
// 			}
// 			ToolbarBuilder.EndSection();
		}
	};

	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);

	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		ViewportPtr->GetCommandList(),
		FToolBarExtensionDelegate::CreateStatic(&Local::FillToolbar)
		);

// 	ToolbarExtender->AddToolBarExtension(
// 		"Asset",
// 		EExtensionHook::After,
// 		ViewportPtr->GetCommandList(),
// 		FToolBarExtensionDelegate::CreateSP(this, &FTileMapEditor::CreateModeToolbarWidgets));

	AddToolbarExtender(ToolbarExtender);
// 
// 	IPaper2DEditorModule* Paper2DEditorModule = &FModuleManager::LoadModuleChecked<IPaper2DEditorModule>("Paper2DEditor");
// 	AddToolbarExtender(Paper2DEditorModule->GetTileMapEditorToolBarExtensibilityManager()->GetAllExtenders());
}

void FTileMapEditor::SetTileMapBeingEdited(UPaperTileMap* NewTileMap)
{
	if ((NewTileMap != TileMapBeingEdited) && (NewTileMap != NULL))
	{
		UPaperTileMap* OldTileMap = TileMapBeingEdited;
		TileMapBeingEdited = NewTileMap;

		// Let the viewport know that we are editing something different
		ViewportPtr->NotifyTileMapBeingEditedHasChanged();

		// Let the editor know that are editing something different
		RemoveEditingObject(OldTileMap);
		AddEditingObject(NewTileMap);
	}
}


void FTileMapEditor::CreateModeToolbarWidgets(FToolBarBuilder& IgnoredBuilder)
{
// 	FToolBarBuilder ToolbarBuilder(ViewportPtr->GetCommandList(), FMultiBoxCustomization::None);
// 	ToolbarBuilder.AddToolBarButton(FTileMapEditorCommands::Get().EnterViewMode);
// 	ToolbarBuilder.AddToolBarButton(FTileMapEditorCommands::Get().EnterSourceRegionEditMode);
// 	ToolbarBuilder.AddToolBarButton(FTileMapEditorCommands::Get().EnterCollisionEditMode);
// 	ToolbarBuilder.AddToolBarButton(FTileMapEditorCommands::Get().EnterRenderingEditMode);
// 	AddToolbarWidget(ToolbarBuilder.MakeWidget());
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
