#ifndef SOUND_H
#define SOUND_H

#include <string>
#include <vector>
#include <filesystem>
#include "libs/stb_image.h"
#include "libs/imgui/imgui.h"
#include "libs/miniaudio.h" // library for audio playback

const std::filesystem::path soundPath("data/sound/"); // default directory with all '.wav' files

class Sound
{
  public:
	ma_sound sound;					 // currently active sound instance
	ma_engine engine;				 // audio engine instance managing playback and sound resources
	bool soundPlaying;				 // flag that controls sound play button
	unsigned int playIcon, stopIcon; // texture IDs for sound play button
	int selectedSound;				 // index of the currently selected sound file path

	Sound(bool doNothing) {}

	Sound()
	{
		// setup sound engine
		soundPlaying = false;

		ma_engine_init(NULL, &engine);

		int width, height, nrChannels;
		unsigned char *data = stbi_load("aux_docs/button_play.png", &width, &height, &nrChannels, 0);
		glGenTextures(1, &playIcon);
		glBindTexture(GL_TEXTURE_2D, playIcon);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		stbi_image_free(data);

		data = stbi_load("aux_docs/button_stop.png", &width, &height, &nrChannels, 0);
		glGenTextures(1, &stopIcon);
		glBindTexture(GL_TEXTURE_2D, stopIcon);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		stbi_image_free(data);
	}

	//! Searches for '.wav' sound files on disk that contain model file name.
	void searchSoundFiles(std::string fname, std::vector<std::string> &sounds)
	{
		selectedSound = 0;

		std::string baseFileName = std::filesystem::path(fname).stem().string();

		for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(soundPath))
		{
			if (!entry.is_regular_file())
				continue;

			std::filesystem::path entryPath = entry.path();

			if (entryPath.extension() != ".wav")
				continue;

			// skip the file if its name doesn't contain the model file name
			if (entryPath.stem().string().find(baseFileName) == std::string::npos)
				continue;

			sounds.push_back(soundPath.string() + entryPath.filename().string());
		}
	}

	//! Updates Dear ImGui sound interface each frame (sound selector and play button; shown only if a sound is detected).
	void updateSoundUI(std::vector<std::string> &sounds)
	{
		if (!sounds.empty())
		{
			ImGui::Spacing();
			ImGui::Text("Sounds: %d", (int)sounds.size());
			ImGui::Spacing();

			// sound selector
			ImGui::SetNextWindowSize(ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * sounds.size() + 15.0f), ImGuiCond_None);

			ImGui::PushItemWidth(130.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 6));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.59f, 0.98f, 0.4f));

			if (ImGui::BeginCombo("##sound_selector", strrchr(sounds[selectedSound].c_str(), '/') + 1))
			{
				for (int i = 0, n = sounds.size(); i < n; i++)
					if (ImGui::Selectable(sounds[i].c_str(), selectedSound == i)) // if an item is clicked (selector returned true), update the selected sound
						selectedSound = i;

				ImGui::EndCombo();
			}

			ImGui::PopItemWidth();
			ImGui::PopStyleVar(1);
			ImGui::PopStyleColor(1);

			ImGui::SameLine();

			// sound play button
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4.0f);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));

			if (ImGui::ImageButton("##sound_play_button", soundPlaying ? stopIcon : playIcon, ImVec2(25, 25)))
			{
				if (!soundPlaying)
				{
					ma_result result = ma_sound_init_from_file(&engine, sounds[selectedSound].c_str(), 0, NULL, NULL, &sound);

					if (result == MA_SUCCESS)
					{
						ma_sound_start(&sound);
						soundPlaying = true;
					}
				}
				else
				{
					ma_sound_stop(&sound);
					ma_sound_uninit(&sound);
					soundPlaying = false;
				}
			}

			ImGui::PopStyleColor(3);

			if (soundPlaying && !ma_sound_is_playing(&sound))
			{
				ma_sound_uninit(&sound);
				soundPlaying = false;
			}
		}
	}
};

#endif
