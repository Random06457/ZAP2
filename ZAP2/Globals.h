#pragma once

#include <string>
#include "ZFile.h"
#include "ZTexture.h"
#include "ZRoom/ZRoom.h"

class Globals
{
public:
	static Globals* Instance;

	bool genSourceFile; // Used for extraction
	bool useExternalResources;
	bool testMode; // Enables certain experimental features
	bool debugMessages; // Enables certain printfs
	bool profile; // Measure performance of certain operations
	bool includeFilePrefix; // Include the file prefix in symbols
	ZFileMode fileMode;
	std::string baseRomPath, inputPath, outputPath, cfgPath;
	TextureType texType;

	ZRoom* lastScene;
	std::map<uint32_t, std::string> symbolMap;

	Globals();
	void GenSymbolMap(std::string symbolMapPath);
};