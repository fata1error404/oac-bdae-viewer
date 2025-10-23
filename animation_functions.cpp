// Animation functions to be added to parserBDAE.cpp

//! Load animations from a separate .bdae animation file.
void Model::loadAnimations(const char *animationFilePath)
{
	LOG("\n\033[1m\033[97mLoading animations from ", animationFilePath, "\033[0m");

	// Open animation file
	CPackPatchReader *animArchive = new CPackPatchReader(animationFilePath, true, false);
	if (!animArchive)
	{
		LOG("[Error] Failed to open animation archive");
		return;
	}

	IReadResFile *animFile = animArchive->openFile("little_endian_not_quantized.bdae");
	if (!animFile)
	{
		LOG("[Error] Failed to open animation file inside archive");
		delete animArchive;
		return;
	}

	// Read header
	BDAEFileHeader header;
	animFile->read(&header, sizeof(header));

	// Read entire file into buffer
	int animFileSize = animFile->getSize();
	char *animBuffer = (char *)malloc(animFileSize);
	animFile->seek(0);
	animFile->read(animBuffer, animFileSize);

	// Helper function to read string at offset
	auto readString = [&](uint32_t offset) -> std::string
	{
		if (offset < 4 || offset >= animFileSize)
			return "";
		int length;
		memcpy(&length, animBuffer + offset - 4, sizeof(int));
		if (length <= 0 || length > 200)
			return "";
		return std::string(animBuffer + offset, length);
	};

	// Helper function to parse source data
	auto parseSourceData = [&](BDAEint sourceOffset, DataType dataType, int elementCount) -> std::vector<float>
	{
		std::vector<float> result;
		if (sourceOffset >= animFileSize)
			return result;

		for (int i = 0; i < elementCount; i++)
		{
			float value = 0.0f;
			switch (dataType)
			{
			case DataType::BYTE:
			{
				int8_t val;
				memcpy(&val, animBuffer + sourceOffset + i * 1, 1);
				value = (float)val;
				break;
			}
			case DataType::UBYTE:
			{
				uint8_t val;
				memcpy(&val, animBuffer + sourceOffset + i * 1, 1);
				value = (float)val;
				break;
			}
			case DataType::SHORT:
			{
				int16_t val;
				memcpy(&val, animBuffer + sourceOffset + i * 2, 2);
				value = (float)val;
				break;
			}
			case DataType::USHORT:
			{
				uint16_t val;
				memcpy(&val, animBuffer + sourceOffset + i * 2, 2);
				value = (float)val;
				break;
			}
			case DataType::INT:
			{
				int32_t val;
				memcpy(&val, animBuffer + sourceOffset + i * 4, 4);
				value = (float)val;
				break;
			}
			case DataType::UINT:
			{
				uint32_t val;
				memcpy(&val, animBuffer + sourceOffset + i * 4, 4);
				value = (float)val;
				break;
			}
			case DataType::FLOAT:
			{
				memcpy(&value, animBuffer + sourceOffset + i * 4, 4);
				break;
			}
			}
			result.push_back(value);
		}
		return result;
	};

	// Find SLibraryAnimations (at offsetData + 56)
	BDAEint libAnimOffset = header.offsetData + 56;
	uint32_t animCount, animOffsetRel;
	memcpy(&animCount, animBuffer + libAnimOffset, sizeof(uint32_t));
	memcpy(&animOffsetRel, animBuffer + libAnimOffset + 4, sizeof(uint32_t));

	BDAEint animArrayOffset = libAnimOffset + 4 + animOffsetRel;

	LOG("Animation count: ", animCount);

	// Find SAnimationData (sources array) - search for size field = animCount * 2
	BDAEint sourcesOffset = 0;
	uint32_t expectedSourcesCount = animCount * 2;

	for (BDAEint pos = header.offsetData; pos < animFileSize - 8; pos++)
	{
		uint32_t count;
		memcpy(&count, animBuffer + pos, sizeof(uint32_t));
		if (count == expectedSourcesCount)
		{
			sourcesOffset = pos;
			break;
		}
	}

	if (sourcesOffset == 0)
	{
		LOG("[Warning] Could not find SAnimationData structure");
		free(animBuffer);
		delete animFile;
		delete animArchive;
		return;
	}

	LOG("Found SAnimationData at offset: 0x", std::hex, sourcesOffset, std::dec);

	// Parse sources array
	uint32_t sourcesCount;
	memcpy(&sourcesCount, animBuffer + sourcesOffset, sizeof(uint32_t));
	BDAEint sourcesArrayOffset = sourcesOffset + 4;

	LOG("Sources count: ", sourcesCount);

	// Read source descriptors
	struct SourceDesc
	{
		uint32_t count;
		BDAEint dataOffset;
	};

	std::vector<SourceDesc> sourceDescs;
	for (uint32_t i = 0; i < sourcesCount; i++)
	{
		BDAEint descPos = sourcesArrayOffset + i * 8;
		SourceDesc desc;
		uint32_t offsetRel;
		memcpy(&desc.count, animBuffer + descPos, sizeof(uint32_t));
		memcpy(&offsetRel, animBuffer + descPos + 4, sizeof(uint32_t));
		desc.dataOffset = descPos + 4 + offsetRel;
		sourceDescs.push_back(desc);
	}

	// Parse each animation
	const int ANIM_STRUCT_SIZE = 40;

	for (uint32_t animIdx = 0; animIdx < animCount; animIdx++)
	{
		BDAEint animPos = animArrayOffset + animIdx * ANIM_STRUCT_SIZE;

		// Read SAnimation structure
		uint32_t idOffset, idPadding;
		uint32_t samplersCount, samplersOffsetRel;
		uint32_t channelsCount, channelsOffsetRel;

		memcpy(&idOffset, animBuffer + animPos, sizeof(uint32_t));
		memcpy(&idPadding, animBuffer + animPos + 4, sizeof(uint32_t));

		BDAEint samplersBase = animPos + 8;
		memcpy(&samplersCount, animBuffer + samplersBase, sizeof(uint32_t));
		memcpy(&samplersOffsetRel, animBuffer + samplersBase + 4, sizeof(uint32_t));
		BDAEint samplersOffset = samplersBase + 4 + samplersOffsetRel;

		BDAEint channelsBase = animPos + 16;
		memcpy(&channelsCount, animBuffer + channelsBase, sizeof(uint32_t));
		memcpy(&channelsOffsetRel, animBuffer + channelsBase + 4, sizeof(uint32_t));
		BDAEint channelsOffset = channelsBase + 4 + channelsOffsetRel;

		std::string animName = readString(idOffset);

		Animation anim;
		anim.name = animName;
		anim.duration = 0.0f;

		LOG("\n[", animIdx, "] ", animName);
		LOG("  Samplers: ", samplersCount);
		LOG("  Channels: ", channelsCount);

		// Parse samplers
		for (uint32_t s = 0; s < samplersCount; s++)
		{
			BDAEint samplerPos = samplersOffset + s * 28;

			AnimationSampler sampler;
			int32_t interp, inType, inComp, inSrc, outType, outComp, outSrc;

			memcpy(&interp, animBuffer + samplerPos, sizeof(int32_t));
			memcpy(&inType, animBuffer + samplerPos + 4, sizeof(int32_t));
			memcpy(&inComp, animBuffer + samplerPos + 8, sizeof(int32_t));
			memcpy(&inSrc, animBuffer + samplerPos + 12, sizeof(int32_t));
			memcpy(&outType, animBuffer + samplerPos + 16, sizeof(int32_t));
			memcpy(&outComp, animBuffer + samplerPos + 20, sizeof(int32_t));
			memcpy(&outSrc, animBuffer + samplerPos + 24, sizeof(int32_t));

			sampler.interpolation = (InterpolationType)interp;
			sampler.inputSourceIndex = inSrc;
			sampler.outputSourceIndex = outSrc;
			sampler.inputType = (DataType)inType;
			sampler.inputComponentCount = inComp;
			sampler.outputType = (DataType)outType;
			sampler.outputComponentCount = outComp;

			// Parse keyframe source data if not already parsed
			if (inSrc >= 0 && inSrc < (int)sourcesCount)
			{
				// Ensure keyframeSources has enough space
				while ((int)keyframeSources.size() <= inSrc)
				{
					keyframeSources.push_back(KeyframeSource());
				}

				// Parse if not already parsed
				if (keyframeSources[inSrc].data.empty())
				{
					keyframeSources[inSrc].dataType = (DataType)inType;
					keyframeSources[inSrc].componentCount = inComp;
					keyframeSources[inSrc].data = parseSourceData(sourceDescs[inSrc].dataOffset, (DataType)inType, sourceDescs[inSrc].count);

					// Update animation duration from time values
					if (!keyframeSources[inSrc].data.empty())
					{
						float maxTime = *std::max_element(keyframeSources[inSrc].data.begin(), keyframeSources[inSrc].data.end());
						if (maxTime > anim.duration)
							anim.duration = maxTime;
					}
				}
			}

			if (outSrc >= 0 && outSrc < (int)sourcesCount)
			{
				while ((int)keyframeSources.size() <= outSrc)
				{
					keyframeSources.push_back(KeyframeSource());
				}

				if (keyframeSources[outSrc].data.empty())
				{
					keyframeSources[outSrc].dataType = (DataType)outType;
					keyframeSources[outSrc].componentCount = outComp;
					keyframeSources[outSrc].data = parseSourceData(sourceDescs[outSrc].dataOffset, (DataType)outType, sourceDescs[outSrc].count);
				}
			}

			anim.samplers.push_back(sampler);
		}

		// Parse channels
		for (uint32_t c = 0; c < channelsCount; c++)
		{
			BDAEint channelPos = channelsOffset + c * 24;

			AnimationChannel channel;
			uint32_t sourceOffset, sourcePadding, channelType, typePadding, reserved1, reserved2;

			memcpy(&sourceOffset, animBuffer + channelPos, sizeof(uint32_t));
			memcpy(&sourcePadding, animBuffer + channelPos + 4, sizeof(uint32_t));
			memcpy(&channelType, animBuffer + channelPos + 8, sizeof(uint32_t));
			memcpy(&typePadding, animBuffer + channelPos + 12, sizeof(uint32_t));
			memcpy(&reserved1, animBuffer + channelPos + 16, sizeof(uint32_t));
			memcpy(&reserved2, animBuffer + channelPos + 20, sizeof(uint32_t));

			channel.targetNodeName = readString(sourceOffset);
			channel.channelType = (ChannelType)channelType;
			channel.samplerIndex = c; // Channels and samplers are paired by index

			anim.channels.push_back(channel);
		}

		animations.push_back(anim);
		LOG("  Duration: ", anim.duration, " seconds");
	}

	free(animBuffer);
	delete animFile;
	delete animArchive;

	animationsLoaded = true;
	LOG("\n\033[1m\033[38;2;200;200;200m[Load] Loaded ", animations.size(), " animations.\033[0m\n");
}

//! Update animations based on elapsed time.
void Model::updateAnimations(float deltaTime)
{
	if (!animationPlaying || animations.empty())
		return;

	currentAnimationTime += deltaTime * animationSpeed;

	// Apply all animations (for now, play all at once)
	for (const Animation &anim : animations)
	{
		// Loop animation
		float time = fmod(currentAnimationTime, anim.duration);
		applyAnimation(anim, time);
	}

	// Recalculate world transforms after animation
	updateNodeTransforms();
}

//! Apply an animation at a specific time.
void Model::applyAnimation(const Animation &anim, float time)
{
	// For each channel, find the keyframes and interpolate
	for (const AnimationChannel &channel : anim.channels)
	{
		if (channel.samplerIndex < 0 || channel.samplerIndex >= (int)anim.samplers.size())
			continue;

		const AnimationSampler &sampler = anim.samplers[channel.samplerIndex];

		// Get time and keyframe data
		if (sampler.inputSourceIndex < 0 || sampler.inputSourceIndex >= (int)keyframeSources.size())
			continue;
		if (sampler.outputSourceIndex < 0 || sampler.outputSourceIndex >= (int)keyframeSources.size())
			continue;

		const KeyframeSource &timeSource = keyframeSources[sampler.inputSourceIndex];
		const KeyframeSource &keyframeSource = keyframeSources[sampler.outputSourceIndex];

		if (timeSource.data.empty() || keyframeSource.data.empty())
			continue;

		// Find the two keyframes to interpolate between
		int keyframeCount = timeSource.data.size();
		int keyframe0 = 0, keyframe1 = 0;
		float t = 0.0f;

		// Find keyframes
		for (int i = 0; i < keyframeCount - 1; i++)
		{
			if (time >= timeSource.data[i] && time <= timeSource.data[i + 1])
			{
				keyframe0 = i;
				keyframe1 = i + 1;
				float t0 = timeSource.data[i];
				float t1 = timeSource.data[i + 1];
				t = (time - t0) / (t1 - t0);
				break;
			}
		}

		// If time is past the last keyframe, use the last keyframe
		if (time >= timeSource.data[keyframeCount - 1])
		{
			keyframe0 = keyframe1 = keyframeCount - 1;
			t = 0.0f;
		}

		// Find target node
		auto nodeIt = nodeNameToIndex.find(channel.targetNodeName);
		if (nodeIt == nodeNameToIndex.end())
			continue;

		int nodeIndex = nodeIt->second;
		if (nodeIndex < 0 || nodeIndex >= (int)nodes.size())
			continue;

		Node &node = nodes[nodeIndex];

		// Apply animation based on channel type
		switch (channel.channelType)
		{
		case ChannelType::Translation:
		{
			int comp = sampler.outputComponentCount;
			if (comp == 3)
			{
				glm::vec3 v0(keyframeSource.data[keyframe0 * 3 + 0],
							 keyframeSource.data[keyframe0 * 3 + 1],
							 keyframeSource.data[keyframe0 * 3 + 2]);
				glm::vec3 v1(keyframeSource.data[keyframe1 * 3 + 0],
							 keyframeSource.data[keyframe1 * 3 + 1],
							 keyframeSource.data[keyframe1 * 3 + 2]);
				node.localPosition = interpolateVec3(v0, v1, t, sampler.interpolation);
			}
			break;
		}
		case ChannelType::Rotation:
		{
			int comp = sampler.outputComponentCount;
			if (comp == 4)
			{
				glm::quat q0(keyframeSource.data[keyframe0 * 4 + 3],  // w
							 keyframeSource.data[keyframe0 * 4 + 0],  // x
							 keyframeSource.data[keyframe0 * 4 + 1],  // y
							 keyframeSource.data[keyframe0 * 4 + 2]); // z
				glm::quat q1(keyframeSource.data[keyframe1 * 4 + 3],
							 keyframeSource.data[keyframe1 * 4 + 0],
							 keyframeSource.data[keyframe1 * 4 + 1],
							 keyframeSource.data[keyframe1 * 4 + 2]);

				// Apply negation to w component (same as in node parsing)
				q0.w = -q0.w;
				q1.w = -q1.w;

				node.localRotation = interpolateQuat(q0, q1, t, sampler.interpolation);
			}
			break;
		}
		case ChannelType::Scale:
		{
			int comp = sampler.outputComponentCount;
			if (comp == 3)
			{
				glm::vec3 v0(keyframeSource.data[keyframe0 * 3 + 0],
							 keyframeSource.data[keyframe0 * 3 + 1],
							 keyframeSource.data[keyframe0 * 3 + 2]);
				glm::vec3 v1(keyframeSource.data[keyframe1 * 3 + 0],
							 keyframeSource.data[keyframe1 * 3 + 1],
							 keyframeSource.data[keyframe1 * 3 + 2]);
				node.localScale = interpolateVec3(v0, v1, t, sampler.interpolation);
			}
			break;
		}
		default:
			break;
		}
	}
}

//! Interpolate between two floats.
float Model::interpolateFloat(float a, float b, float t, InterpolationType interp)
{
	switch (interp)
	{
	case InterpolationType::STEP:
		return a;
	case InterpolationType::LINEAR:
		return a + (b - a) * t;
	case InterpolationType::HERMITE:
		// Simplified hermite (cubic) interpolation
		return a + (b - a) * (t * t * (3.0f - 2.0f * t));
	default:
		return a;
	}
}

//! Interpolate between two vec3s.
glm::vec3 Model::interpolateVec3(const glm::vec3 &a, const glm::vec3 &b, float t, InterpolationType interp)
{
	return glm::vec3(
		interpolateFloat(a.x, b.x, t, interp),
		interpolateFloat(a.y, b.y, t, interp),
		interpolateFloat(a.z, b.z, t, interp));
}

//! Interpolate between two quaternions.
glm::quat Model::interpolateQuat(const glm::quat &a, const glm::quat &b, float t, InterpolationType interp)
{
	switch (interp)
	{
	case InterpolationType::STEP:
		return a;
	case InterpolationType::LINEAR:
	case InterpolationType::HERMITE:
		// Use spherical linear interpolation (slerp) for quaternions
		return glm::slerp(a, b, t);
	default:
		return a;
	}
}

//! Start playing animations.
void Model::playAnimation()
{
	animationPlaying = true;
}

//! Pause animations.
void Model::pauseAnimation()
{
	animationPlaying = false;
}

//! Reset animation to beginning.
void Model::resetAnimation()
{
	currentAnimationTime = 0.0f;
}
