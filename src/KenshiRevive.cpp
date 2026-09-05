#ifdef _DEBUG
#error "RE_Kenshi mod must be built in Release mode"
#endif
#if !defined(_WIN64)
#error "RE_Kenshi mod must be built for 'x64' not 'Win32' targets"
#endif
#if !defined(_MSC_VER) || _MSC_VER > 1600
#error "RE_Kenshi mod must be built using Microsoft Visual Studio 2010 toolchain"
#endif

#ifndef _WINDOWS_
#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#include <Windows.h>
#endif

#include <Debug.h>
#include <core/Functions.h>

#include <kenshi/gui/ForgottenGUI.h>
#include <kenshi/Character.h>
#include <kenshi/Platoon.h>
#include <kenshi/PlayerInterface.h>

#include "Translate.h"

struct KenshiReviveState {
	MyGUI::Widget* revivePanel;
	hand selectedDeadCharacter;
	hand selectedAliveCharacter;

	KenshiReviveState() : revivePanel(nullptr), selectedDeadCharacter(nullptr), selectedAliveCharacter(nullptr) {}
};

/** Store all global state here */
static KenshiReviveState g_state;

// Kenshi prefixes widget names as "prefix_Name"; match by suffix (RE_Kenshi pattern).
// 
// Borrowed from: https://github.com/jacobericson/KenshiRotate/blob/bfcc5cec6c3196b1ceec7e7929b9c43f7e09370f/src/Settings.cpp
// -- Thanks jacobericson! You bloody' fuckin' legend. Your Kenshi mod code is great, nice and easy to read. <3 SilbinaryWolf
static MyGUI::Widget* FindWidget(MyGUI::EnumeratorWidgetPtr enumerator,
	const std::string& name, unsigned short depth = 0)
{
	while (enumerator.next())
	{
		std::string widgetName = enumerator.current()->getName();
		size_t splitPos = widgetName.find('_');
		{
			char debugBuf[256];
			sprintf_s(debugBuf, "depth(%d) - %s", depth, widgetName.substr(splitPos + 1).c_str());
			DebugLog(debugBuf);
		}
		if (splitPos != std::string::npos &&
			widgetName.substr(splitPos + 1) == name)
		{
			return enumerator.current();
		}
		if (enumerator.current()->getChildCount() > 0)
		{
			MyGUI::Widget* child = FindWidget(
				enumerator.current()->getEnumerator(), name, depth + 1);
			if (child != nullptr)
				return child;
		}
	}
	return nullptr;
}


void OnReviveButtonPress(MyGUI::WidgetPtr sender)
{
	Character* deadCharacter = g_state.selectedDeadCharacter.getCharacter();
	ActivePlatoon* aliveCharacterPlatoon = g_state.selectedAliveCharacter.getActivePlatoon();
	if (deadCharacter != nullptr &&
		deadCharacter->medical.dead &&
		aliveCharacterPlatoon != nullptr &&
		aliveCharacterPlatoon->isPlayer != nullptr)
	{
		PlayerInterface* player_interface = aliveCharacterPlatoon->isPlayer;
		deadCharacter->medical.dead = false; // Definitely needed before "recruit" or the game crashes and they stay dead
		deadCharacter->healCompletely();
		player_interface->recruit(deadCharacter, false); // false = editor

		MyGUI::Widget* revivePanel = sender->getParent();
		if (revivePanel != nullptr) {
			revivePanel->setVisible(false);
		}
	}
}

const char KR_REVIVE_PANEL[] = "KR_SW_RevivePanel";
const char KR_REVIVE_BUTTON[] = "KR_SW_ReviveButton";

void (*ForgottenGUI_selectedObjectsChanged_orig)(ForgottenGUI*) = nullptr;

void ForgottenGUI_selectedObjectsChanged_hook(ForgottenGUI* thisptr)
{
	ForgottenGUI_selectedObjectsChanged_orig(thisptr);

	// If dead character selected
	if (thisptr->selectedObject.type == ::CHARACTER) {
		Character* character = thisptr->selectedObject.getCharacter();
		if (character != nullptr &&
			character->isDead()) {
			g_state.selectedDeadCharacter = character->getHandle();
			g_state.selectedAliveCharacter = thisptr->selectedPlayerCharacter;
			if (g_state.revivePanel == nullptr) {
				MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
				MyGUI::Widget* revivePanel = gui->findWidget<MyGUI::Widget>(KR_REVIVE_PANEL, false);
				if (revivePanel != nullptr) {
					revivePanel->setVisible(true);
				} else {
					// If button doesn't exist create it under the "Status Panel
					MyGUI::Widget* widget = FindWidget(gui->getEnumerator(), "StatusPanel");
					if (widget != nullptr) {
						MyGUI::Widget* revivePanel = widget->getParent()->createWidgetReal<MyGUI::Widget>("Kenshi_Panel", 0.75f, 0.0f, 0.25f, 0.15f, MyGUI::Align::Right, KR_REVIVE_PANEL);
						MyGUI::Button* reviveButton = revivePanel->createWidgetReal<MyGUI::Button>("Kenshi_Button1", 0.0f, 0.0f, 1.0f, 1.0f, MyGUI::Align::Left, KR_REVIVE_BUTTON);
						reviveButton->setCaption(Tr(TR_REVIVE));
						reviveButton->eventMouseButtonClick += MyGUI::newDelegate(OnReviveButtonPress);
					}
				}
			}
			return;
		}
	}

	// If not a dead character selected character, reset values
	if (g_state.selectedDeadCharacter.type == ::CHARACTER) {
		g_state.selectedDeadCharacter = nullptr;
		g_state.selectedAliveCharacter = nullptr;
		MyGUI::Widget* revivePanel = MyGUI::Gui::getInstancePtr()->findWidget<MyGUI::Widget>(KR_REVIVE_PANEL, false);
		if (revivePanel != nullptr) {
			revivePanel->setVisible(false);
		}
	}
}

__declspec(dllexport) void startPlugin()
{
	DebugLog("[KenshiRevive] Starting plugin...");

	DetectLanguage();

	// okFlag is non-NULL only for the two persistence hooks.
	struct HookEntry
	{
		const char* name;
		void* realAddr;
		void* detour;
		void** origStore;
		bool* okFlag;
	};

	#define HOOK_ENTRY(label, targetFn, hookFn, origPtr, okFlagPtr) \
		{ label, \
		  reinterpret_cast<void*>(KenshiLib::GetRealAddress(&targetFn)), \
		  reinterpret_cast<void*>(hookFn), \
		  reinterpret_cast<void**>(&origPtr) }

	HookEntry hooks[] =
	{
		HOOK_ENTRY("ForgottenGUI::selectedObjectsChanged",   ForgottenGUI::selectedObjectsChanged,  ForgottenGUI_selectedObjectsChanged_hook, ForgottenGUI_selectedObjectsChanged_orig, nullptr),
	};

	#undef HOOK_ENTRY

	const size_t hookCount = sizeof(hooks) / sizeof(hooks[0]);
	for (size_t i = 0; i < hookCount; ++i) {
		const HookEntry& h = hooks[i];
		if (KenshiLib::SUCCESS != KenshiLib::AddHook(h.realAddr, h.detour, h.origStore)) {
			ErrorLog((std::string("[KenshiRevive] Failed to hook ") + h.name).c_str());
		} else {
			DebugLog((std::string("[KenshiRevive] Hooked ") + h.name + " OK").c_str());
		}
	}

	DebugLog("[KenshiRevive] Plugin started successfully");
}
