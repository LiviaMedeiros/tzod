// Object.cpp
///////////////////////////////////////////////////////

#include "stdafx.h"
#include "Object.h"
#include "level.h"

#include "core/debug.h"

#include "fs/SaveFile.h"
#include "fs/MapFile.h"


///////////////////////////////////////////////////////////////////////////////
// ObjectProperty class implementation

ObjectProperty::ObjectProperty(PropertyType type, const string_t &name)
: _type(type), _name(name), _value_index(0), _int_value(0), _int_min(0), _int_max(0)
{
}

ObjectProperty::PropertyType ObjectProperty::GetType(void) const
{
	return _type;
}

string_t ObjectProperty::GetName(void) const
{
	return _name;
}

int ObjectProperty::GetValueInt(void) const
{
	_ASSERT(TYPE_INTEGER == _type);
	return _int_value;
}

int ObjectProperty::GetMin(void) const
{
	_ASSERT(TYPE_INTEGER == _type);
	return _int_min;
}

int ObjectProperty::GetMax(void) const
{
	_ASSERT(TYPE_INTEGER == _type);
	return _int_max;
}

void ObjectProperty::SetValueInt(int value)
{
	_ASSERT(TYPE_INTEGER == _type);
	_int_value = value;
}

void ObjectProperty::SetRange(int min, int max)
{
	_ASSERT(TYPE_INTEGER == _type);
	_int_min = min;
	_int_max = max;
}

void ObjectProperty::SetValue(const string_t &str)
{
	_ASSERT(TYPE_STRING == _type);
	_str_value = str;
}

string_t ObjectProperty::GetValue(void) const
{
	_ASSERT( TYPE_STRING == _type );
	return _str_value;
}

void ObjectProperty::AddItem(const string_t &str)
{
	_ASSERT(TYPE_MULTISTRING == _type);
	_value_set.push_back(str);
}

string_t ObjectProperty::GetSetValue(size_t index) const
{
	_ASSERT(TYPE_MULTISTRING == _type);
	_ASSERT(index < _value_set.size());
	return _value_set[index];
}

size_t ObjectProperty::GetCurrentIndex(void) const
{
	_ASSERT(TYPE_MULTISTRING == _type);
	return _value_index;
}

void ObjectProperty::SetCurrentIndex(size_t index)
{
	_ASSERT(TYPE_MULTISTRING == _type);
	_ASSERT(index < _value_set.size());
	_value_index = index;
}

size_t ObjectProperty::GetSetSize(void) const
{
	return _value_set.size();
}

///////////////////////////////////////////////////////////////////////////////
// IPropertySet class implementation

IPropertySet::IPropertySet(GC_Object *object)
{
	_object   = object;
	_refcount = 1;
}

IPropertySet::~IPropertySet()
{
	_object = NULL;
}

int IPropertySet::AddRef()
{
	return ++_refcount;
}

int IPropertySet::Release()
{
	int result = --_refcount;
	if( 0 == result )
		delete this;
	return result;
}

int IPropertySet::GetCount() const
{
	return 0;
}

ObjectProperty* IPropertySet::GetProperty(int index)
{
	_ASSERT(FALSE);
	return NULL;
}

void IPropertySet::Exchange(bool bApply)
{
}

///////////////////////////////////////////////////////////////////////////////
// GC_Object class implementation

GC_Object::GC_Object() : _memberOf(g_level->objects, this)
{
	_refCount             = 1;
	_notifyProtectCount   = 0;
	_flags                = 0;

	ZeroMemory(&_location, sizeof(Location));
	MoveTo(vec2d(0, 0));
}

GC_Object::GC_Object(FromFile) : _memberOf(g_level->objects, this)
{
	_notifyProtectCount = 0;
}

GC_Object::~GC_Object()
{
	_ASSERT(0 == _refCount);
	_ASSERT(0 == _notifyProtectCount);
	_ASSERT(IsKilled());
}

void GC_Object::Kill()
{
	if( IsKilled() ) return;
	SetFlags(GC_FLAG_OBJECT_KILLED);
	LeaveAllContexts();

	// ������� �� ������� ������
	SetEvents(0);

	PulseNotify(NOTIFY_OBJECT_KILL);
	Release();
}

void GC_Object::Notify::Serialize(SaveFile &f)
{
	f.Serialize(type);
	f.Serialize(once);
	f.Serialize(hasGuard);
	f.Serialize(subscriber);

	// we are not allowed to serialize pointers so we use small hack :)
	f.Serialize(reinterpret_cast<size_t&>(handler));
}

void GC_Object::Serialize(SaveFile &f)
{	/////////////////////////////////////
	f.Serialize(_flags);
	f.Serialize(_location);
	f.Serialize(_pos);
	f.Serialize(_refCount);
	/////////////////////////////////////
	// events
	if( f.loading() )
	{
		DWORD tmp = _flags & GC_FLAG_OBJECT_EVENTS_ALL;
		ClearFlags(GC_FLAG_OBJECT_EVENTS_ALL);
		SetEvents(tmp);
	}
	/////////////////////////////////////
	// notify list
	unsigned short count = _notifyList.size();
	f.Serialize(count);
	if( f.loading() )
	{
		_ASSERT(_notifyList.empty());
		for( int i = 0; i < count; i++ )
		{
			_notifyList.push_back(Notify());
			_notifyList.back().Serialize(f);
		}
	}
	else
	{
		std::list<Notify>::iterator it = _notifyList.begin();
		for( ; it != _notifyList.end(); ++it )
			it->Serialize(f);
	}
}

GC_Object* GC_Object::CreateFromFile(SaveFile &file)
{
	DWORD bytesRead;
	ObjectType type;

	ReadFile(file._file, &type, sizeof(type), &bytesRead, NULL);
	if( bytesRead != sizeof(type) )
		throw "Load error: unexpected end of file\n";

	LOGOUT_3("restoring object %d at 0x%X\n", type,
		SetFilePointer(file._file, 0, 0, FILE_CURRENT));

	_from_file_map::const_iterator it = _get_from_file_map().find(type);
	if( _get_from_file_map().end() == it )
	{
		LOGOUT_2("Load error: unknown object type %u\n", type);
		throw "Load error: unknown object type\n";
	}

	GC_Object *object = it->second();
	
	size_t id;
	file.Serialize(id);
	file.RegPointer(object, id);

	object->Serialize(file);
	return object;
}


int GC_Object::AddRef()
{
	return ++_refCount;
}

int GC_Object::Release()
{
	_ASSERT(_refCount > 0);
	if( 0 == (--_refCount) )
	{
		_ASSERT(IsKilled());
		delete this;
		return 0;
	}
	return _refCount;
}

void GC_Object::LocationFromPoint(const vec2d &pt, Location &l)
{
	int x = __max(0, int((pt.x + LOCATION_SIZE / 4) / (LOCATION_SIZE / 2)) - 1);
	int y = __max(0, int((pt.y + LOCATION_SIZE / 4) / (LOCATION_SIZE / 2)) - 1);

	l.level = (x % 2) + (y % 2) * 2;
	l.x = __min(g_level->_locations_x-1, x / 2);
	l.y = __min(g_level->_locations_y-1, y / 2);
}

//����������� ������
void GC_Object::MoveTo(const vec2d &pos)
{
	Location l;
	LocationFromPoint(pos, l);

	_pos = pos;

	if( 0 != memcmp(&l, &_location, sizeof(Location)) )
	{
		LeaveAllContexts();
		EnterAllContexts(l);
	}

	PulseNotify(NOTIFY_OBJECT_MOVE);
}

void GC_Object::LeaveAllContexts()
{
	for( CONTEXTS_ITERATOR it = _contexts.begin(); it != _contexts.end(); ++it )
		LeaveContext(*it);
}

void GC_Object::LeaveContext(ObjectContext &context)
{
	_ASSERT(context.inContext);
	(*context.pGridSet)(_location.level).
		element(_location.x, _location.y).safe_erase(context.iterator);
	context.inContext = FALSE;
}

void GC_Object::EnterAllContexts(Location const &l)
{
	_ASSERT(!IsKilled());
	_location = l;
	for( CONTEXTS_ITERATOR it = _contexts.begin(); it != _contexts.end(); ++it )
		EnterContext(*it, _location);
}

void GC_Object::EnterContext(ObjectContext &context, Location const &l)
{
	_ASSERT(!IsKilled());
	_ASSERT(!context.inContext);

	(*context.pGridSet)(l.level).element(l.x, l.y).push_front(this);
	context.iterator  = (*context.pGridSet)(l.level).element(l.x, l.y).begin();
	context.inContext = TRUE;
}

void GC_Object::AddContext(OBJECT_GRIDSET *pGridSet)
{
	_ASSERT(!IsKilled());

	ObjectContext context;
	context.inContext  = FALSE;
	context.pGridSet   = pGridSet;

	_contexts.push_front(context);
	EnterContext(_contexts.front(), _location);
}

void GC_Object::RemoveContext(OBJECT_GRIDSET *pGridSet)
{
	for( CONTEXTS_ITERATOR it = _contexts.begin(); it != _contexts.end(); ++it )
	{
		if( (*it).pGridSet == pGridSet )
		{
			if( it->inContext)
				LeaveContext(*it);
			_contexts.erase(it);
			return;
		}
	}

	// �� ������ ��������� ��������
	_ASSERT(FALSE);
}

void GC_Object::SetEvents(DWORD dwEvents)
{
	// �������� �� TIMESTEP_FIXED
	if( 0 == (GC_FLAG_OBJECT_EVENTS_TS_FIXED & dwEvents) &&
		0 != (GC_FLAG_OBJECT_EVENTS_TS_FIXED & _flags) )
	{
        g_level->ts_fixed.safe_erase(_itPosFixed);
	}
	// ���������� � TIMESTEP_FIXED
	else if( 0 != (GC_FLAG_OBJECT_EVENTS_TS_FIXED & dwEvents) &&
			 0 == (GC_FLAG_OBJECT_EVENTS_TS_FIXED & _flags) )
	{
		_ASSERT(!IsKilled());
		g_level->ts_fixed.push_front(this);
		_itPosFixed = g_level->ts_fixed.begin();
	}

	// �������� �� TIMESTEP_FLOATING
	if( 0 != (GC_FLAG_OBJECT_EVENTS_TS_FLOATING & _flags) &&
		0 == (GC_FLAG_OBJECT_EVENTS_TS_FLOATING & dwEvents) )
	{
		g_level->ts_floating.safe_erase(_itPosFloating);
	}
	// ���������� � TIMESTEP_FLOATING
	else if( 0 == (GC_FLAG_OBJECT_EVENTS_TS_FLOATING & _flags) &&
			 0 != (GC_FLAG_OBJECT_EVENTS_TS_FLOATING & dwEvents) )
	{
		_ASSERT(!IsKilled());
		g_level->ts_floating.push_front(this);
		_itPosFloating = g_level->ts_floating.begin();
	}

	// �������� �� ENDFRAME
	if( 0 == (GC_FLAG_OBJECT_EVENTS_ENDFRAME & dwEvents) &&
		0 != (GC_FLAG_OBJECT_EVENTS_ENDFRAME & _flags) )
	{
		g_level->endframe.safe_erase(_itPosEndFrame);
	}
	// ���������� � ENDFRAME
	else if( 0 != (GC_FLAG_OBJECT_EVENTS_ENDFRAME & dwEvents) &&
			 0 == (GC_FLAG_OBJECT_EVENTS_ENDFRAME & _flags) )
	{
		_ASSERT(!IsKilled());
		g_level->endframe.push_front(this);
		_itPosEndFrame = g_level->endframe.begin();
	}

	//-------------------------
	ClearFlags(GC_FLAG_OBJECT_EVENTS_ALL);
	SetFlags(dwEvents);
}

void GC_Object::Subscribe(NotyfyType type, GC_Object *subscriber,
						  NOTIFYPROC handler, bool once, bool guard)
{
	_ASSERT(subscriber);
	_ASSERT(handler);
	//--------------------------------------------------
	Notify notify;
	notify.type         = type;
	notify.subscriber   = subscriber;
	notify.handler      = handler;
	notify.once         = once;
	notify.hasGuard     = guard;
	_notifyList.push_back(notify);
	//--------------------------------------------------
	if( guard )	// ������ �� ������ ���� pSubscriber
	{				// ����� ������, ��� this
		notify.type        = NOTIFY_OBJECT_KILL;
		notify.subscriber  = this;
		notify.handler     = &GC_Object::OnKillSubscriber;
		notify.once        = true;
		notify.hasGuard    = false;
		subscriber->_notifyList.push_back(notify);
	}
}

void GC_Object::Unsubscribe(GC_Object *subscriber)
{
	std::list<Notify>::iterator it = _notifyList.begin();
	if( _notifyProtectCount )
	{
		while( it != _notifyList.end() )
		{
			if( subscriber != it->subscriber )
			{
				++it;
				continue;
			}
			std::list<Notify>::iterator tmp = it++;
			tmp->removed = true;
		}
	}
	else
	{
		while( it != _notifyList.end() )
		{
			if( subscriber != it->subscriber )
			{
				++it;
				continue;
			}
			std::list<Notify>::iterator tmp = it++;
			_notifyList.erase(tmp);
		}
	}
}

bool GC_Object::IsSubscriber(const GC_Object *object) const
{
	std::list<Notify>::const_iterator it = _notifyList.begin();
	for( ; it != _notifyList.end(); ++it )
	{
		if( it->removed ) continue;
		if( object == it->subscriber ) return true;
	}
	return false;
}

GC_Object* GC_Object::GetSubscriber(ObjectType type) const
{
	std::list<Notify>::const_iterator it = _notifyList.begin();
	for( ;it != _notifyList.end(); ++it )
	{
		if( it->removed ) continue;
		if( it->subscriber->GetType() == type ) return GetRawPtr(it->subscriber);
	}
	return NULL;
}

void GC_Object::OnKillSubscriber(GC_Object *sender, void *param)
{
	Unsubscribe(sender);
}

void GC_Object::PulseNotify(NotyfyType type, void *param)
{
	if( _notifyList.empty() ) return;

	_notifyProtectCount++;

	std::list<Notify>::iterator tmp, it = _notifyList.begin();
	while( it != _notifyList.end() )
	{
		if( type != it->type ) { ++it; continue; }
		_ASSERT(it->subscriber);
		(GetRawPtr(it->subscriber)->*it->handler)(this, param);
		tmp = it++;
        if( tmp->once ) _notifyList.erase(tmp);
	}

	if( 0 == --_notifyProtectCount )
		_notifyList.remove_if(Notify::CleanUp());
}

void GC_Object::TimeStepFixed(float dt)
{
}

void GC_Object::TimeStepFloat(float dt)
{
}

void GC_Object::EndFrame()
{
}

void GC_Object::EditorAction()
{
}

IPropertySet* GC_Object::GetProperties()
{
	return NULL;
}

void GC_Object::mapExchange(MapFile &f)
{
	// ���������� ������ �����������.
	// ����������� �������� ���������� ����� �����������.
	if( !f.loading() )
	{
		MAP_EXCHANGE_FLOAT(x, _pos.x, 0);
		MAP_EXCHANGE_FLOAT(y, _pos.y, 0);
	}
}

///////////////////////////////////////////////////////////////////////////////
// end of file
