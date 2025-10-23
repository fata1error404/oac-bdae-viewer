#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdint>
#include <vector>
#include <algorithm>

int main()
{
	std::cout << "Animation Debug Tool\n";
	std::cout << "====================\n\n";

	// Read animation file
	std::ifstream file("tools/dragon_animation_idle.bdae", std::ios::binary);
	if (!file)
	{
		std::cerr << "Failed to open animation file\n";
		return 1;
	}

	// Get file size
	file.seekg(0, std::ios::end);
	size_t fileSize = file.tellg();
	file.seekg(0, std::ios::beg);

	std::cout << "File size: " << fileSize << " bytes\n\n";

	// Read header
	uint32_t signature;
	file.read((char *)&signature, 4);

	char sigStr[5] = {0};
	memcpy(sigStr, &signature, 4);
	std::cout << "Signature: " << sigStr << "\n";

	// Skip to offset data (at byte 32)
	file.seekg(32);
	uint64_t offsetData;
	file.read((char *)&offsetData, 8);

	std::cout << "Offset Data: 0x" << std::hex << offsetData << std::dec << "\n\n";

	// Read SLibraryAnimations (at offsetData + 56)
	uint64_t libAnimOffset = offsetData + 56;
	file.seekg(libAnimOffset);

	uint32_t animCount, animOffsetRel;
	file.read((char *)&animCount, 4);
	file.read((char *)&animOffsetRel, 4);

	uint64_t animArrayOffset = libAnimOffset + 4 + animOffsetRel;

	std::cout << "Animation count: " << animCount << "\n";
	std::cout << "Animation array offset: 0x" << std::hex << animArrayOffset << std::dec << "\n\n";

	// Read first few animations
	int maxToShow = 10;
	for (int i = 0; i < std::min((int)animCount, maxToShow); i++)
	{
		uint64_t animPos = animArrayOffset + i * 40;
		file.seekg(animPos);

		uint32_t idOffset;
		file.read((char *)&idOffset, 4);
		file.seekg(4, std::ios::cur); // skip padding

		uint32_t samplersCount, samplersOffsetRel;
		file.read((char *)&samplersCount, 4);
		file.read((char *)&samplersOffsetRel, 4);

		uint32_t channelsCount, channelsOffsetRel;
		file.read((char *)&channelsCount, 4);
		file.read((char *)&channelsOffsetRel, 4);

		// Read animation name
		file.seekg(idOffset);
		uint32_t nameLen;
		file.seekg(idOffset - 4);
		file.read((char *)&nameLen, 4);

		std::vector<char> name(nameLen + 1, 0);
		file.read(name.data(), nameLen);

		std::cout << "[" << i << "] " << name.data() << "\n";
		std::cout << "  Samplers: " << samplersCount << "\n";
		std::cout << "  Channels: " << channelsCount << "\n";

		// Read sampler details
		if (samplersCount > 0)
		{
			uint64_t samplersOffset = animPos + 12 + samplersOffsetRel;
			file.seekg(samplersOffset);

			for (uint32_t s = 0; s < samplersCount; s++)
			{
				int32_t interp, inType, inComp, inSrc, outType, outComp, outSrc;
				file.read((char *)&interp, 4);
				file.read((char *)&inType, 4);
				file.read((char *)&inComp, 4);
				file.read((char *)&inSrc, 4);
				file.read((char *)&outType, 4);
				file.read((char *)&outComp, 4);
				file.read((char *)&outSrc, 4);

				std::cout << "    Sampler[" << s << "]: ";
				std::cout << "in_src=" << inSrc << " (" << inComp << " comp), ";
				std::cout << "out_src=" << outSrc << " (" << outComp << " comp)\n";
			}
		}

		std::cout << "\n";
	}

	file.close();
	return 0;
}
