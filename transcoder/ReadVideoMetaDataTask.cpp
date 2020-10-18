#include "ReadVideoMetaDataTask.h"
#include "Video.h"

ReadVideoMetaDataTask::ReadVideoMetaDataTask(File* file) {
	this->file = file;
}

void ReadVideoMetaDataTask::execute() {
	Video* video;

	if (file->typeIsVideo()) {

		video = new Video(file);

		video->readMetaData();
		file->addMetaData("Video", "");
		file->addMetaData("Video.Width", to_string(video->width));
		file->addMetaData("Video.Height", to_string(video->height));
		file->addMetaData("Video.Duration", to_string(video->duration));

		delete video;
	}
}
