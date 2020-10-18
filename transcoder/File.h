#ifndef FILE_H_
#define FILE_H_

#include <string>
#include <stdint.h>
#include <map>

#define FILE_HASH_SIZE 20

using namespace std;

class File {

public:

	enum FileType {
		FILE_TYPE_UNKNOWN = 0,

		/* Image Formats */
		FILE_TYPE_JPG     = 1,
		FILE_TYPE_GIF     = 2,
		FILE_TYPE_PNG     = 3,
		FILE_TYPE_HEIC    = 4,
		FILE_TYPE_TIF     = 5,

		/* Video Formats */
		FILE_TYPE_MTS     = 6,
		FILE_TYPE_MPG     = 7,
		FILE_TYPE_MKV     = 8,
		FILE_TYPE_AVI     = 9,
		FILE_TYPE_MP4     = 10,
		FILE_TYPE_WMV     = 11,
		FILE_TYPE_WEBM    = 12,
		FILE_TYPE_HASH    = 13,

		/* Audio Formats */
		FILE_TYPE_MID     = 14,
		FILE_TYPE_MP2     = 15,
		FILE_TYPE_MP3     = 16,
		FILE_TYPE_FLAC    = 17,
		FILE_TYPE_AAC     = 18,
		FILE_TYPE_WAV     = 19,
		FILE_TYPE_MOD     = 20,
		FILE_TYPE_SID     = 21,
	};

	File(string dirPath, string name);

	string                  dirPath;
	string                  name;
	FileType                type;
	bool                    origAnimation;
	uint32_t                size;
	uint32_t                fsModifiedTs;      /* Timestamp of last file change according to file system record  */
	uint8_t                 hash[FILE_HASH_SIZE];
	bool                    completelyProcessedImages;
	bool                    completelyProcessedVideos;
	uint32_t                completelyProcessedImagesTs;
	uint32_t                completelyProcessedVideosTs;
	std::map<string,string> imageMetaDataMap;
	std::map<string,string> audioMetaDataMap;
	std::map<string,string> supportedMetaDataKeyMap;
	bool                    imageRotationRequired;
	bool                    letterboxed;
	uint32_t                width;
	uint32_t                height;
	float                   duration;
	string                  outputSuffixes; // Comma separated string of file name extensions after sha1

	void   analyze();
	string getPath();
	string getNameWithoutFileType();
	string getFileEnding();
	void   print();
	void   populateSupportedMetaDataKeyMap(void);
	void   addMetaData(string key, string value);
	string getMetaData(string key);
	string getHashString(void);
	string getSrcFileInfo(void);

	bool writeHashFile();

	void appendOutputSuffixes(string suffix);

	uint32_t getWidth();
	uint32_t getHeight();
	uint32_t getDuration();

	string fileTypeSuffix();
	void determineFileType();
	void calculateCheckSum();
	void writeChecksumFile();
	void writeInfoFile(string path);
	void writeInfoFile_visual(string path);
	void writeInfoFile_audio(string path);
	string createTmpDir();
	void deleteTmpDir();

	bool typeIsImage();
	bool typeIsAnimation();
	bool typeIsVideo();
	bool typeIsAudio();
	bool typeIsHash();

	bool endsWith(const string& s, const string& suffix);
	void toLower(string& s);
};


#endif /* FILE_H_ */
