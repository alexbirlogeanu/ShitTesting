#pragma once
//////////////////////////////////////////////////////////////////////////
//REMAINDER: all classes derived from Event should typedef a EventCallback like:
//  typedef Callback1<void, DERIVEDEVENT*> EventCallback;
//////////////////////////////////////////////////////////////////////////

class Event
{
public:
    virtual ~Event(){}

    virtual void Send() = 0;
protected:
};



template<typename EVENT>
class Listener
{
public:
    typedef typename EVENT::EventCallback EventCB;
    Listener(EventCB& cb) 
        : m_cb(std::move(cb))
    {
    }

    void Consume(EVENT* e)
    {
        m_cb(e);
    }

private:
    EventCB     m_cb;
};

template<typename EVENT>
class Channel
{
public:
    static void SendEvent(EVENT* e)
    {
        auto list = GetInstance()->m_listeners;
        for(auto it = list.begin(); it != list.end(); ++it)
            (*it)->Consume(e);
    }

    static void AddListener(Listener<EVENT>* listener)
    {
        GetInstance()->m_listeners.insert(listener);
    }

    static void RemoveListener(Listener<EVENT>* listener)
    {
        GetInstance()->m_listeners.erase(listener);
    }

private:
    static Channel* GetInstance()
    {
        if (!ms_instance)
            ms_instance = new Channel();

        return ms_instance;
    }
    static Channel*                             ms_instance;
    std::unordered_set<Listener<EVENT>*>        m_listeners;
};
template<typename EVENT> Channel<EVENT>* Channel<EVENT>::ms_instance = nullptr;
