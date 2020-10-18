#ifndef READIMAGEMETADATATASK_H_
#define READIMAGEMETADATATASK_H_


#include "Task.h"
#include "File.h"

#include <string>

using namespace std;

/* The purpose of this class is to work around FreeImage limitation with
 * functions that are not thread safe (it is clearly documented in API description).
 * All instantiations of this task will execute on the same thread.
 */
class ReadImageMetaDataTask : Task {
public:
	ReadImageMetaDataTask(File* file);
    virtual void execute(void);

private:
    void              readImageMetaData(FIBITMAP* bitmap, FREE_IMAGE_MDMODEL model, File* file);
    void              loadImageFile(File* file, FIBITMAP** bitmap);
    void              readMetaData(File* file, FIBITMAP* bitmap);
    uint32_t          getFreeImageLoadFlags(File::FileType fileType);
    FREE_IMAGE_FORMAT getFreeImageFormat(File::FileType fileType);

    File*  file;
};


#endif /* READIMAGEMETADATATASK_H_ */
