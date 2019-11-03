#include "inc/gc/Field.h"
#include "inc/gc/World.h"
#include "inc/gc/World.inl"
#include "inc/gc/WorldCfg.h"
#include "inc/gc/WorldEvents.h"
#include "inc/gc/RigidBodyDynamic.h"
#include "inc/gc/Player.h"
#include "inc/gc/Macros.h"
#include "inc/gc/TypeSystem.h"

#include "inc/gc/SaveFile.h"

#include <fs/FileSystem.h>
#include <MapFile.h>
#include <cfloat>
#include <sstream>

static int DivFloor(int number, unsigned int denominator)
{
	if (number < 0)
	{
		return -((-number - 1) / (int)denominator + 1);
	}
	else
	{
		return number / (int)denominator;
	}
}

static int DivCeil(int number, unsigned int denominator)
{
	if (number > 0)
	{
		return (number - 1) / (int)denominator + 1;
	}
	else
	{
		return -((-number) / (int)denominator);
	}
}

World::World(RectRB blockBounds, bool initField)
	: _seed(1)
	, _safeMode(true)
	, _time(0)
	, _gameStarted(false)
	, _nightMode(false)
	, _bounds(RectToFRect(blockBounds * WORLD_BLOCK_SIZE))
	, _blockBounds(blockBounds)
	, _locationBounds{
		DivFloor(blockBounds.left * WORLD_BLOCK_SIZE, WORLD_LOCATION_SIZE),
		DivFloor(blockBounds.top * WORLD_BLOCK_SIZE, WORLD_LOCATION_SIZE),
		DivCeil(blockBounds.right * WORLD_BLOCK_SIZE, WORLD_LOCATION_SIZE),
		DivCeil(blockBounds.bottom * WORLD_BLOCK_SIZE, WORLD_LOCATION_SIZE) }
#ifdef NETWORK_DEBUG
	, _checksum(0)
	, _frame(0)
	, _dump(nullptr)
#endif
{
	// don't create game objects in the constructor

	grid_rigid_s.resize(_locationBounds);
	grid_walls.resize(_locationBounds);
	grid_pickup.resize(_locationBounds);
	grid_moving.resize(_locationBounds);

	if (initField)
	{
		_field = std::make_unique<Field>();
		_field->Resize(WIDTH(_blockBounds) + 1, HEIGHT(_blockBounds) + 1);
	}
	_waterTiles.resize(WIDTH(_blockBounds) * HEIGHT(_blockBounds));
	_woodTiles.resize(WIDTH(_blockBounds) * HEIGHT(_blockBounds));
}

int World::GetTileIndex(vec2d pos) const
{
	int blockX = (int)std::floor(pos.x / WORLD_BLOCK_SIZE);
	int blockY = (int)std::floor(pos.y / WORLD_BLOCK_SIZE);
	return GetTileIndex(blockX, blockY);
}

int World::GetTileIndex(int blockX, int blockY) const
{
	if (blockX >= _blockBounds.left && blockX < _blockBounds.right && blockY >= _blockBounds.top && blockY < _blockBounds.bottom)
		return blockX - _blockBounds.left + WIDTH(_blockBounds) * (blockY - _blockBounds.top);
	else
		return -1;
}

void World::OnKill(GC_Object &obj)
{
	for( auto ls: eWorld._listeners )
		ls->OnKill(obj);
}

void World::Clear()
{
	assert(IsSafeMode());

	ObjectList &ls = GetList(LIST_objects);
	while( !ls.empty() )
	{
		ls.at(ls.begin())->Kill(*this);
	}

	// reset info
	_infoAuthor.clear();
	_infoEmail.clear();
	_infoUrl.clear();
	_infoDesc.clear();
	_infoTheme.clear();
	_infoOnInit.clear();

	// reset variables
	_time = 0;
	_gameStarted = false;
#ifdef NETWORK_DEBUG
	_checksum = 0;
	_frame = 0;
	if( _dump )
	{
		fclose(_dump);
		_dump = nullptr;
	}
#endif
	assert(GetList(LIST_objects).empty());
}

World::~World()
{
	assert(IsSafeMode());
	Clear();
	assert(GetList(LIST_objects).empty());
}

void World::Serialize(SaveFile &f)
{
	assert(IsSafeMode());
	assert(GetList(LIST_objects).empty() || !f.loading());

	f.Serialize(_gameStarted);
	f.Serialize(_time);
	f.Serialize(_nightMode);

	ObjectList &objects = GetList(LIST_objects);
	if (f.loading())
	{
		// fill pointers cache
		for (;;)
		{
			ObjectType type;
			f.Serialize(type);
			if (INVALID_OBJECT_TYPE == type) // end of list signal
				break;
			if (GC_Object *obj = RTTypes::Inst().CreateFromFile(*this, type))
				f.RegPointer(obj);
			else
				throw std::runtime_error("Load error: unknown object type");
		}
	}
	else
	{
		// pointers to game objects
		for (auto it = objects.begin(); it != objects.end(); it = objects.next(it))
		{
			GC_Object *object = objects.at(it);
			ObjectType type = object->GetType();
			f.Serialize(type);
			f.RegPointer(object);
		}
		ObjectType terminator(INVALID_OBJECT_TYPE);
		f.Serialize(terminator);
	}

	// serialize objects contents in the same order as pointers
	for (ObjectList::id_type it = objects.begin(); it != objects.end(); it = objects.next(it))
	{
		objects.at(it)->Serialize(*this, f);
	}
}

void World::Import(MapFile &file)
{
	assert(GetList(LIST_objects).empty());
	assert(IsSafeMode());

	file.getMapAttribute("author",   _infoAuthor);
	file.getMapAttribute("desc",     _infoDesc);
	file.getMapAttribute("link-url", _infoUrl);
	file.getMapAttribute("e-mail",   _infoEmail);
	file.getMapAttribute("theme",    _infoTheme);
	file.getMapAttribute("on_init",  _infoOnInit);

	while( file.ReadNextObject() )
	{
		ObjectType t = RTTypes::Inst().GetTypeByName(file.GetCurrentClassName());
		if( INVALID_OBJECT_TYPE != t )
		{
			GC_Object *obj;
			const RTTypes::EdItem &ei = RTTypes::Inst().GetTypeInfo(t);
			if (ei.service)
			{
				obj = &ei.CreateDetachedService();
			}
			else
			{
				float x = 0;
				float y = 0;
				file.getObjectAttribute("x", x);
				file.getObjectAttribute("y", y);
				obj = &ei.CreateDetachedObject({ x, y });
			}
			obj->MapExchange(file);
			std::string name;
			if (file.getObjectAttribute("name", name))
				obj->SetName(*this, name.c_str());
			obj->Register(*this);
			obj->Init(*this);
		}
	}
}

FRECT World::GetOccupiedBounds() const
{
	FRECT bounds = { FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX };

	FOREACH(GetList(LIST_objects), GC_Object, object)
	{
		auto type = object->GetType();
		if (RTTypes::Inst().IsRegistered(type))
		{
			auto &typeInfo = RTTypes::Inst().GetTypeInfo(object->GetType());
			if (!typeInfo.service)
			{
				vec2d pos = static_cast<GC_MovingObject*>(object)->GetPos();
				vec2d halfSize = typeInfo.size / 2;
				bounds.left = std::min(bounds.left, pos.x - halfSize.x);
				bounds.top = std::min(bounds.top, pos.y - halfSize.y);
				bounds.right = std::max(bounds.right, pos.x + halfSize.x);
				bounds.bottom = std::max(bounds.bottom, pos.y + halfSize.y);
			}
		}
	}

	return bounds;
}

void World::Export(FS::Stream &s)
{
	assert(IsSafeMode());

	MapFile file(s, true);

	//
	// map info
	//

	file.setMapAttribute("type", "deathmatch");

	std::ostringstream str;
	str << VERSION;
	file.setMapAttribute("version", str.str());

	RectRB blockBounds;
	FRECT occupiedBounds = GetOccupiedBounds();
	blockBounds.left = (int)std::floor(occupiedBounds.left / WORLD_BLOCK_SIZE);
	blockBounds.top = (int)std::floor(occupiedBounds.top / WORLD_BLOCK_SIZE);
	blockBounds.right = (int)std::ceil(occupiedBounds.right / WORLD_BLOCK_SIZE);
	blockBounds.bottom = (int)std::ceil(occupiedBounds.bottom / WORLD_BLOCK_SIZE);

	file.setMapAttribute("north_bound", blockBounds.top);
	file.setMapAttribute("west_bound", blockBounds.left);
	file.setMapAttribute("width", WIDTH(blockBounds));
	file.setMapAttribute("height", HEIGHT(blockBounds));

	file.setMapAttribute("author",   _infoAuthor);
	file.setMapAttribute("desc",     _infoDesc);
	file.setMapAttribute("link-url", _infoUrl);
	file.setMapAttribute("e-mail",   _infoEmail);

	file.setMapAttribute("theme",    _infoTheme);

	file.setMapAttribute("on_init",  _infoOnInit);

	// objects
	FOREACH( GetList(LIST_objects), GC_Object, object )
	{
		if( RTTypes::Inst().IsRegistered(object->GetType()) )
		{
			file.BeginObject(RTTypes::Inst().GetTypeInfo(object->GetType()).name);
			if (auto optName = object->GetName(*this); !optName.empty())
				file.setObjectAttribute("name", optName);
			object->MapExchange(file);
			file.WriteCurrentObject();
		}
	}

	file.WriteEndOfFile();
}

int World::net_rand()
{
	return ((_seed = _seed * 214013L + 2531011L) >> 16) & NET_RAND_MAX;
}

float World::net_frand(float max)
{
	return (float) net_rand() / (float) NET_RAND_MAX * max;
}

vec2d World::net_vrand(float len)
{
	return Vec2dDirection(net_frand(PI2)) * len;
}

bool World::CalcOutstrip( vec2d origin,
                          float projectileSpeed,
                          vec2d targetPos,
                          vec2d targetVelocity,
                          vec2d &out_fake) // out: fake target position
{
	float vt = targetVelocity.len();

	if( vt >= projectileSpeed || vt < 1e-7 )
	{
		out_fake = targetPos;
		return false;
	}

	float cg = targetVelocity.x / vt;
	float sg = targetVelocity.y / vt;

	float x   = (targetPos.x - origin.x) * cg + (targetPos.y - origin.y) * sg;
	float y   = (targetPos.y - origin.y) * cg - (targetPos.x - origin.x) * sg;
	float tmp = projectileSpeed*projectileSpeed - vt*vt;

	float fx = x + vt * (x*vt + sqrt(x*x * projectileSpeed*projectileSpeed + y*y * tmp)) / tmp;

	out_fake = Vec2dClamp(origin + vec2d{ fx*cg - y*sg, fx*sg + y*cg }, _bounds);
	return true;
}

GC_RigidBodyStatic* World::TraceNearest( const Grid<ObjectList> &list,
                                         const GC_RigidBodyStatic* ignore,
                                         const vec2d &x0,      // origin
                                         const vec2d &a,       // direction with length
                                         vec2d *ht,
                                         vec2d *norm) const
{
//	DbgLine(x0, x0 + a);

	struct SelectNearest
	{
		const GC_RigidBodyStatic *ignore;
		vec2d x0;
		vec2d lineCenter;
		vec2d lineDirection;

		GC_RigidBodyStatic *result;
		vec2d resultPos;
		vec2d resultNorm;

		SelectNearest()
			: result(nullptr)
		{
		}

		bool Select(GC_RigidBodyStatic *obj, vec2d norm, float enter, float exit)
		{
			if( ignore != obj )
			{
				result = obj;
				resultPos = lineCenter + lineDirection * enter;
				resultNorm = norm;

				lineDirection *= enter + 0.5f;
				lineCenter = x0 + lineDirection / 2;
			}
			return false;
		}
		inline const vec2d& GetCenter() const { return lineCenter; }
		inline const vec2d& GetDirection() const { return lineDirection; }
	};
	SelectNearest selector;
	selector.ignore = ignore;
	selector.x0 = x0;
	selector.lineCenter = x0 + a/2;
	selector.lineDirection = a;
	RayTrace(list, selector);
	if( selector.result )
	{
		if( ht ) *ht = selector.resultPos;
		if( norm ) *norm = selector.resultNorm;
	}
	return selector.result;
}

void World::TraceAll( const Grid<ObjectList> &list,
                      const vec2d &x0,      // origin
                      const vec2d &a,       // direction with length
                      std::vector<CollisionPoint> &result) const
{
	struct SelectAll
	{
		vec2d lineCenter;
		vec2d lineDirection;

		std::vector<CollisionPoint> &result;

		explicit SelectAll(std::vector<CollisionPoint> &r)
			: result(r)
		{
		}

		bool Select(GC_RigidBodyStatic *obj, vec2d norm, float enter, float exit)
		{
			CollisionPoint cp;
			cp.obj = obj;
			cp.normal = norm;
			cp.enter = enter;
			cp.exit = exit;
			result.push_back(cp);
			return false;
		}
		inline const vec2d& GetCenter() const { return lineCenter; }
		inline const vec2d& GetDirection() const { return lineDirection; }
	};
	SelectAll selector(result);
	selector.lineCenter = x0 + a/2;
	selector.lineDirection = a;
	RayTrace(list, selector);
}

IMPLEMENT_POOLED_ALLOCATION(ResumableObject);

ResumableObject* World::Timeout(GC_Object &obj, float timeout)
{
	assert(GetTime() + timeout >= GetTime());
	auto id = new ResumableObject(obj);
	_resumables.emplace(std::unique_ptr<ResumableObject>(id), GetTime() + timeout);
	return id;
}

void World::Step(float dt)
{
	if( !_gameStarted )
	{
		_gameStarted = true;
		for( auto ls: eWorld._listeners )
			ls->OnGameStarted();
	}

	float nextTime = _time + dt;
	while (!_resumables.empty() && _resumables.top().time < nextTime)
	{
		_time = _resumables.top().time;
		GC_Object *obj = _resumables.top().obj->ptr;
		_resumables.pop();
		if (obj)
			obj->Resume(*this);
	}

	_time = nextTime;

	_safeMode = false;
	ObjectList &ls = GetList(LIST_timestep);
	ls.for_each([=](ObjectList::id_type id, GC_Object *o){
		o->TimeStep(*this, dt);
	});
	GC_RigidBodyDynamic::ProcessResponse(*this);
	_safeMode = true;


	//
	// sync lost error detection
	//

#ifdef NETWORK_DEBUG
	if( !_dump )
	{
		char fn[MAX_PATH];
		sprintf_s(fn, "network_dump_%u_%u.txt", GetTickCount(), GetCurrentProcessId());
		_dump = fopen(fn, "w");
		assert(_dump);
	}
	++_frame;
	fprintf(_dump, "\n### frame %04d ###\n", _frame);

	DWORD dwCheckSum = 0;
	for( ObjectList::safe_iterator it = ts_fixed.safe_begin(); it != ts_fixed.end(); ++it )
	{
		if( DWORD cs = (*it)->checksum() )
		{
			dwCheckSum = dwCheckSum ^ cs ^ 0xD202EF8D;
			dwCheckSum = (dwCheckSum >> 1) | ((dwCheckSum & 0x00000001) << 31);
			fprintf(_dump, "0x%08x -> local 0x%08x, global 0x%08x  (%s)\n", (*it), cs, dwCheckSum, typeid(**it).name());
		}
	}
	_checksum = dwCheckSum;
	fflush(_dump);
#endif
}

GC_Object* World::FindObject(std::string_view name) const
{
	std::map<std::string, const GC_Object*>::const_iterator it = _nameToObjectMap.find(name);
	return _nameToObjectMap.end() != it ? const_cast<GC_Object*>(it->second) : nullptr;
}

///////////////////////////////////////////////////////////////////////////////

GC_Player* World::GetPlayerByIndex(size_t playerIndex)
{
	GC_Player *player = nullptr;
	FOREACH(GetList(LIST_players), GC_Player, p)
	{
		if( 0 == playerIndex-- )
		{
			player = p;
			break;
		}
	}
	return player;
}

void World::Seed(unsigned long seed)
{
    _seed = seed;
}


///////////////////////////////////////////////////////////////////////////////
// end of file
