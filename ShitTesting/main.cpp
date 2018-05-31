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
#include <thread>
#include "../VULKAN/include/glm/glm.hpp"

#include "FU.h"
#include "../vulkan/VulkanLoader.h"

#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_print.hpp"
#include "Callback.h"
#include <sstream>

using namespace std;


#define BREAKPOINT(cond) do \
{ \
    if(!(cond)) \
    { \
        __debugbreak(); \
    } \
} while(0);

typedef void (*FuncPtr)();

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


class Wait
{
public:

    ~Wait()
    {
        Pause();
    }
};

static Wait w;

class State
{
public:
    State()
    {

    }
    virtual ~State(){}

    virtual void Execute() = 0;
    virtual bool IsLeaf() = 0;
    virtual State* GetSelectedState() const = 0;
    virtual void Link(){};
    virtual void SetLastParrent(State* pState) = 0;
    virtual void Start(){};
    virtual void Stop(){};
protected:
   
};

template<typename T>
class StateImpl : public State
{
public:
    StateImpl()
        //: m_outs(nullptr)
        : m_selectedState(nullptr)
        , m_lastParrent(nullptr)
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
         unsigned int uIndex = static_cast<unsigned int>(index);
         BREAKPOINT(m_outs[uIndex]);
         m_selectedState = m_outs[uIndex];
         m_selectedState->SetLastParrent(this);
    }

    void SetLastParrent(State* pState) final
    {
        m_lastParrent = pState;
    }
private:
    std::array<State*, (unsigned int)T::Count> m_outs;
    State*          m_selectedState;
    State*          m_lastParrent;
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

enum class ELoop
{
    Loop,
    Out,
    Count
};

class DoneEvent : public Event
{
public:
    typedef Callback1<void, DoneEvent*> EventCallback;
    void Send()
    {
        Channel<DoneEvent>::SendEvent(this);
    }
};

class LoopState : public StateImpl<ELoop>
{
public:
    LoopState()
        : m_done(false)
        , m_listener(DoneEvent::EventCallback(this, &LoopState::OnEvent))
    {
        Link(ELoop::Loop, this);
        Channel<DoneEvent>::AddListener(&m_listener);
    }

    void Execute() override
    {
        if (m_done)
        {
            SelectOutState(ELoop::Out);
        }
        else
        {
            SelectOutState(ELoop::Loop);
        }
    }

    void Start() override
    {
    }

    void Stop() override
    {
    }

    void OnEvent(DoneEvent* e)
    {
        m_done = true;
    }

protected:
    Listener<DoneEvent>     m_listener;

private:
    bool    m_done;
};

class EndState : public StateImpl<EEmpty>
{
public:
    void Execute()
    {
        std::cout << "It's about sending a message!" << std::endl;
    }

    void Stop() override
    {
        std::cout << "EndState: STOP" << std::endl;
    }

    void Start() override
    {
        std::cout << "EndState: START" << std::endl;
    }
};


class Serializer;
class ISeriable;
class PropertyGeneric
{
public:
	virtual void Save(rapidxml::xml_node<char>* objNode, Serializer* serializer, ISeriable* obj) = 0;
	virtual void Load(rapidxml::xml_node<char>* objNode, Serializer* serializer, ISeriable* obj) = 0;
};

class ISeriable
{
public:
	ISeriable(const std::string& nodeName)
		: m_name(nodeName)
	{}

	virtual void Serialize(Serializer* serializer) = 0;

	const std::string& GetName() const { return m_name; }
	void SetName(const std::string& name) { m_name = name; }

private:
	std::string m_name;
};


class Serializer
{
public:
	Serializer()
		: m_isSaving(true)
		, m_currentNode(nullptr)
	{
		auto root = GetNewNode(GetNewString("objects"));
		m_document.append_node(root);
	}

	void SerializeProperty(PropertyGeneric* prop, ISeriable* obj)
	{
		if (m_isSaving)
			prop->Save(m_currentNode, this, obj);
		else
			prop->Load(m_currentNode, this, obj);

	}

	void BeginSerializing(ISeriable* obj)
	{
		if (m_isSaving)
		{
			auto newNode = GetNewNode(obj->GetName().c_str());
			m_nodeStack.push_back(newNode);
			m_currentNode = newNode;
		}
		else //loading step
		{
			if (!m_currentNode)
			{
				auto root = m_document.first_node();
				m_currentNode = root->first_node();
			}
			else
			{
				if (m_nodeStack.empty())
					m_currentNode = m_currentNode->next_sibling(obj->GetName().c_str());
				else
					m_currentNode = m_currentNode->first_node(obj->GetName().c_str());
			}
			BREAKPOINT(m_currentNode); //no node has been found.
			m_nodeStack.push_back(m_currentNode);
		}
	}

	void EndSerializing(ISeriable* obj)
	{
		BREAKPOINT(!m_nodeStack.empty());
		auto last = m_nodeStack.back();
		BREAKPOINT(last == m_currentNode); //sanity check
		m_nodeStack.pop_back();
		auto newCurrentNode = m_currentNode;

		if (m_isSaving)
		{
			auto parrentNode = (m_nodeStack.empty()) ? m_document.first_node() : m_nodeStack.back();
			parrentNode->append_node(m_currentNode);
			newCurrentNode = (m_nodeStack.empty()) ? nullptr : m_nodeStack.back();
		}
		else
		{
			newCurrentNode = (m_nodeStack.empty()) ? m_currentNode : m_nodeStack.back();
		}
		m_currentNode = newCurrentNode;

	}


	rapidxml::xml_attribute<char>*	GetNewAttribute(const char* name = "", const char* val = "")
	{
		return m_document.allocate_attribute(name, val);
	}

	rapidxml::xml_node<char>* GetNewNode(const char* name = "")
	{
		return m_document.allocate_node(rapidxml::node_type::node_element, name);
	}

	char* GetNewString(const std::string& str)
	{
		return m_document.allocate_string(str.c_str());
	}

	void PrintContent()
	{
		std::stringstream ss;
		std::fstream f("objects.xml", std::ios_base::out);

		f << *m_document.first_node() << std::endl;
		//std::string content = ss.str();
		//std::cout << content;
		f.close();
	}

	void ToggleIsSaving() { m_isSaving = false; m_currentNode = nullptr; } //this one is debug
private:
	rapidxml::xml_node<char>*				m_currentNode;
	std::vector<rapidxml::xml_node<char>*>	m_nodeStack;

	rapidxml::xml_document<char>	m_document;
	bool							m_isSaving;

};

template<class T>
class SeriableImpl : public ISeriable
{
public:
	SeriableImpl(const std::string& nodeName)
		: ISeriable(nodeName)
	{}

	virtual void Serialize(Serializer* serializer) override
	{
		serializer->BeginSerializing(this);
		for (auto prop : T::PropertiesMap)
			serializer->SerializeProperty(prop, this);

		serializer->EndSerializing(this);
	}


protected:
	static std::vector<PropertyGeneric*> PropertiesMap;
};


#define BEGIN_PROPERTY_MAP(CLASSTYPE) std::vector<PropertyGeneric*> SeriableImpl<CLASSTYPE>::PropertiesMap = {

#define END_PROPERTY_MAP(CLASSTYPE) };

#define IMPLEMENT_PROPERTY(PTYPE, PNAME, PLABEL, CLASSTYPE) new Property<PTYPE, CLASSTYPE>(CLASSTYPE::Get##PNAME(), PLABEL)

#define DECLARE_PROPERTY(PTYPE, PNAME, CLASSTYPE)  protected: PTYPE PNAME; \
													public: static PTYPE CLASSTYPE::* Get##PNAME() { return &CLASSTYPE::PNAME; }

template<typename T, typename BASE> //default int
class Property : public PropertyGeneric
{
public:
	typedef T BASE::* PtmType;
	Property()
	{}
	Property(PtmType offset, const std::string& label)
		: m_ptm(offset)
		, m_label(label)
	{}

	virtual void Save(rapidxml::xml_node<char>* objNode, Serializer* serializer, ISeriable* obj)
	{
		BASE* cobj = dynamic_cast<BASE*>(obj);
		BREAKPOINT(cobj);

		auto prop = serializer->GetNewAttribute(m_label.c_str(), serializer->GetNewString(to_string(cobj->*m_ptm)));
		objNode->append_attribute(prop);
	};

	virtual void Load(rapidxml::xml_node<char>* objNode, Serializer* serializer, ISeriable* obj)
	{
		auto prop = objNode->first_attribute(m_label.c_str(), 0, false);
		BASE* cobj = dynamic_cast<BASE*>(obj);
		BREAKPOINT(cobj);

		cobj->*m_ptm = std::stoi(prop->value());

	};
private:
	PtmType						m_ptm;
	std::string					m_label;
};


template<typename BASE>
class Property<float, BASE> : public PropertyGeneric
{
public:
	typedef float BASE::* PtmType;
	Property()
	{}
	Property(PtmType offset, const std::string& label)
		: m_ptm(offset)
		, m_label(label)
	{}

	virtual void Save(rapidxml::xml_node<char>* objNode, Serializer* serializer, ISeriable* obj)
	{
		BASE* cobj = dynamic_cast<BASE*>(obj);
		BREAKPOINT(cobj);

		auto prop = serializer->GetNewAttribute(m_label.c_str(), serializer->GetNewString(to_string(cobj->*m_ptm)));
		objNode->append_attribute(prop);
	};

	virtual void Load(rapidxml::xml_node<char>* objNode, Serializer* serializer, ISeriable* obj)
	{
		auto prop = objNode->first_attribute(m_label.c_str(), 0, false);
		BASE* cobj = dynamic_cast<BASE*>(obj);
		BREAKPOINT(cobj);
		cobj->*m_ptm = std::stof(prop->value());

	};
private:
	PtmType						m_ptm;
	std::string					m_label;
};

template<typename BASE>
class Property<glm::vec3, BASE> : public PropertyGeneric
{
public:
	typedef glm::vec3 BASE::* PtmType;
	Property()
	{}
	Property(PtmType offset, const std::string& label)
		: m_ptm(offset)
		, m_label(label)
	{}

	virtual void Save(rapidxml::xml_node<char>* objNode, Serializer* serializer, ISeriable* obj)
	{
		BASE* cobj = dynamic_cast<BASE*>(obj);
		BREAKPOINT(cobj);

		auto node = serializer->GetNewNode(m_label.c_str());

		node->append_attribute(serializer->GetNewAttribute("x", serializer->GetNewString(to_string((cobj->*m_ptm)[0]))));
		node->append_attribute(serializer->GetNewAttribute("y", serializer->GetNewString(to_string((cobj->*m_ptm)[1]))));
		node->append_attribute(serializer->GetNewAttribute("z", serializer->GetNewString(to_string((cobj->*m_ptm)[2]))));

		objNode->append_node(node);
	};

	virtual void Load(rapidxml::xml_node<char>* objNode, Serializer* serializer, ISeriable* obj)
	{
		auto node = objNode->first_node(m_label.c_str(), 0, false);
		BREAKPOINT(node);
		BASE* cobj = dynamic_cast<BASE*>(obj);
		BREAKPOINT(cobj);

		(cobj->*m_ptm)[0] = std::stof(node->first_attribute("x")->value()); //need some checks for null
		(cobj->*m_ptm)[1] = std::stof(node->first_attribute("y")->value());
		(cobj->*m_ptm)[2] = std::stof(node->first_attribute("z")->value());
	};
private:
	PtmType						m_ptm;
	std::string					m_label;
};

template<typename BASE>
class Property<std::string, BASE> : public PropertyGeneric
{
public:
	typedef std::string BASE::* PtmType;
	Property()
	{}
	Property(PtmType offset, const std::string& label)
		: m_ptm(offset)
		, m_label(label)
	{}

	virtual void Save(rapidxml::xml_node<char>* objNode, Serializer* serializer, ISeriable* obj)
	{
		BASE* cobj = dynamic_cast<BASE*>(obj);
		BREAKPOINT(cobj);

		auto prop = serializer->GetNewAttribute(m_label.c_str(), serializer->GetNewString(cobj->*m_ptm));
		objNode->append_attribute(prop);
	};

	virtual void Load(rapidxml::xml_node<char>* objNode, Serializer* serializer, ISeriable* obj)
	{
		auto prop = objNode->first_attribute(m_label.c_str(), 0, false);
		BASE* cobj = dynamic_cast<BASE*>(obj);
		BREAKPOINT(cobj);
		cobj->*m_ptm = std::string(prop->value());
	};
private:
	PtmType						m_ptm;
	std::string					m_label;
};




class TestSer : public SeriableImpl<TestSer>
{
public:

	TestSer() : SeriableImpl<TestSer>("testSer") 
	{};

	TestSer(float dick, int women, int men)
		: SeriableImpl<TestSer>("testSer")
		, dickLength(dick)
		, womenFucked(women)
		, menFucked(men)
	{
		mesaj = "muie cu cacat pe o pula de : " + to_string(dickLength);
	}

protected:
	DECLARE_PROPERTY(float, dickLength, TestSer);
	DECLARE_PROPERTY(int, womenFucked, TestSer);
	DECLARE_PROPERTY(int, menFucked, TestSer);
	DECLARE_PROPERTY(std::string, mesaj, TestSer);
};

BEGIN_PROPERTY_MAP(TestSer)
	IMPLEMENT_PROPERTY(float, dickLength, "dickLength", TestSer),
	IMPLEMENT_PROPERTY(int, womenFucked, "WomenFucked", TestSer),
	IMPLEMENT_PROPERTY(int, menFucked, "MenFucked", TestSer),
	IMPLEMENT_PROPERTY(std::string, mesaj, "Mesaj", TestSer)
END_PROPERTY_MAP(TestSer)

template<typename BASE>
class Property<TestSer, BASE> : public PropertyGeneric
{
public:
	typedef TestSer BASE::* PtmType;
	Property()
	{}
	Property(PtmType offset, const std::string& label)
		: m_ptm(offset)
		, m_label(label)
	{}

	virtual void Save(rapidxml::xml_node<char>* objNode, Serializer* serializer, ISeriable* obj)
	{
		BASE* cobj = dynamic_cast<BASE*>(obj);
		BREAKPOINT(cobj);
		(cobj->*m_ptm).SetName(m_label);
		(cobj->*m_ptm).Serialize(serializer);
	};

	virtual void Load(rapidxml::xml_node<char>* objNode, Serializer* serializer, ISeriable* obj)
	{
		BASE* cobj = dynamic_cast<BASE*>(obj);
		BREAKPOINT(cobj);
		(cobj->*m_ptm).SetName(m_label);
		(cobj->*m_ptm).Serialize(serializer);
	};

private:
	PtmType						m_ptm;
	std::string					m_label;
};

class ComplexTestSer : public SeriableImpl<ComplexTestSer>
{
public:

	ComplexTestSer()
		: SeriableImpl<ComplexTestSer>("complex")
		, testFloat(69.0f)
		, ser(2 * 21.6f, 3 * 10, 20)
		, position(1, 2, 3)
	{}

private:
	DECLARE_PROPERTY(float, testFloat, ComplexTestSer);
	DECLARE_PROPERTY( TestSer, ser, ComplexTestSer);
	DECLARE_PROPERTY(glm::vec3, position, ComplexTestSer);
};

BEGIN_PROPERTY_MAP(ComplexTestSer)
	IMPLEMENT_PROPERTY(float, testFloat, "test", ComplexTestSer),
	IMPLEMENT_PROPERTY(TestSer, ser, "testSerializer", ComplexTestSer),
	IMPLEMENT_PROPERTY(glm::vec3, position, "PoSItion", ComplexTestSer)
END_PROPERTY_MAP(ComplexTestSer)

int Sum(int a, int b)
{
	return a + b;
}


class Foo
{
public:
	Foo()
	{
		cout << "Ctor" << endl;
	}

	Foo(const Foo& other)
	{
		cout << "Copy ctor" << endl;
	}

	Foo(Foo&& other)
	{
		cout << "Move ctor" << endl;
	}

	Foo& operator=(const Foo& other)
	{
		cout << "Copy operator" << endl;
	}

	Foo& operator=(Foo&& other)
	{
		cout << "Move operator" << endl;
	}

	~Foo()
	{
		cout << "Dtor" << endl;
	}
private:
	int p;
};

Foo Func(Foo o)
{
	return o;
}

Foo Func()
{
	return Foo();
}

static std::vector<int> map = { 2 };

int GetInt()
{
	int a = 6;
	return a;
}

template<class A, class C>
class Printer
{
public:
	void Print(C c)
	{
		std::cout << "Ia Pula: ERROR" << endl;
	}
};

template <class A>
class Printer<A, int> 
{
public:
	void Print(int i)
	{
		std::cout << "Ia Pula: int " << std::endl;
	}
};


template<class A, class P1, class P2>
class Printer<A, std::pair<P1, P2> >
{
public:
	void Print(std::pair<P1, P2> p)
	{
		std::cout << "Ia Pula: Pair " << std::endl;

	}
};

int main (int argc, char** argv)
{
	/*TestSer t(21.6f, 10, 20);
	TestSer t2(13.0f, 2, 0);
	ComplexTestSer ct;

	Serializer serializer;
	t.Serialize(&serializer);
	t2.Serialize(&serializer);
	ct.Serialize(&serializer);

	serializer.PrintContent();

	serializer.ToggleIsSaving();
	TestSer tloads[2];
	tloads[0].Serialize(&serializer);
	tloads[1].Serialize(&serializer);
	ComplexTestSer ctload;
	ctload.Serialize(&serializer);
*/
	
    return 0;
}
