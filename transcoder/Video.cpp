#include "Video.h"
#include "ShellCmd.h"
#include "Common.h"

#include <string.h>
#include <stdio.h>

Video::Video(File* file) {
	this->file = file;
}

void Video::readMetaData() {
	bool result;
	string cmd;
	string cmdResp;

	cmd = string("/usr/bin/ffprobe  -v error -of flat=s=_ -select_streams v:0 -show_entries stream=height,width,duration -i ")
			+ "\"" + file->getPath() + "\"";

	cmdResp = ShellCmd::execute(cmd);

	if (cmdResp != "") {
		vector<string> propertyVector;
		vector<string> valueVector;

		propertyVector = Common::splitString(cmdResp, "\n");

		for(vector<string>::iterator it = propertyVector.begin(); it != propertyVector.end(); ++it) {
			string p = *it;

			printf("string '%s'\n", p.c_str());

			if (Common::subStringCount(p, "streams_stream_0_width")) {
				valueVector = Common::splitString(p, "=");
				width = Common::stringToUint32(valueVector[1]);
				printf("width=%d\n", width);

			} else if (Common::subStringCount(p, "streams_stream_0_height")) {
				valueVector = Common::splitString(p, "=");
				height = Common::stringToUint32(valueVector[1]);
				printf("height=%d\n", height);
			} else if (Common::subStringCount(p, "streams_stream_0_duration")) {
				valueVector = Common::splitString(p, "=\"");
				duration = Common::stringToFloat(valueVector[1]);
				printf("duration=%f\n", duration);
			}
		}
	}

	if (duration < 0.001) {
		cmd = string("/usr/bin/mplayer -identify -frames 0 -vo null -nosound ")
			+ "\"" + file->getPath() + "\" 2>&1 | awk -F= '/LENGTH/{print $2}'";
		cmdResp = ShellCmd::execute(cmd);

		if (cmdResp != "") {
		duration = Common::stringToFloat(cmdResp);
		printf("duration=%03f\n", duration);
		}
	}
}

Image* Video::getImage(string timestamp) {
	string cmd;
	File* f;
	Image* image;
	bool result;
	string cmdResp;
	/* Extract frame from video and save it in /tmp under hash name *
	 * to avoid file name collisions. 	                            */
	f = new File("/tmp", file->getHashString() + ".png");
	cmd = "/usr/bin/ffmpeg -y -threads 1 -i \"" + file->getPath() + "\" -ss " + timestamp +
		  " -vframes 1 " + f->getPath();

	printf("cmd: %s\n", cmd.c_str());
	cmdResp = ShellCmd::execute(cmd);
	//printf("cmdResp: \n%s", cmdResp.c_str());

	/* Load image */
	image = new Image(f);
	result = image->loadImage();
	if (!result) {
		delete f;
		delete image;
		return NULL;
	}

	/* Remove file from temporary dir */
	cmd = "rm -fv " + f->getPath();
	printf("cmd: %s\n", cmd.c_str());
	cmdResp = ShellCmd::execute(cmd);
	printf("cmdResp: \n%s", cmdResp.c_str());

	delete f;

	image->width  = width;
	image->height = height;

    return image;
}
