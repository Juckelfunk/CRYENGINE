// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AudioControlsEditorPlugin.h"

#include <CryCore/Platform/platform_impl.inl>

#include "IUndoManager.h"
#include "QtViewPane.h"
#include "AudioControlsEditorWindow.h"
#include "IResourceSelectorHost.h"
#include "AudioControlsLoader.h"
#include "AudioControlsWriter.h"
#include "AudioAssets.h"
#include <IAudioSystemEditor.h>
#include <CryAudio/IAudioSystem.h>
#include <CryAudio/IObject.h>
#include <CryMath/Cry_Camera.h>

#include <CrySystem/File/CryFile.h>
#include <CryString/CryPath.h>
#include "ImplementationManager.h"

REGISTER_PLUGIN(CAudioControlsEditorPlugin);

using namespace CryAudio;
using namespace ACE;
using namespace PathUtil;

CAudioAssetsManager CAudioControlsEditorPlugin::s_pAssetsManager;
std::set<string> CAudioControlsEditorPlugin::s_currentFilenames;
IObject* CAudioControlsEditorPlugin::s_pIAudioObject = nullptr;
ControlId CAudioControlsEditorPlugin::s_audioTriggerId = InvalidControlId;
CImplementationManager CAudioControlsEditorPlugin::s_implementationManager;
uint CAudioControlsEditorPlugin::s_loadingErrorMask;
CCrySignal<void()> CAudioControlsEditorPlugin::signalAboutToLoad;
CCrySignal<void()> CAudioControlsEditorPlugin::signalLoaded;

REGISTER_VIEWPANE_FACTORY(CAudioControlsEditorWindow, "Audio Controls Editor", "Tools", true)

CAudioControlsEditorPlugin::CAudioControlsEditorPlugin()
{
	SCreateObjectData const objectData("Audio trigger preview", EOcclusionType::Ignore);
	s_pIAudioObject = gEnv->pAudioSystem->CreateObject(objectData);

	s_implementationManager.LoadImplementation();
	ReloadModels(false);
	s_pAssetsManager.Initialize();
	GetISystem()->GetISystemEventDispatcher()->RegisterListener(this, "CAudioControlsEditorPlugin");
}

CAudioControlsEditorPlugin::~CAudioControlsEditorPlugin()
{
	s_implementationManager.Release();
	if (s_pIAudioObject != nullptr)
	{
		StopTriggerExecution();
		gEnv->pAudioSystem->ReleaseObject(s_pIAudioObject);
	}
}

void CAudioControlsEditorPlugin::SaveModels()
{
	ACE::IAudioSystemEditor* pImpl = s_implementationManager.GetImplementation();
	if (pImpl)
	{
		CAudioControlsWriter writer(&s_pAssetsManager, pImpl, s_currentFilenames);
	}
	s_loadingErrorMask = static_cast<uint>(EErrorCode::eErrorCode_NoError);
}

void CAudioControlsEditorPlugin::ReloadModels(bool bReloadImplementation)
{
	signalAboutToLoad();
	GetIEditor()->GetIUndoManager()->Suspend();

	ACE::IAudioSystemEditor* pImpl = s_implementationManager.GetImplementation();
	if (pImpl)
	{
		s_pAssetsManager.Clear();
		if (bReloadImplementation)
		{
			pImpl->Reload();
		}
		CAudioControlsLoader loader(&s_pAssetsManager);
		loader.LoadAll();
		s_pAssetsManager.ClearDirtyFlags();
		s_currentFilenames = loader.GetLoadedFilenamesList();
		s_loadingErrorMask = loader.GetErrorCodeMask();
	}
	GetIEditor()->GetIUndoManager()->Resume();
	signalLoaded();

}

void CAudioControlsEditorPlugin::ReloadScopes()
{
	s_pAssetsManager.ClearScopes();
	CAudioControlsLoader loader(&s_pAssetsManager);
	loader.LoadScopes();
}

CAudioAssetsManager* CAudioControlsEditorPlugin::GetAssetsManager()
{
	return &s_pAssetsManager;
}

ACE::IAudioSystemEditor* CAudioControlsEditorPlugin::GetAudioSystemEditorImpl()
{
	return s_implementationManager.GetImplementation();
}

void CAudioControlsEditorPlugin::ExecuteTrigger(const string& sTriggerName)
{
	if (!sTriggerName.empty() && s_pIAudioObject)
	{
		StopTriggerExecution();
		gEnv->pAudioSystem->GetAudioTriggerId(sTriggerName.c_str(), s_audioTriggerId);

		if (s_audioTriggerId != InvalidControlId)
		{
			const CCamera& camera = GetIEditor()->GetSystem()->GetViewCamera();
			const Matrix34& cameraMatrix = camera.GetMatrix();
			s_pIAudioObject->SetTransformation(cameraMatrix);
			s_pIAudioObject->ExecuteTrigger(s_audioTriggerId);
		}
	}
}

void CAudioControlsEditorPlugin::StopTriggerExecution()
{
	if (s_pIAudioObject && s_audioTriggerId != InvalidControlId)
	{
		s_pIAudioObject->StopTrigger(s_audioTriggerId);
		s_audioTriggerId = InvalidControlId;
	}
}

void CAudioControlsEditorPlugin::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_AUDIO_IMPLEMENTATION_LOADED:
		GetIEditor()->GetIUndoManager()->Suspend();
		s_implementationManager.LoadImplementation();
		GetIEditor()->GetIUndoManager()->Resume();
		break;
	}
}

CImplementationManager* CAudioControlsEditorPlugin::GetImplementationManger()
{
	return &s_implementationManager;
}
