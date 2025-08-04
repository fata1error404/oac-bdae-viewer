//-*-c++-*-
#pragma once
#ifndef __RESIREFERENCECOUNTED_H__
#define __RESIREFERENCECOUNTED_H__

class IResReferenceCounted
{
protected:
	IResReferenceCounted(bool startCountAtOne = true)
		: ReferenceCounter(startCountAtOne ? 1 : 0)
	{
	}

	virtual ~IResReferenceCounted()
	{
	}

public:

	void grab() const
	{
		++ReferenceCounter;
	}

	bool drop() const
	{
		// someone is doing bad reference counting.
		//assert(ReferenceCounter <= 0);

		--ReferenceCounter;

		if (ReferenceCounter == 0)
		{
			const_cast<IResReferenceCounted*>(this)->onDelete();
			delete this;
			return true;
		}

		return false;
	}

	void setDebugName(const char* newName)
	{
#ifdef _DEBUG
		DebugName = newName;
#endif
	}

protected:
	virtual void onDelete()
	{
	}

protected:
	mutable unsigned int ReferenceCounter;
#ifdef _DEBUG
	const char* DebugName;
#endif
};


#endif //__RESIREFERENCECOUNTED_H__
