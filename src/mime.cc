#include "mime.h"

bool Mime::readMimeConfig(string filename) 
{
	vector<string> tokens;

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

			for (int j = tokens.size(); j > 1; j--) {
				this->mimemap[tokens[j-1]] = tokens[0];
			}
		}
	}

	file.close();
}

string Mime::getMimeFromExtension(string filename) 
{
	// Find the extension (assumes the extension is whatever follows the last '.')
	string file_extension = filename.substr(filename.rfind(".")+1, filename.length());
	
	if (this->mimemap.find(file_extension) == this->mimemap.end()) 
	    return "text/plain";
	else
		return this->mimemap[file_extension];
}