#include <windows.h>

#include "rapidxml/rapidxml.hpp"
#include "../VULKAN/defines.h"
#include <iostream>
#include <fstream>
#include <string>
#include <cstdint>

#define SRCXML "src/shaderlist.xml"
#define SRCDIR "src/"

typedef rapidxml::xml_document<char> TXmlDoc;
typedef rapidxml::xml_node<char>      TXmlNode;
typedef rapidxml::xml_attribute<char> TXmlAttribute;

TXmlNode* GetNodeVerbose(TXmlNode* parrent, char* name = nullptr)
{
    TXmlNode* node = parrent->first_node(name);
    if (!node)
        std::cout << "ERROR: Node: " << name << " not found for node " << parrent->name() << "!" << std::endl;

    return node;
}

TXmlNode* GetNodeVerbose(TXmlDoc* parrent, char* name = nullptr)
{
    TXmlNode* node = parrent->first_node(name);
    
    if (!node)
        std::cout << "ERROR: Node: " << name << " not found for node " << parrent->name() << "!" << std::endl;
    return node;
}

TXmlAttribute* GetAttributeVerbose(TXmlNode* parrent, char* name = nullptr)
{
    TXmlAttribute* att = parrent->first_attribute(name);
    if (!att)
        std::cout << "ERROR: Attribute: " << name << " not found for node " << parrent->name() << "!" << std::endl;
    return att;
}

void Pause()
{
    std::cout << "Press any key to continue...";
    getchar();
}

void NotifyShaderCompilingEnded()
{
    HWND wndHandle =  FindWindow(WNDCLASSNAME, WNDNAME);
    if(wndHandle != NULL)
    {
        COPYDATASTRUCT msg;
        ZeroMemory(&msg, sizeof(COPYDATASTRUCT));
        msg.dwData = MSGSHADERCOMPILED;

        LRESULT r = SendMessage(wndHandle, WM_COPYDATA, (WPARAM)0, (LPARAM)&msg);
        TRAP(r == S_OK);
        if(r != S_OK)
            std::cout << "ERROR: Notify failed!" << std::endl;
    }
    else
    {
        std::cout << "Notification skipped! Reason: Main app its not running" << std::endl;
    }
}


bool ReadXmlFile(const std::string& xml, char** fileContent)
{
    std::ifstream hXml (xml, std::ifstream::in);
    
    if (!hXml.is_open())
    {
        std::cout << "Shader list its not found!" << std::endl;
        return false;
    }

    hXml.seekg(0, std::ios_base::end);
    std::streamoff size = hXml.tellg();
    hXml.seekg(0, std::ios_base::beg);
    *fileContent = new char[size + 1];
    char* line = new char[512];
    uint64_t offset = 0;
    while(true)
    {
        hXml.getline(line, 512);
        if(hXml.eof())
            break;

        TRAP(!hXml.fail());
        std::streamoff bytes = hXml.gcount();
        memcpy(*fileContent + offset, line, bytes - 1); //exclude \0
        offset += bytes - 1; //gcount count delim too
    }
    (*fileContent)[offset] = '\0';
    hXml.close();

    return true;
}

void GetOutputDir(TXmlDoc& xml, std::string& outDir)
{
    bool useDefault = true;
    TXmlNode* outNode = GetNodeVerbose(&xml, "outputdir");
    if (outNode)
    {
        TXmlAttribute* outAtt = GetAttributeVerbose(outNode, "dir");
        if (outAtt)
        {
            outDir = std::string(outAtt->value());
            useDefault = false;
        }
    }

    if (useDefault)
    {
        std::cout << "No output directory found! Defaulting to bin/ " << std::endl;
        outDir = std::string("bin/");
    }
}

bool CompileShader(const char* const shaderFile, const char* const outFile, const std::string& outDir)
{
    STARTUPINFO startInfo;
    ZeroMemory(&startInfo, sizeof(STARTUPINFO));
    startInfo.cb = sizeof(startInfo);

    PROCESS_INFORMATION pInfo;
    ZeroMemory(&pInfo, sizeof(PROCESS_INFORMATION));

    std::string outLocation = outDir + "/" + outFile;
    std::string nonConstShaderFile = shaderFile;
    std::string commandLine = SRCDIR;
    commandLine += "glslangValidator.exe -V "; 
    commandLine += SRCDIR + nonConstShaderFile + " -o " + outLocation;

    char data[128];
    memcpy(data, commandLine.data(), commandLine.size());
    data[commandLine.size()] = '\0';

    BOOL result = CreateProcess(
        NULL,
        data,
        NULL,
        NULL,
        FALSE,
        0,
        NULL,
        NULL,
        &startInfo,
        &pInfo
        );

    DWORD exitCode = -1;
    if(result)
    {
        WaitForSingleObject(pInfo.hProcess, INFINITE);

        GetExitCodeProcess(pInfo.hProcess, &exitCode);

        CloseHandle(pInfo.hProcess);
        CloseHandle(pInfo.hThread);
    }

    return (exitCode == 0);
}

bool CompileAllShaders(TXmlDoc& xml, const std::string& outDir, const std::string& specificShader)
{
    TXmlNode* shaderListNode = GetNodeVerbose(&xml, "shaderlist");

    if(!shaderListNode)
        return false;

    TXmlNode* currShaderNode = shaderListNode->first_node();
    unsigned int nbTotalCompiledShaders = 0;
    while(currShaderNode)
    {
        if ( std::string(currShaderNode->name()).compare("shader") == 0)
        {
            TXmlAttribute* fileNameAtt = GetAttributeVerbose(currShaderNode, "shaderfile");
            TXmlAttribute* outFileNameAtt = GetAttributeVerbose(currShaderNode, "out");

            bool isValid = fileNameAtt && outFileNameAtt;

            if(specificShader.empty() || (!specificShader.empty() && specificShader.compare(fileNameAtt->value()) == 0))
            {
                if(isValid)
                    isValid &= CompileShader(fileNameAtt->value(), outFileNameAtt->value(), outDir);

                if (isValid)
                    ++nbTotalCompiledShaders;
                else
                {
                    std::cout << "ERROR! " << fileNameAtt->value() << " shader failed to compile! Aborting the rest of compilation process" << std::endl;
                    return false;
                }
            }
        }

        currShaderNode = currShaderNode->next_sibling();
    }

    std::cout << "LOG: Number of compiled shaders: " << nbTotalCompiledShaders << std::endl;
    return true;
}

int main(int argc, char* argv[])
{
    char* xmlContent;
    std::string outputDir;
    std::string searchShader;
    if(argc > 1)
        searchShader = argv[1];

    if (ReadXmlFile(SRCXML, &xmlContent))
    {
        TXmlDoc xml;
        xml.parse<rapidxml::parse_default>(xmlContent);
        GetOutputDir(xml, outputDir);

        if (CompileAllShaders(xml, outputDir, searchShader))
            NotifyShaderCompilingEnded();
    }

    Pause();
    return 0;
}