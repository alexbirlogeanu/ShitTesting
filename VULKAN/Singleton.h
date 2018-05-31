#pragma once

#include "defines.h"

template<typename T>
class Singleton
{
public:
	static void CreateInstance()
	{
		if (!ms_instance)
			ms_instance = new T();
	}

	static void DestroyInstance()
	{
		if (ms_instance)
			delete ms_instance;
	}

	static T* GetInstance()
	{
		TRAP(ms_instance);
		return ms_instance;
	}

protected:
	Singleton(){}
	virtual ~Singleton(){};

	Singleton(const Singleton& other);
	Singleton& operator= (const Singleton& other);

private:
	static T*		ms_instance;
};

template<typename T>
T* Singleton<T>::ms_instance = nullptr;