#define _CRTDBG_MAP_ALLOC
#include <iostream>
#include <fstream>

#include <stdlib.h>
#include <cmath>
#include <vector>
#include <unordered_map>
#include <utility>
#include <string>
#include <cassert>
#include <ctime>
#include <functional>
#include <queue>

#include <crtdbg.h>
#include <windows.h>
#include <random>
#include <string>
#include <stdlib.h>
#include <stdint.h>
#include <limits>

#include "FU.h"

#include "rapidxml/rapidxml.hpp"

using namespace std;


#define BREAKPOINT(cond) do \
{ \
    if(!(cond)) \
    { \
        __debugbreak(); \
    } \
} while(0);

typedef void (*FuncPtr)();

class Sparam 
{
public:
    unsigned int id;
    unsigned int value;
    void* address;
    FuncPtr f;

    Sparam()
    {
        address = nullptr;
        value = 0;
    }

    Sparam(unsigned int i) : id(i)
    {
        address = nullptr;
        value = 0;
    }

    virtual void DoSomething()
    {
    }
};
template<typename T,
         typename FUNC>
class SpecificParam1 : public Sparam
{
public:
    SpecificParam1(unsigned int i, FUNC f) : Sparam(i)
    {
        this->f = (FuncPtr)f;
    }

    virtual void DoSomething() override
    {
        ((FUNC)f)(id);
    }
private:
    //FUNC f;
};



template<typename T,
    typename FUNC>
class SpecificParam2 : public Sparam
{
public:
    SpecificParam2(unsigned int i, FUNC f) : Sparam(i)
    {
        address = &id;
        this->f = (FuncPtr)f;
    }

    virtual void DoSomething() override
    {
        value = ((FUNC)f)((T*)address);
    }
private:
    //FUNC f;
};
#define ELEMS 200

typedef void (*func1)(unsigned int);
typedef unsigned int (*func2)(int*);


typedef SpecificParam1<int, func1> IntParam; 
typedef SpecificParam2<int, func2> PtrParam;

void intFunc(unsigned int i)
{
    cout << "INT: " << i << endl;
}

unsigned int PtrFunc(int* ptr)
{
    cout << "PTR: " << *ptr << endl;
    return (*ptr);
}

int SumCopy(std::vector<int> vec)
{
    int sum = 0;
    for(auto it = vec.begin(); it != vec.end(); ++it)
    {
        sum += (*it);
    }
    return sum;
}

int SumRef(std::vector<int>& vec)
{
    int sum = 0;
    for(auto it = vec.begin(); it != vec.end(); ++it)
    {
        sum += (*it);
    }
    return sum;
}

class A
{
public:
    char a : 2;
    bool b : 1;
};

int power(int b, int x)
{
    int result = 1;
    while(x)
    {
        
        if(x & 1)
        {
            result *= b;
        }
        x >>= 1;
        b *= b;
    }
    return result;
}

class V
{
public:
    const int* p;
    V(const vector<int>& v)
    {
        p = v.data();
    }
};

class Device
{
};

template<class VKTYPE, class P = Device>
class VkHandle
{
private:
    VKTYPE m_handle;
    P m_parent;
};

class Buffer
{
};


class CThread
{
public:
    CThread()
        : m_handle(NULL)
        , m_threadId(~0)
    {
    };

    void SetParams(void* data) { m_threadParams.params = data; };

    void Create()
    {
        m_threadParams.threadToStart = this;
        SECURITY_ATTRIBUTES sa;
        ZeroMemory(&sa, sizeof(SECURITY_ATTRIBUTES));
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        m_handle = CreateThread(&sa,
            0,
            CThread::StartThreadProc,
            (LPVOID)&m_threadParams,
            CREATE_SUSPENDED,
            &m_threadId);
    }

    void Create(void* params)
    {
        m_threadParams.params = params;
        Create();
    }
    void Start()
    {
        if(m_handle)
        {
            ResumeThread(m_handle);
        }
        else
        {
            std::cout << "Handle is invalid" << std::endl;
        }

    };

    void Wait()
    {
        if(m_handle)
        {
            WaitForSingleObject(m_handle, INFINITE);
        }
    };

    virtual ~CThread()
    {
        CloseHandle(m_handle);
    };
protected:
    virtual bool DoWork(void* params) = 0;
private:
    struct SStartThreadParams
    {
        CThread*        threadToStart;
        void*           params;

        SStartThreadParams() 
            : threadToStart(nullptr)
            , params(nullptr)
        {
        }
    };
    static DWORD WINAPI StartThreadProc(LPVOID params)
    {
        SStartThreadParams casted_params = *(SStartThreadParams*)params;
        return casted_params.threadToStart->DoWork(casted_params.params);
    }
    std::function<DWORD(LPVOID)>    m_threadProcPtr;
    HANDLE                          m_handle;
    DWORD                           m_threadId;
    SStartThreadParams              m_threadParams;
};

struct SProcessInfo
{
    std::string vert;
    std::string frag;
    std::string out;
};

class CCreateProcessThread : public CThread
{
public:
    CCreateProcessThread()
        : m_dataMutex(NULL)
        , m_logHandle(NULL)
        , m_isQuiting(false)
    {
        m_dataMutex = CreateMutex(NULL,
            FALSE,
            NULL);

        assert(m_dataMutex);
        ZeroMemory(&m_processInfo, sizeof(PROCESS_INFORMATION));
    }

     ~CCreateProcessThread()
     {
         CloseHandle(m_dataMutex);
         CloseHandle(m_logHandle);
         CloseProcces();
     }

     void Quit() { m_isQuiting = true; }

     void PushRequest(SProcessInfo& info)
     {
         WaitForSingleObject(m_dataMutex, INFINITE);
         m_pendingRequest.push(info);
         assert(ReleaseMutex(m_dataMutex));
     }

protected:
    bool DoWork(void* params) override
    {
        OpenLog();
        DWORD waitResult;

        while(true)
        {
            bool closeProcess;
            if(IsProcessFinished(closeProcess))
            {
                if(closeProcess)
                {
                    CloseProcces();
                }

                waitResult = WaitForSingleObject(m_dataMutex, 0);
                if(waitResult == WAIT_OBJECT_0)
                {
                    if(m_pendingRequest.empty())
                    {
                        if(m_isQuiting)
                        {
                            assert(ReleaseMutex(m_dataMutex));
                            return true;
                        }
                    }
                    else
                    {
                        SProcessInfo& info = m_pendingRequest.front();
                        StartProcess(info.vert, info.frag, info.out);
                        m_pendingRequest.pop();

                    }
                    assert(ReleaseMutex(m_dataMutex));
                }
                
            }
            
            Yield();
        }

        return true;
    }
private:
    void OpenLog()
    {
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(sa);
        sa.lpSecurityDescriptor = NULL;
        sa.bInheritHandle = TRUE;

        m_logHandle = CreateFile("log.txt",
            GENERIC_WRITE | GENERIC_READ,
            FILE_SHARE_READ,
            &sa,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL);

        assert(m_logHandle);
    }

    void StartProcess(const std::string& vert, const std::string& frag, const std::string& out)
    {
        char cmdLine[128]; //= "glslangValidator.exe -V spv.test.vert spv.test.frag -o test.spv";
        std::string cmdStr = "glslangValidator.exe -V " + vert + " " + frag + " -o " + out;
        memcpy(cmdLine, cmdStr.data(), cmdStr.size());
        cmdLine[cmdStr.size()] = '\0';

        DWORD creationFlags = 0; // = CREATE_NEW_CONSOLE;
        STARTUPINFO startInfo;
        memset(&startInfo, 0, sizeof(STARTUPINFO));
        startInfo.cb = sizeof(STARTUPINFO);

        startInfo.dwFlags = STARTF_USESTDHANDLES;
        startInfo.hStdError = m_logHandle;
        startInfo.hStdOutput = m_logHandle;

        ZeroMemory(&m_processInfo, sizeof(PROCESS_INFORMATION));

        BOOL res = CreateProcess(NULL,
            cmdLine,
            NULL,
            NULL,
            TRUE,
            creationFlags,
            NULL,
            NULL,
            &startInfo,
            &m_processInfo
            );
    }

    bool IsProcessFinished(bool& needCloseProcess)
    {
        needCloseProcess = false;
        if(m_processInfo.hProcess)
        {
            DWORD result = WaitForSingleObject(m_processInfo.hProcess, 0l);
            needCloseProcess = (result == WAIT_OBJECT_0);
            return !(result == WAIT_TIMEOUT);
        }
        return true;
    }

    void CloseProcces()
    {
        CloseHandle(m_processInfo.hProcess);
        CloseHandle(m_processInfo.hThread);
        ZeroMemory(&m_processInfo, sizeof(PROCESS_INFORMATION));
    }

    bool                        m_isQuiting;
    std::queue<SProcessInfo>   m_pendingRequest;
    HANDLE m_dataMutex;
    HANDLE m_logHandle;
    PROCESS_INFORMATION m_processInfo;
};

class CSafeBool
{
private:
    typedef void (CSafeBool::* safe_bool_func)();

    void SafeBoolFunc() {}
public:
    operator safe_bool_func () const
    {
        return IsTrue()? &CSafeBool::SafeBoolFunc : nullptr;
    }
protected:
    virtual bool IsTrue() const { return true; }
};

class CTestSafeBool :  public CSafeBool
{
public:
    CTestSafeBool() : m_value(0){}
    CTestSafeBool(int val) : m_value(val) {}
protected:
    virtual bool IsTrue() const override { return m_value != 0; }

    int m_value;
};

class MoveTestClass
{
public:
    MoveTestClass()
        : id(global_id++)
    {
        
        std::cout << "Default Ctor: " << id << std::endl;
    }

    ~MoveTestClass()
    {
        std::cout << "Dtor: " << id << std::endl;
    }

    MoveTestClass(const MoveTestClass& other)
        : id(other.id)
    {
        std::cout << "Copy ctor: " << id << std::endl;
    }

    MoveTestClass(MoveTestClass&& other)
        : id(other.id)
    {
        std::cout << "Move ctor: " << id << std::endl;
    }

    static int global_id;
private:
    int id;
};
int MoveTestClass::global_id = 0;
void Pause()
{
    std::cout << "Press any key to continue...";
    getchar();
}

MoveTestClass CreateCopy()
{
    MoveTestClass a;
    return a;
}

MoveTestClass CreateCopyFrom(const MoveTestClass& other)
{
    return other;
}

int main (int argc, char** argv)
{
    MoveTestClass a;// = CreateCopy();
    MoveTestClass b = CreateCopyFrom(a);

    Pause();
    return 0;
}