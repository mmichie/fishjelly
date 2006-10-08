#include "global.h"
#include <string>
#include <vector>

class Token {
	public:
		void tokenize(const string& str, 
		vector<string>& tokens, 
		const string& delimiters);
};