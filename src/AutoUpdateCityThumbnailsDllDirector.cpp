/*
 * This file is part of sc4-auto-update-city-thumbnails, a DLL Plugin for
 * SimCity 4 that adds a cheat code to automate updating the city thumbnails
 * in the region view.
 *
 * Copyright (C) 2026 Nicholas Hayes
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, under
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <https://www.gnu.org/licenses/>.
 */

#include "version.h"
#include "cIGZApp.h"
#include "cIGZCheatCodeManager.h"
#include "cIGZCOM.h"
#include "cIGZFrameWork.h"
#include "cIGZMessage2.h"
#include "cIGZMessage2Standard.h"
#include "cIGZMessageServer2.h"
#include "cIGZString.h"
#include "cISC4App.h"
#include "cISC4City.h"
#include "cISC4Region.h"
#include "cISC4RegionalCity.h"
#include "cISC4Simulator.h"
#include "cRZMessage2COMDirector.h"
#include "cRZAutoRefCount.h"
#include "cRZBaseString.h"
#include "GZServPtrs.h"
#include "Logger.h"
#include "SC4NotificationDialog.h"
#include "SC4Point.h"
#include "SC4String.h"
#include "SC4Vector.h"
#include "Settings.h"
#include "StringViewUtil.h"
#include "Stopwatch.h"
#include <array>

 // This must be unique for every plugin. Generate a random 32-bit integer and use it.
 // DO NOT REUSE DIRECTOR IDS EVER.
static constexpr uint32_t kAutoUpdateCityThumbnailsDllDirectorID = 0x407A2989;

static constexpr uint32_t kSC4MessagePostRegionInit = 0xCBB5BB45;
static constexpr uint32_t kSC4MessagePreRegionShutdown = 0x8BB5BB46;
static constexpr uint32_t kMessageCheatIssued = 0x230E27AC;

static constexpr uint32_t kMessageAutoUpdateCityThumbnailsLoadCity = 0xC0A03AD0;

static constexpr uint32_t kAutoUpdateCityThumbnailsCheatID = 0x6F8077E1;
static const char* const kAutoUpdateCityThumbnailsCheatName = "AutoUpdateCityThumbnails";

static constexpr std::array<uint32_t, 3> MessageIDs
{
	kSC4MessagePostRegionInit,
	kSC4MessagePreRegionShutdown
};

class AutoUpdateCityThumbnailsDllDirector : public cRZMessage2COMDirector
{
public:
	AutoUpdateCityThumbnailsDllDirector()
		: regionalCityLocations(),
		  pSC4App(nullptr),
		  pMS2(nullptr),
		  settings(),
		  regionalCityIndex(0),
		  needToResorePopupModalDialogState(false),
		  postedLoadCityMessage(false),
		  updateCityThumbnailCheatRegistered(false),
		  updateCityThumbnailCheatRunning(false)
	{
		Logger::GetInstance().WriteLogFileHeader("SC4AutoUpdateCityThumbnails v" PLUGIN_VERSION_STR);
	}

private:
	uint32_t GetDirectorID() const
	{
		return kAutoUpdateCityThumbnailsDllDirectorID;
	}

	bool DoMessage(cIGZMessage2* pMsg)
	{
		switch (pMsg->GetType())
		{
		case kMessageCheatIssued:
			ProcessCheatCode(static_cast<cIGZMessage2Standard*>(pMsg));
			break;
		case kSC4MessagePostRegionInit:
			PostRegionInit();
			break;
		case kSC4MessagePreRegionShutdown:
			PreRegionShutdown();
			break;
		case kMessageAutoUpdateCityThumbnailsLoadCity:
			AutoUpdateCityThumbnailsLoadCity();
			break;
		}

		return true;
	}

	void UpdateRegionalCityThumbnail(const SC4Point<int32_t>& location)
	{
		if (pSC4App)
		{
			cISC4Region* pRegion = pSC4App->GetRegion();

			if (pRegion)
			{
				cRZAutoRefCount<cISC4RegionalCity>* pRegionalCity = pRegion->GetCity(location.x, location.y);

				if (pRegionalCity)
				{
					SC4String saveFilePath;

					(*pRegionalCity)->GetCitySaveFilePath(saveFilePath);

					if (saveFilePath.Strlen() > 0 && pSC4App->LoadCity(saveFilePath, pRegionalCity))
					{
						cISC4City* pCity = pSC4App->GetCity();

						if (pCity)
						{
							cISC4Simulator* pSim = pCity->GetSimulator();

							if (settings.PauseCityBeforeSave() && !pSim->IsPaused())
							{
								pSim->Pause();
							}

							regionalCityIndex++;

							if (!pSim->IsEmergencyPaused())
							{
								if (settings.LogCityInfo())
								{
									Logger& logger = Logger::GetInstance();

									if (pCity->GetEstablished())
									{
										cRZBaseString name;

										pCity->GetCityName(name);

										logger.WriteLineFormatted(
											LogLevel::Info,
											"  Saving '%s' (%zu of %zu)",
											name.ToChar(),
											regionalCityIndex,
											regionalCityLocations.size());
									}
									else
									{
										logger.WriteLineFormatted(
											LogLevel::Info,
											"  Saving city at x: %d, z: %d (%zu of %zu)",
											location.x,
											location.y,
											regionalCityIndex,
											regionalCityLocations.size());
									}
									logger.Flush();
								}

								pSC4App->RequestSaveCity(false, false);
								pSC4App->RequestGoToRegionView(false);
							}
						}
					}
				}
			}
		}
	}

	void AutoUpdateCityThumbnailsLoadCity()
	{
		postedLoadCityMessage = false;
		if (regionalCityIndex < regionalCityLocations.size())
		{
			UpdateRegionalCityThumbnail(regionalCityLocations[regionalCityIndex]);
		}
	}

	void PostLoadCityMessageToSelf()
	{
		// Only allow the city load message to be posted once for each requested city load.
		// The game can call this function multiple times through PostRegionInit, and
		// having more than one city load message in the queue will cause a crash.
		if (!postedLoadCityMessage)
		{
			// Requesting a cIGZMessage2Standard instance from SC4 is required to use
			// any of the cIGZMessageServer2 Post methods due to the game taking
			// ownership of the passed in instance.

			cRZAutoRefCount<cIGZMessage2Standard> standardMessage;

			if (mpCOM->GetClassObject(
				GZCLSID_cRZMessage2Standard,
				GZIID_cIGZMessage2Standard,
				standardMessage.AsPPVoid()))
			{
				standardMessage->SetType(kMessageAutoUpdateCityThumbnailsLoadCity);

				postedLoadCityMessage = true;
				pMS2->GeneralMessagePostToTarget(standardMessage, this);
			}
		}
	}

	void ProcessCheatCode(cIGZMessage2Standard* pStandardMsg)
	{
		uint32_t cheatID = static_cast<uint32_t>(pStandardMsg->GetData1());

		if (cheatID == kAutoUpdateCityThumbnailsCheatID)
		{
			enum class IncludedRegionalCities
			{
				All = 0,
				Established,
				Unestablished
			};

			IncludedRegionalCities includedRegionalCities = IncludedRegionalCities::Unestablished;

			const cIGZString* cheatString = static_cast<cIGZString*>(pStandardMsg->GetVoid2());
			const std::string_view cheatStringAsStringView(cheatString->ToChar(), cheatString->Strlen());

			const size_t spaceIndex = cheatStringAsStringView.find(' ');

			if (spaceIndex != std::string_view::npos && spaceIndex != (cheatStringAsStringView.size() - 1))
			{
				const std::string_view cityModeArgument = cheatStringAsStringView.substr(spaceIndex + 1);

				if (StringViewUtil::EqualsIgnoreCase(cityModeArgument, "all"))
				{
					includedRegionalCities = IncludedRegionalCities::All;
				}
				else if (StringViewUtil::EqualsIgnoreCase(cityModeArgument, "established"))
				{
					includedRegionalCities = IncludedRegionalCities::Established;
				}
				else if (!StringViewUtil::EqualsIgnoreCase(cityModeArgument, "unestablished"))
				{
					cRZBaseString caption(kAutoUpdateCityThumbnailsCheatName);
					cRZBaseString message("Unknown city inclusion mode. Must be one of all, established, or unestablished.");

					SC4NotificationDialog::ShowDialog(message, caption);
					return;
				}
			}

			if (pSC4App)
			{
				cISC4Region* pRegion = pSC4App->GetRegion();

				if (pRegion)
				{
					SC4Vector<cISC4Region::cLocation> cityLocations;

					pRegion->GetCityLocations(cityLocations);

					regionalCityLocations.clear();

					if (settings.LogCityInfo())
					{
						const char* modeDescription = "unestablished (God mode)";

						if (includedRegionalCities == IncludedRegionalCities::All)
						{
							modeDescription = "all";
						}
						else if (includedRegionalCities == IncludedRegionalCities::Established)
						{
							modeDescription = "established (Mayor mode)";
						}

						Logger::GetInstance().WriteLineFormatted(
							LogLevel::Info,
							"Updating the region city thumbnail for %s cities in %s...",
							modeDescription,
							pRegion->GetName()->ToChar());
					}

					for (const auto& item : cityLocations)
					{
						cRZAutoRefCount<cISC4RegionalCity>* ppRegionalCity = pRegion->GetCity(item.x, item.z);

						if (ppRegionalCity)
						{
							switch (includedRegionalCities)
							{
							case IncludedRegionalCities::All:
								regionalCityLocations.push_back(SC4Point<int32_t>(item.x, item.z));
								break;
							case IncludedRegionalCities::Established:
								if ((*ppRegionalCity)->GetEstablished())
								{
									regionalCityLocations.push_back(SC4Point<int32_t>(item.x, item.z));
								}
								break;
							case IncludedRegionalCities::Unestablished:
								if (!(*ppRegionalCity)->GetEstablished())
								{
									regionalCityLocations.push_back(SC4Point<int32_t>(item.x, item.z));
								}
								break;
							}
						}
					}

					if (regionalCityLocations.empty())
					{
						if (settings.LogCityInfo())
						{
							Logger::GetInstance().WriteLine(
								LogLevel::Info,
								"  No cities to process.");
						}
					}
					else
					{
						if (pSC4App->GetPopupDialogsEnabled())
						{
							// Disable popup modal dialog boxes (such as the Reconcile Edges dialog) to prevent
							// a crash if one pops up when we are trying to save the loaded city.
							// The game doesn't appear to provide a reliable way to detect if a modal dialog is
							// active other than possibly listening for the creation/destruction messages sent
							// over cIGZMessageServer and doing our own state tracking, similar to what the
							// game does in cSC4AudioEventHandler::HandleModalMessages.
							//
							// Note that the scripted tutorial window created by cSC4TutorialUIWinProc
							// (TGI 0, 0x96a006b0, 0xa2dd355) is not a modal dialog box even though it visually
							// disables rest of the UI.
							pSC4App->SetPopupDialogsEnabled(false);
							needToResorePopupModalDialogState = true;
						}
						updateCityThumbnailCheatRunning = true;
						regionalCityIndex = 0;

						if (settings.LogCityInfo())
						{
							stopwatch.Restart();
						}

						// To prevent a crash within cGZCheatCodeManager::DoDefaultCheatCodeProcessing in some
						// regions (such as the default Timbuktu region), we must delay loading the first city
						// until the game finishes sending cheat commands.
						// This is accomplished by posting a custom message to ourselves and loading the city
						// when the game delivers it.
						PostLoadCityMessageToSelf();
					}
				}
			}
		}
	}

	void RegisterRegionViewCheatCode()
	{
		if (pSC4App)
		{
			cIGZCheatCodeManager* pCCM = pSC4App->GetCheatCodeManager();

			pCCM->RegisterCheatCode(kAutoUpdateCityThumbnailsCheatID, cRZBaseString(kAutoUpdateCityThumbnailsCheatName));
			pCCM->AddNotification2(this, 0);
			updateCityThumbnailCheatRegistered = true;
		}
	}

	void PostRegionInit()
	{
		if (updateCityThumbnailCheatRunning)
		{
			if (regionalCityIndex < regionalCityLocations.size())
			{
				// The next city is delay loaded using a custom message.
				// This allows the game to finish sending PostRegionInit to its other subscribers.
				PostLoadCityMessageToSelf();
			}
			else
			{
				updateCityThumbnailCheatRunning = false;

				if (needToResorePopupModalDialogState)
				{
					pSC4App->SetPopupDialogsEnabled(true);
				}

				if (settings.LogCityInfo())
				{
					stopwatch.Stop();
					// %T is an alias for the %H:%M:%S format.
					std::string text = std::format(
						"Processed {} cities in {:%T}",
						regionalCityLocations.size(),
						stopwatch.GetElapsedDuration());

					Logger::GetInstance().WriteLine(LogLevel::Info,	text.c_str());
				}
				RegisterRegionViewCheatCode();
			}
		}
		else
		{
			RegisterRegionViewCheatCode();
		}
	}

	void PreRegionShutdown()
	{
		if (updateCityThumbnailCheatRegistered)
		{
			updateCityThumbnailCheatRegistered = false;

			cIGZCheatCodeManager* pCCM = pSC4App->GetCheatCodeManager();

			pCCM->RemoveNotification2(this, 0);
			pCCM->UnregisterCheatCode(kAutoUpdateCityThumbnailsCheatID);
		}
	}

	bool PostAppInit()
	{
		mpFrameWork->Application()->QueryInterface(GZIID_cISC4App, reinterpret_cast<void**>(&pSC4App));

		cIGZMessageServer2Ptr msgServ;
		pMS2 = msgServ;
		msgServ->AddRef();

		for (uint32_t messageID : MessageIDs)
		{
			pMS2->AddNotification(this, messageID);
		}

		return true;
	}

	bool PreAppShutdown()
	{
		cISC4App* localSC4App = pSC4App;
		pSC4App = nullptr;

		if (localSC4App)
		{
			localSC4App->Release();
		}

		cIGZMessageServer2* localMS2 = pMS2;
		pMS2 = nullptr;

		if (localMS2)
		{
			localMS2->Release();
		}

		return true;
	}

	bool OnStart(cIGZCOM* pCOM)
	{
		mpFrameWork->AddHook(this);	
		return true;
	}

	Stopwatch stopwatch;
	std::vector<SC4Point<int32_t>> regionalCityLocations;
	size_t regionalCityIndex;
	cISC4App* pSC4App;
	cIGZMessageServer2* pMS2;
	Settings settings;
	bool needToResorePopupModalDialogState;
	bool postedLoadCityMessage;
	bool updateCityThumbnailCheatRegistered;
	bool updateCityThumbnailCheatRunning;
};

cRZCOMDllDirector* RZGetCOMDllDirector() {
	static AutoUpdateCityThumbnailsDllDirector sDirector;
	return &sDirector;
}
