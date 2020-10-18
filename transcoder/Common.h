#ifndef COMMON_H_
#define COMMON_H_

#include <stdint.h>
#include <string>
#include <vector>

using namespace std;

class Common {
public:
	Common();

	/* Type conversion */
	static uint32_t stringToUint32(string str);
	static float    stringToFloat(string str);
	static double   stringToDouble(string str);
	static string   uint32ToString(uint32_t value);

	/* String Handling */
	static vector<string> splitString(string str, string separator);
	static uint32_t       subStringCount(string str, string subStr);
	static string         removeAllSubStrings(string str, string subStr);
};


#endif /* COMMON_H_ */
