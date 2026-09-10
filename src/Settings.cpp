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

#include "Settings.h"
#include "FileSystem.h"
#include "IniReader.h"
#include "Logger.h"
#include <fstream>

Settings::Settings() : logCityInfo(true), pauseCityBeforeSave(true)
{
}

void Settings::Load()
{
	Logger& logger = Logger::GetInstance();

	try
	{
		std::filesystem::path path = FileSystem::GetDllIniFilePath();

		std::ifstream stream(path, std::ifstream::in);

		if (stream)
		{
			IniReader iniReader(stream);

			const auto& autoUpdateCityThumbnailsSection = iniReader.get_section("AutoUpdateCityThumbnails");

			logCityInfo = autoUpdateCityThumbnailsSection.get_converted_value<bool>("LogCityInfo", true);
			pauseCityBeforeSave = autoUpdateCityThumbnailsSection.get_converted_value<bool>("PauseCityBeforeSave", true);
		}
		else
		{
			logger.WriteLine(LogLevel::Error, "Failed to open the DLL INI file.");
		}
	}
	catch (const std::exception& e)
	{
		logger.WriteLineFormatted(
			LogLevel::Error,
			"Error reading the DLL INI file: %s",
			e.what());
	}
}

bool Settings::LogCityInfo() const
{
	return logCityInfo;
}

bool Settings::PauseCityBeforeSave() const
{
	return pauseCityBeforeSave;
}
