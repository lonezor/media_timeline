#include "Common.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

uint32_t Common::stringToUint32(string str) {
	string tmpString = string(str.c_str());

	return (uint32_t) atoi(tmpString.c_str());
}

double Common::stringToDouble(string str) {
	string tmpString = string(str.c_str());

	return atof(tmpString.c_str());
}

float Common::stringToFloat(string str) {
	return (float) stringToDouble(str);
}

string Common::uint32ToString(uint32_t value) {
	char str[10];
	snprintf(str,sizeof(str),"%u", value);

	return string(str);
}

vector<string> Common::splitString(string str, string separator) {
	string tmpStr = string(str.c_str()); /* force deep string copy, the string tokenizer alters the string */
	vector<string> res;
	char* s;
	char* token = strtok_r((char*)tmpStr.c_str(), (const char*)separator.c_str(), &s);

	while(token != NULL) {
		res.push_back(string(token));
		token = strtok_r(NULL, separator.c_str(), &s);
	}

	return res;
}

uint32_t Common::subStringCount(string str, string subStr) {
	uint32_t          count = 0;
	string::size_type pos   = 0;

	while ((pos = str.find(subStr,pos)) != string::npos) {
		pos += subStr.length();
		count++;
	}

	return count;
}

string Common::removeAllSubStrings(string str, string subStr) {
	string::size_type pos;

	while ((pos = str.find(subStr)) != string::npos) {
		str.replace(pos, subStr.size(), "");
	};

	return str;
}

