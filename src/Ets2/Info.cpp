#include "precomp.hpp"
#include "Info.hpp"

#include "Parser/Cfg.hpp"
#include "lib/Utf8ToUtf16.hpp"

#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/dir.h>

namespace Ets2 {
	const std::string Info::SAVE_FORMAT_VAR_NAME = "g_save_format";

	std::wstring Info::getDefaultDirectory() {
		wxStandardPaths standardPaths = wxStandardPaths::Get();
		return std::wstring(standardPaths.GetDocumentsDir().Append(L"\\Euro Truck Simulator 2"));
	}

	Info::Info(const std::wstring directory) {
		wxStopWatch initTime;
		mDirectory = directory;
		mProfiles = ProfileList();

		wxFileName configFileName(mDirectory + L"\\config.cfg");
		wxDir profilesDir(mDirectory + L"\\profiles\\");
		mIsValid = configFileName.Exists() && profilesDir.IsOpened();
		if (!mIsValid) {
			return;
		}

		mConfigFileName = configFileName.GetFullPath();

		wxStopWatch parseTime;
		processSaveFormat();
		DEBUG_LOG(L"%s: parsed in %lld �s", configFileName.GetFullPath(), parseTime.TimeInMicro());

		// Get the profiles
		wxString profileDir;
		bool fileFound = profilesDir.GetFirst(&profileDir, L"*", wxDIR_DIRS);
		while (fileFound) {
			Profile * profile = new Profile((profilesDir.GetNameWithSep() + profileDir).ToStdWstring());
			if (profile->isValid()) {
				mProfiles.push_back(profile);
			} else {
				delete profile;
			}
			fileFound = profilesDir.GetNext(&profileDir);
		}
		mProfiles.sort();
		DEBUG_LOG(L"%s: initialized in %lld �s", mDirectory, initTime.TimeInMicro());
	}

	Info::Info(Info& info) {
		Info::Info(info.getDirectory());
	}

	bool Info::isValid() {
		return mIsValid;
	}

	std::wstring Info::getDirectory() {
		return mDirectory;
	}

	std::wstring Info::getConfigFileName() {
		return mConfigFileName;
	}

	Info::SaveFormat Info::getSaveFormat() {
		return mSaveFormat;
	}

	void Info::changeSaveFormat(SaveFormat newFormat) {
		if (mIsValid) {
			processSaveFormat(newFormat);
		}
	}

	std::wstring Info::getRawSaveFormat() {
		return mRawSaveFormat;
	}

	const ProfileList& Info::getProfiles() const {
		return mProfiles;
	}

	void Info::processSaveFormat(SaveFormat newFormat) {
		mRawSaveFormat = std::wstring(L"");
		std::string configFileContents;
		wxStopWatch pt;
		::read_file(mConfigFileName, configFileContents);
		std::string newConfigFile;
		std::string currentLine;
		mSaveFormat = SAVE_FORMAT_NOT_FOUND;
		std::string newFormatLine;
		if (newFormat != SAVE_FORMAT_INVALID) {
			newFormatLine.append("uset ");
			newFormatLine.append(SAVE_FORMAT_VAR_NAME);
			newFormatLine.append(" \"");
			newFormatLine.append(std::to_string(newFormat));
			newFormatLine.append("\"");
		}
		bool formatLineAppended = false;
		Parser::Cfg::parse(configFileContents, [this, &newFormat, &newConfigFile, &newFormatLine, &formatLineAppended](const std::string& line, const std::string& name, const std::string& value) {
			bool lineChanged = false;
			if (name == SAVE_FORMAT_VAR_NAME) {
				if (newFormat == SAVE_FORMAT_INVALID) {
					Utf8ToUtf16(value.data(), value.length(), mRawSaveFormat);
					long saveFormat;
					try {
						saveFormat = std::stol(mRawSaveFormat);
					} catch (std::invalid_argument) {
						saveFormat = -1;
					}
					switch (saveFormat) {
					case 0: case 1:
						mSaveFormat = SAVE_FORMAT_BINARY;
						break;
					case 2:
						mSaveFormat = SAVE_FORMAT_TEXT;
						break;
					case 3:
						mSaveFormat = SAVE_FORMAT_BOTH;
						break;
					default:
						mSaveFormat = SAVE_FORMAT_INVALID;
					}
				} else {
					newConfigFile.append(newFormatLine);
					newConfigFile.append("\r\n");
					formatLineAppended = true;
					lineChanged = true;
				}
			}
			if (newFormat != SAVE_FORMAT_INVALID && !lineChanged) {
				newConfigFile.append(line);
				newConfigFile.append("\r\n");
			}
		});

		if (newFormat != SAVE_FORMAT_INVALID) {
			if (!formatLineAppended) {
				newConfigFile.append(newFormatLine);
				newConfigFile.append("\r\n");
			}
			try {
				write_file(newConfigFile, mConfigFileName);
			} catch(int) {
				// if there was an error, check again to see if the change was saved
				processSaveFormat();
				if (mSaveFormat != newFormat) {
					throw(SaveFormatChangeFailed());
				}
			}
		}
	}
}