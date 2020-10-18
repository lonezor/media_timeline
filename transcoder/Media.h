#ifndef MEDIA_H_
#define MEDIA_H_

#include <string>

using namespace std;

class Media {
public:

	enum Resolution {
		RESOLUTION_UNKNOWN = 0,
		RESOLUTION_1K      = 1,
		RESOLUTION_2K      = 2,
		RESOLUTION_4K      = 3,
	};

	Media();

};




#endif /* MEDIA_H_ */
