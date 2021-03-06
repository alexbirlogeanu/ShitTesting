#pragma once

#include <memory>

template<typename R>
class CB_Base0
{
public:
    virtual ~CB_Base0()
    {}

    virtual R operator()() = 0;
};

template<typename R>
class Callback0
{
private:
    template<typename C, typename F>
    class CBImpl : public CB_Base0<R>
    {
    public:
        CBImpl(C* o, F f) : m_obj(o), m_func(f) {}
        virtual ~CBImpl() { }
        R operator()()
        {
             return (m_obj->*m_func)();
        }

    private:
        C*      m_obj;
        F       m_func;
    };

	std::shared_ptr<CB_Base0<R>>   m_cb;
public:
    Callback0() {}
    template<typename C, typename F>
    Callback0(C* obj, F func) { m_cb.reset(new CBImpl<C,F>(obj, func));}
	Callback0(Callback0&& other) { m_cb = std::move(other.m_cb); }
	Callback0(const Callback0& other){ m_cb = other.m_cb; }

    virtual ~Callback0() { }

    R operator()() { return (*m_cb)(); }
private:
   
};
//=====================================================CB1===================
template<typename R, typename T1>
class CB_Base1
{
public:
    virtual ~CB_Base1()
    {}

    virtual R operator()(T1) = 0;
};

template<typename R, typename T1>
class Callback1
{
private:
    template<typename C, typename F>
    class CBImpl : public CB_Base1<R,T1>
    {
    public:
        CBImpl(C* o, F f) : m_obj(o), m_func(f) {}
        R operator()(T1 p)
        {
            return (m_obj->*m_func)(p);
        }

    private:
        C*      m_obj;
        F       m_func;
    };

    std::shared_ptr<CB_Base1<R,T1>>   m_cb;
public:
    Callback1() {};

    template<typename C, typename F>
    Callback1(C* obj, F func) { m_cb.reset(new CBImpl<C,F>(obj, func));}
	Callback1(Callback1&& other) { m_cb = std::move(other.m_cb); }
	Callback1(const Callback1& other){ m_cb = other.m_cb; };

    virtual ~Callback1() { }
    
	R operator()(T1 p) { return (*m_cb)(p); }
private:

};
//===============================================