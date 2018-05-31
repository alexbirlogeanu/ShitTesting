#include <string>
#include <unordered_set>
#include <vector>
#include <fstream>
#include <iostream>

#include "MeshLoader.h"
#include "SVertex.h"

#include "include/rapidxml/rapidxml.hpp"
#include "defines.h"

#pragma comment(lib, "assimp.lib")

void ReadXmlFile(const std::string& xmlFile, char** fileContent)
{
	std::ifstream hXml(xmlFile, std::ifstream::in);
	TRAP(hXml.is_open());

	hXml.seekg(0, std::ios_base::end);
	std::streamoff size = hXml.tellg();
	hXml.seekg(0, std::ios_base::beg);
	*fileContent = new char[size + 1];
	char* line = new char[512];
	uint64_t offset = 0;
	while (true)
	{
		hXml.getline(line, 512);
		if (hXml.eof())
			break;

		TRAP(!hXml.fail());
		std::streamoff bytes = hXml.gcount();
		memcpy(*fileContent + offset, line, bytes - 1); //exclude \0
		offset += bytes - 1; //gcount count delim too
	}
	(*fileContent)[offset] = '\0';
	hXml.close();
}

void Binarize(const std::string& file)
{
	std::cout << "Binarize: " << file << "..." << std::endl;

	std::vector<SVertex> vertices;
	std::vector<unsigned int> indexes;
	std::string outExt = "mb";

	MeshLoader loader;
	loader.LoadInto(file, &vertices, &indexes);

	std::size_t pos = file.find_first_of('.');
	TRAP(pos != std::string::npos);

	std::string newFileName = file.substr(0, pos + 1) + outExt;
	std::fstream outFile(newFileName, std::ios_base::out | std::ios_base::binary);

	TRAP(outFile.is_open());

	outFile << vertices.size();
	outFile.write((const char*)vertices.data(), vertices.size() * sizeof(SVertex));
	outFile << indexes.size();
	outFile.write((const char*)indexes.data(), indexes.size() * sizeof(unsigned int));

	outFile.close();
}

int main(int argc, char** argv)
{
	static std::string meshLibrary = "obj/mesh_library.xml";
	char* xmlContent = nullptr;
	ReadXmlFile(meshLibrary, &xmlContent);

	rapidxml::xml_document<char> doc;
	doc.parse<rapidxml::parse_default>(xmlContent);

	rapidxml::xml_node<char>* root = doc.first_node("meshes", 0, false);
	TRAP(root);
	std::unordered_set<std::string> meshList; //used for not binarize same mesh multiple times
	for (auto child = root->first_node("mesh", 0, false); child != nullptr; child = child->next_sibling())
	{
		rapidxml::xml_attribute<char>* file = child->first_attribute("file", 0, false);
		TRAP(file);
		meshList.insert(std::string(file->value()));
	}

	for (auto it = meshList.begin(); it != meshList.end(); ++it)
		Binarize(*it);

	return 0;
}