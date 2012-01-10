#ifndef _LOCKLESS_QUEUE_
#define _LOCKLESS_QUEUE_

//*******************************************************************************
//  Copyright (C) 2006 Casey Borders, Moo Productions
//  Contact: Casey.Borders@MooProductions.org
//
//  This software is provided 'as-is', without any express or implied warranty.
//  In no event will the authors be held liable for any damages arising from
//  the use of this software.
//
//  Permission is granted to anyone to use this software for any purpose,
//  including commercial applications, and to alter it and redistribute it
//  freely, subject to the following restrictions:
//
//  *  The origin of this software must not be misrepresented; you must not
//     claim that you wrote the original software. If you use this software in a
//     product, an acknowledgment in the product documentation would be
//     appreciated but is not required.
//
//  *  Altered source versions must be plainly marked as such, and must not be
//     misrepresented as being the original software.
//
//  *  This notice may not be removed or altered from any source distribution.
//********************************************************************************

template <class T> class LocklessQueue
	{
	private:
		T* mBuffer;
		size_t mHead;
		size_t mTail;
		int mSize;
		
	public:
		LocklessQueue(int size) : mSize(size+1),
		mBuffer(0), mHead(0), mTail(0)
		{
			mBuffer = new T[mSize];
		}
		
		~LocklessQueue()
		{
			delete[] mBuffer;
		}
		
		bool push(T msg)
		{
			size_t head = mHead;
			
			if((head+1)%mSize == mTail)
				return false;
			
			mBuffer[head++] = msg;
			
			if(head == mSize)
				head = 0;
			
			mHead = head;
			return true;
		}
		
		bool pop(T &msg)
		{
			size_t tail = mTail;
			
			if(tail == mHead)
				return false;
			
			msg = mBuffer[tail++];
			
			if(tail == mSize)
				tail = 0;
			
			mTail = tail;
			return true;
		}
	};
#endif