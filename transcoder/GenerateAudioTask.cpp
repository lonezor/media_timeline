#include "GenerateAudioTask.h"
#include "FileSystem.h"
#include "File.h"
#include "general.h"
#include "ShellCmd.h"
#include "Common.h"

#include <libgen.h>
#include <string.h>

// In the long run the difference between using AAC and FLAC becomes irrelevant. Only images and videos are expected to change with significant increase
// of resolution. Audio stays more or less the same (up to 24bit 192kHz). FLAC is used.

// sox: to mono, 'remix -'

GenerateAudioTask::GenerateAudioTask(File* file, string targetDirName) {
	this->file          = file;
	this->targetDirName = targetDirName;
}

/*-------------------------------------------------------------------------------------------------------------------*/

void GenerateAudioTask::execute() {
	bool result;
	char cmd[2048];

	printf("[%s GenerateAudioTask] %s %s\n", __FUNCTION__,
		   file->getPath().c_str(), targetDirName.c_str());

	string tmpDir = file->createTmpDir();

	result = FileSystem::makedir(targetDirName);
	if (!result) {
		printf("[%s] failed to create directory %s\n", __FUNCTION__, targetDirName.c_str());
		return;
	}

	if (file->type == File::FILE_TYPE_MID) {
		int i;
		char outBaseSuffix[128];
		char outBasePath[1024];

		snprintf(outBaseSuffix, sizeof(outBaseSuffix), "_FluidR3");
		snprintf(outBasePath, sizeof(outBasePath), "%s/%s%s", tmpDir.c_str(), file->getHashString().c_str(), outBaseSuffix);

		// Timidity using Fluid R3 soundfont
		snprintf(cmd, sizeof(cmd), "timidity --output-24bit -A120 \"%s\" -Ow -o \"%s_tmp.wav\"",
			file->getPath().c_str(), outBasePath);
		printf("cmd: '%s'\n", cmd);
		system(cmd);

		snprintf(cmd, sizeof(cmd), "sox \"%s_tmp.wav\" \"%s/%s%s.flac\" norm -3", // norm-3 to leave headroom for playback-resampling
						outBasePath, targetDirName.c_str(), file->getHashString().c_str(), outBaseSuffix);
		printf("cmd: '%s'\n", cmd);
		system(cmd);

		file->appendOutputSuffixes(string(outBaseSuffix) + ".flac");

		// Adlib OPL3 (Yamaha YMF262) player using all available FM patches (from old DOS games)
		for(i=0; i < 66; i++) {
			snprintf(outBaseSuffix, sizeof(outBaseSuffix), "_OPL3_%02d",  i);
			snprintf(outBasePath, sizeof(outBasePath), "%s/%s%s", tmpDir.c_str(), file->getHashString().c_str(), outBaseSuffix);

			snprintf(cmd, sizeof(cmd), "adlmidi \"%s\" -nl -w \"%s_tmp.wav\" %d",
					file->getPath().c_str(), outBasePath, i);
			printf("cmd: '%s'\n", cmd);
			system(cmd);

			snprintf(cmd, sizeof(cmd), "sox \"%s_tmp.wav\" \"%s/%s%s.flac\" norm -3", // norm-3 to leave headroom for playback-resampling
						outBasePath, targetDirName.c_str(), file->getHashString().c_str(), outBaseSuffix);
			printf("cmd: '%s'\n", cmd);
			system(cmd);

			file->appendOutputSuffixes(string(outBaseSuffix) + ".flac");
		}

		// Roland MT-32 emulation via munt
		snprintf(outBaseSuffix, sizeof(outBaseSuffix), "_Roland_MT-32");
		snprintf(outBasePath, sizeof(outBasePath), "%s/%s%s", tmpDir.c_str(), file->getHashString().c_str(), outBaseSuffix);

		snprintf(cmd, sizeof(cmd), "mt32emu-smf2wav -m /usr/share/sounds/rom/ \"%s\" -o \"%s_tmp.wav\"", // note: rom path must end with /
			file->getPath().c_str(), outBasePath);
		printf("cmd: '%s'\n", cmd);
		system(cmd);

		snprintf(cmd, sizeof(cmd), "sox \"%s_tmp.wav\" \"%s/%s%s.flac\" norm -3 rate 48000", // norm-3 to leave headroom for playback-resampling
						outBasePath, targetDirName.c_str(), file->getHashString().c_str(), outBaseSuffix);
		printf("cmd: '%s'\n", cmd);
		system(cmd);

		file->appendOutputSuffixes(string(outBaseSuffix) + ".flac");
	}
	else if (file->type == File::FILE_TYPE_MOD)
	{
		char outBasePath[1024];

		snprintf(outBasePath, sizeof(outBasePath), "%s/%s", tmpDir.c_str(), file->getHashString().c_str());

		// Timidity MOD playback
		snprintf(cmd, sizeof(cmd), "timidity --output-24bit -A120 \"%s\" -Ow -o \"%s_tmp.wav\"",
			file->getPath().c_str(), outBasePath);
		printf("cmd: '%s'\n", cmd);
		system(cmd);

		snprintf(cmd, sizeof(cmd), "sox \"%s_tmp.wav\" -r 48000 -b 16 \"%s/%s.flac\" norm -3", // norm-3 to leave headroom for playback-resampling
						outBasePath, targetDirName.c_str(), file->getHashString().c_str());
		printf("cmd: '%s'\n", cmd);
		system(cmd);

		file->appendOutputSuffixes(".flac");
	}
	else if (file->type == File::FILE_TYPE_SID)
	{
		snprintf(cmd, sizeof(cmd), "cd %s && sidplay2 -w \"%s\"",
						tmpDir.c_str(), file->getPath().c_str());
		printf("cmd: '%s'\n", cmd);
		system(cmd);
		
		// Convert tracks by probing the file increments
		int i = 0;
		while (true) {
			i++;
			string baseName = string(basename((char*)file->getPath().c_str()));
			char trackPath[1024] = {0};
			char trackPath2[1024] = {0};
			snprintf(trackPath, sizeof(trackPath), "%s/%s", tmpDir.c_str(), baseName.c_str());
			#define SID_ENDING ".sid"
			trackPath[strlen(trackPath) - strlen(SID_ENDING)] = 0; // remove ending
			
			if (i == 1) {
				snprintf(trackPath2, sizeof(trackPath2), "%s", trackPath);
			}

			char suffix[64];
			snprintf(suffix, sizeof(suffix), "[%d].wav", i);
			strcat(trackPath, suffix);

			if (i == 1) {
				snprintf(suffix, sizeof(suffix), ".wav");
				strcat(trackPath2, suffix);
			}

			char outSuffix[128];
			snprintf(outSuffix, sizeof(outSuffix), "_%02d.flac", i);

			if (FileSystem::fileExists(string(trackPath))) {
				snprintf(cmd, sizeof(cmd), "sox \"%s\" \"%s/%s%s\" norm -3 rate 48000", // norm-3 to leave headroom for playback-resampling
					trackPath,
					targetDirName.c_str(),
					file->getHashString().c_str(),
					outSuffix);
					printf("cmd: '%s'\n", cmd);
					system(cmd);

					file->appendOutputSuffixes(string(outSuffix));
			} else if (FileSystem::fileExists(string(trackPath2))) {
				snprintf(cmd, sizeof(cmd), "sox \"%s\" \"%s/%s%s\" norm -3 rate 48000", // norm-3 to leave headroom for playback-resampling
					trackPath2,
					targetDirName.c_str(),
					file->getHashString().c_str(),
					i);
					printf("cmd: '%s'\n", cmd);
					system(cmd);	

					file->appendOutputSuffixes(string(outSuffix));			
			} else {
				break; // no more files
			}
		}
	}
	else if (file->type == File::FILE_TYPE_MP2 || file->type == File::FILE_TYPE_MP3 ||
		     file->type == File::FILE_TYPE_WAV || file->type == File::FILE_TYPE_FLAC ||
			 file->type == File::FILE_TYPE_AAC) {

		// Sox doesn't support all encoding formats. Convert to WAV via FFmpeg and then sox to FLAC
		snprintf(cmd, sizeof(cmd), "ffmpeg -i \"%s\" \"%s/%s_tmp.wav\"", // note: rom path must end with /
			file->getPath().c_str(), tmpDir.c_str(), file->getHashString().c_str());
		printf("cmd: '%s'\n", cmd);
		system(cmd);

		snprintf(cmd, sizeof(cmd), "sox \"%s/%s_tmp.wav\" \"%s/%s.flac\" norm -3 rate 48000", // norm-3 to leave headroom for playback-resampling
					tmpDir.c_str(),
					file->getHashString().c_str(),
					targetDirName.c_str(),
					file->getHashString().c_str());
		printf("cmd: '%s'\n", cmd);
		system(cmd);

		file->appendOutputSuffixes(".flac");
	}

	file->deleteTmpDir();

	file->writeInfoFile(targetDirName + "/" + file->getHashString() + ".info");

	// Move original file
	FileSystem::moveFile(file->getPath(), targetDirName + "/" + file->getHashString());
}

/*-------------------------------------------------------------------------------------------------------------------*/