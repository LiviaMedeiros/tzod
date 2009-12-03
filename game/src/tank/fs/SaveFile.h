// SaveFile.h

#pragma once

#include "FileSystem.h"

class GC_Object;

class SaveFile
{
	typedef std::map<GC_Object*, size_t> PtrToIndex;
	typedef std::vector<GC_Object*> IndexToPtr;

	PtrToIndex _ptrToIndex;
	IndexToPtr _indexToPtr;

	SafePtr<FS::Stream> _stream;
	bool _load;

public:
	SaveFile(SafePtr<FS::Stream> &s, bool loading);

	bool loading() const
	{
		return _load;
	}


	void Serialize(string_t &str);

	template<class T>
	void Serialize(T &obj);

	template<class T>
	void Serialize(SafePtr<T> &ptr);

	template<class T>
	void SerializeArray(T *p, size_t count);

	void RegPointer(GC_Object *ptr);

private:
	template<class T>
	void Serialize(const T &obj);
	template<class T>
	void Serialize(const SafePtr<T> &ptr);
	template<class T>
	void Serialize(T *) {assert(!"you are not allowed to serialize raw pointers");}
};

template<class T>
void SaveFile::Serialize(T &obj)
{
	assert(0 != strcmp(typeid(obj).raw_name(), typeid(string_t).raw_name()));
	assert(NULL == strstr(typeid(obj).raw_name(), "SafePtr"));
	if( loading() )
		_stream->Read(&obj, sizeof(T));
	else
		_stream->Write(&obj, sizeof(T));
}

template<class T>
void SaveFile::Serialize(SafePtr<T> &ptr)
{
	DWORD_PTR id;
	if( loading() )
	{
		Serialize(id);
		if( id )
		{
			if( _indexToPtr.size() <= id )
				throw std::runtime_error("ERROR: invalid links");
			if( !dynamic_cast<T*>(_indexToPtr[id]) )
				throw std::runtime_error("ERROR: invalid object pointer");
			ptr = WrapRawPtr(static_cast<T*>(_indexToPtr[id]));
		}
		else
		{
			ptr = NULL;
		}
	}
	else
	{
		if( !ptr || ptr->IsKilled() )
		{
			id = 0;
		}
		else
		{
			assert(_ptrToIndex.count(GetRawPtr(ptr)));
			id = _ptrToIndex[GetRawPtr(ptr)];
		}
		Serialize(id);
	}
}

template<class T>
void SaveFile::SerializeArray(T *p, size_t count)
{
	assert(0 != strcmp(typeid(T).raw_name(), typeid(string_t).raw_name()));
	assert(NULL == strstr(typeid(T).raw_name(), "SafePtr"));
	if( loading() )
		_stream->Read(p, sizeof(T) * count);
	else
		_stream->Write(p, sizeof(T) * count);
}

// end of file
