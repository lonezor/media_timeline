#ifndef TASK_H_
#define TASK_H_

#include <string>

#include "File.h"
#include <FreeImage.h>

/* A Task is a work item for the slave threads. It needs to be self-contained and provide
 * all information needed to carry out the work. This way dependencies are reduced to other
 * system parts. The master thread is producer of tasks and the slave threads consumers.
 */

using namespace std;

/*-------------------------------------------------------------------------------------------------------------------*/

class Task {
public:
	virtual ~Task(void);
	virtual void execute(void) = 0;

	void setSharedData(void* sharedThreadData);

protected:
	 void* sharedThreadData;
};

/*-------------------------------------------------------------------------------------------------------------------*/

#endif /* TASK_H_ */
