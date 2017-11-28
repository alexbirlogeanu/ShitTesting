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
#include <typeinfo>
#include <unordered_set>
#include <memory>
#include <array>

#include "Event.h"
#include <crtdbg.h>
#include <windows.h>
#include <random>
#include <string>
#include <stdlib.h>
#include <stdint.h>
#include <limits>

#include "FU.h"
#include "../vulkan/VulkanLoader.h"

#include "rapidxml/rapidxml.hpp"
#include "Callback.h"

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

class Base
{
public:
    virtual ~Base()
    {}

    virtual void f()
    {
        std::cout << "Base::f" << std::endl;
    }

    void g()
    {
        std::cout << "Base::g" << std::endl;

    }
};

class Derived : public Base
{
public:
    virtual ~Derived()
    {
    }

    void f() override
    {
        std::cout << "Derived::f" << std::endl;
    }

    void g()
    {
        std::cout << "Derived::g" << std::endl;

    }
};
class GenericItem
{
public:
    GenericItem()
        : m_ptr(nullptr)
    {}

    virtual ~GenericItem(){}

    inline void* Get()
    {
        return m_ptr;
    }
protected:
    inline void Set(void* ptr)
    {
        m_ptr = ptr;
    }
private:
    void* m_ptr;
};

template <typename T>
class Item : public GenericItem
{
public:
    Item(T* ptr)
    {
        Set(ptr);
    }

    virtual ~Item(){}
};

class Container
{
public:
    Container()
    {
        m_items.resize(10);
    }

    template<typename T>
    T GetAs(unsigned int index)
    {
        Item<T>* item = dynamic_cast<Item<T>*>(m_items[index]);
        BREAKPOINT(item && "Not correct type");
        return *(T*)item->Get();
    }

    template<typename T>
    void SetAs(T* ptr, unsigned int index)
    {
        m_items[index] = new Item<T>(ptr);
    }
private:
    std::vector<GenericItem*> m_items;
};

class Callable
{
public:
    virtual ~Callable()
    {}
};

//template<typename TR, typename R>
//class Callback0
//{
//public:
//    typedef R (TR::*f)();
//
//    Callback0()
//        : m_obj(nullptr)
//    {}
//
//    Callback0(TR* o, f func)
//        : m_obj(o)
//        , m_func(func)
//    {
//    }
//
//    R operator()()
//    {
//        return (m_obj->*m_func)();
//    }
//private:
//    TR*                 m_obj;
//    f                   m_func;
//};

//template<typename TR, typename R, typename T1>
//class Callback1
//{
//public:
//    typedef R (TR::*f)(T1);
//
//    Callback1()
//        : m_obj(nullptr)
//        , m_func(nullptr)
//    {}
//
//    Callback1(TR* o, f func)
//        : m_obj(o)
//        , m_func(func)
//    {
//    }
//
//    R operator()(T1 p1)
//    {
//        return (m_obj->*m_func)(p1);
//    }
//private:
//    TR*                 m_obj;
//    f                   m_func;
//};

struct P
{
    int a;
    int b;

    P(){};
    P(int f1, int f2) : a(f1), b(f2){}
};


class A
{
public:
    A() 
        : result(5)
    {}

    virtual ~A()
    {
    }

    void f()
    {
        cout << "A::f" << endl;
    }

    int GetResult() const
    {
        return result;
    }

    virtual void PrintResult()
    {
        cout << "A::Result: " << GetResult() << endl;
    }

    virtual int g(int r)
    {
        cout << "A::g: " << r << endl;
        return ++r;
    }

    virtual int Compute(P p)
    {
        cout << "A::Compute" << endl;
        result = p.a + p.b;
        return result;
    }

    int Compute2(P& p)
    {
        cout << "A::Compute2" << endl;
        p.a *= 2;
        result = p.a + p.b;
        return result;
    }
protected:
    int result;
};

class B : public A 
{
public:
    virtual ~B()
    {
    }

    void NotOk()
    {
        cout << "Not Ok" << endl;
    }

    virtual void PrintResult()
    {
        cout << "B::Result: " << GetResult() << endl;
    }

    void f()
    {
        cout << "B::f" << endl;
    }

    virtual int Compute(P p)
    {
        cout << "B::Compute" << endl;
        result = p.a * p.b;
        return result;
    }
};

class C : public B
{
public:
    int Compute(P p)
    {
        cout << "C::Compute" << endl;
        result = p.a - p.b;
        return result;
    }

    virtual void PrintResult()
    {
        cout << "C::Result: " << GetResult() << endl;
    }
};

class Wait
{
public:

    ~Wait()
    {
        Pause();
    }
};

static Wait w;

template <typename T>
struct has_typedef_foobar {
    // Types "yes" and "no" are guaranteed to have different sizes,
    // specifically sizeof(yes) == 1 and sizeof(no) == 2.
    typedef char yes[1];
    typedef char no[2];

    template <typename C>
    static yes& test(typename C::foobar*);

    template <typename>
    static no& test(...);

    // If the "sizeof" of the result of calling test<T>(0) would be equal to sizeof(yes),
    // the first overload worked and T has a nested type named foobar.
    static const bool value = sizeof(test<T>(0)) == sizeof(yes);
};

struct foo {    
    typedef float foobar;
};

class State
{
public:
    State()
    {}
    virtual ~State(){}

    virtual void Execute() = 0;
    virtual bool IsLeaf() = 0;
    virtual State* GetSelectedState() const = 0;
    virtual void Link(){};
};

template<typename T>
class StateImpl : public State
{
public:
    StateImpl()
        //: m_outs(nullptr)
        : m_selectedState(nullptr)
    {
    }

    virtual ~StateImpl(){}
    virtual void Execute() = 0;
    State* GetSelectedState() const final
    {
        return m_selectedState;
    }

    bool IsLeaf()  final
    {
        return m_outs.size() == 0;
    }
    void Link(T index, State* state) 
    { 
        BREAKPOINT(index < T::Count);
        m_outs[(unsigned int)index] = state;
    }

protected:
    //methods
    void SelectOutState(T index)
    {
         BREAKPOINT(index < T::Count);
         unsigned int uIndex = (unsigned int)index;
         BREAKPOINT(m_outs[uIndex]);
         m_selectedState = m_outs[uIndex];
    }

private:
    std::array<State*, (unsigned int)T::Count> m_outs;
    State*          m_selectedState;
};

enum class EBinary
{
    False,
    True,
    Count
};

enum class EEmpty
{
    Count
};
class BinaryState : public StateImpl<EBinary>
{
public:
    BinaryState(bool defaultValue)
        : m_value(defaultValue)
    {}

    void Execute()
    {
        std::cout << "Executing Binary State with value= " <<  ((m_value)? "True" : "False") << std::endl;
        EBinary outIndex = EBinary::False;
        if (m_value)
            outIndex = EBinary::True;

        SelectOutState(outIndex);
    }
    
private:
    bool        m_value;
};

class NoState : public StateImpl<EEmpty>
{
public:
    void Execute()
    {
        std::cout << "Transitioning to No State" << std::endl;
    }
};

class YesState : public StateImpl<EEmpty>
{
public:
    void Execute()
    {
        std::cout << "Transitioning to Yes State" << std::endl;
    }
};

int main (int argc, char** argv)
{
    /*std::cout << std::boolalpha;
    std::cout << has_typedef_foobar<int>::value << std::endl;
    std::cout << has_typedef_foobar<foo>::value << std::endl;*/


    BinaryState trueState(true);
    BinaryState falseState(false);
    NoState noState;
    YesState yesState;

    trueState.Link(EBinary::True, &falseState);
    trueState.Link(EBinary::False, &noState);

    falseState.Link(EBinary::True, &yesState);
    falseState.Link(EBinary::False, &noState);

    State* currentState;
    currentState = &trueState; //starting state

    while (!currentState->IsLeaf())
    {
        currentState->Execute();
        currentState = currentState->GetSelectedState();
    }

    if (currentState)
        currentState->Execute();

    return 0;
}
