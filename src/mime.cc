#include "mime.h"

bool Mime::readMimeConfig(string filename) 
{
	vector<string> tokens;
	map<string, string> mimemap;

	ifstream file(filename.c_str());
	if (!file.is_open()) {
		cerr << "Error: can't open " << filename << endl;
		return false;
	}

	string tmp_line;
	while (!getline(file, tmp_line).eof()) {
		tokens.clear();

		// if not comment, process
		if (tmp_line[0] != '#') {

			// tokenize the line
			string buf; 
			stringstream ss(tmp_line); 
			while (ss >> buf)
				tokens.push_back(buf);
		}

		for (int j = tokens.size(); j > 0; j--) {
			mimemap[tokens[j-1]] = tokens[0];
		}
	}

	file.close();

/*
	map<string, string>::iterator iter;
	for(iter = mimemap.begin(); iter != mimemap.end(); iter++) {
		cout << iter->first << " : " << iter->second << endl;
	} 
*/

}

string Mime::getMimeFromExtension(string filename) 
{
	
	
}