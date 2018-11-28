#pragma once

template<typename T>
class CDirtyValue
{
public:
	T c;
	T p;

	CDirtyValue()
		: c(0)
		, p(0)
		, m_forceDirty(true)
	{
	}

	bool IsDirty() { return m_forceDirty || (c != p); }
	void Set(T value) { p = c; c = value; }
	void Reset() { p = c; m_forceDirty = false; }
	void SetDirty() { m_forceDirty = true; }
private:
	bool m_forceDirty;
};
