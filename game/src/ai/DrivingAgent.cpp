#include "DrivingAgent.h"
#include <gc/Field.h>
#include <gc/SaveFile.h>
#include <gc/Turrets.h>
#include <gc/Vehicle.h>
#include <gc/VehicleState.h>
#include <gc/WeaponBase.h>
#include <gc/World.h>
#include <gc/WorldCfg.h>

#include <functional>

static void CatmullRom(const vec2d &p1, const vec2d &p2, const vec2d &p3, const vec2d &p4, vec2d &out, float s)
{
	float s2 = s * s;
	float s3 = s2 * s;
	out = (p1*(2 * s2 - s3 - s) + p2*(3 * s3 - 5 * s2 + 2) + p3*(4 * s2 - 3 * s3 + s) + p4*(s3 - s2)) * 0.5f;
}

#define NODE_RADIUS         32.0f
#define MIN_PATH_ANGLE       0.8f

#define GRID_ALIGN(x, sz)    ((x)-(x)/(sz)*(sz)<(sz)/2)?((x)/(sz)):((x)/(sz)+1)


void DrivingAgent::Serialize(SaveFile &f)
{
	f.Serialize(_arrivalPoint);
	f.Serialize(_backTime);
	f.Serialize(_stickTime);

	if (f.loading())
	{
		size_t size;
		f.Serialize(size);
		while (size--)
		{
			_attackList.push_back(AttackListType::value_type());
			f.Serialize(_attackList.back());
		}
	}
	else
	{
		size_t size = _attackList.size();
		f.Serialize(size);
		for (AttackListType::iterator it = _attackList.begin(); _attackList.end() != it; ++it)
		{
			f.Serialize(*it);
		}
	}
}


// check the cell's passability taking into account current weapon settings
static bool CheckCell(const FieldCell &cell, bool hasWeapon)
{
	return (0xFF != cell.Properties() && hasWeapon) || (0 == cell.Properties() && !hasWeapon);
}

static float EstimatePathLength(RefFieldCell begin, RefFieldCell end)
{
	int dx = abs(end.x - begin.x);
	int dy = abs(end.y - begin.y);
	return (float)std::max(dx, dy) + (float)std::min(dx, dy) * 0.4142f;
}

float DrivingAgent::CreatePath(World &world, vec2d from, vec2d to, int team, float max_depth, bool bTest, const AIWEAPSETTINGS *ws)
{
	if (!PtInFRect(world.GetBounds(), to))
	{
		return -1;
	}

	Field::NewSession();
	Field &field = world._field;

	std::priority_queue<RefFieldCell, std::vector<RefFieldCell>, FieldCellCompare> open(field);

	RefFieldCell startRef = { GRID_ALIGN(int(from.x), WORLD_BLOCK_SIZE), GRID_ALIGN(int(from.y), WORLD_BLOCK_SIZE) };
	RefFieldCell endRef = { GRID_ALIGN(int(to.x), WORLD_BLOCK_SIZE) , GRID_ALIGN(int(to.y), WORLD_BLOCK_SIZE) };

	FieldCell &start = field(startRef.x, startRef.y);

	if( !CheckCell(start, !!ws) )
		return -1;

	start.Check();
	start._before = 0;
	start._total = EstimatePathLength(startRef, endRef);
	start._stepX = 0;
	start._stepY = 0;

	open.push(startRef);


	while( !open.empty() )
	{
		RefFieldCell currentRef = open.top();
		open.pop();

		FieldCell &cn = field(currentRef.x, currentRef.y);

		if(currentRef == endRef)
			break; // a path was found

		// neighbor nodes check order
		//    4 | 0 | 6
		//   ---+---+---
		//    2 | n | 3
		//   ---+---+---
		//    7 | 1 | 5
		//                               0  1  2  3  4  5  6  7
		static const int   per_x[8] = {  0, 0,-1, 1,-1, 1, 1,-1 };  // node x offset
		static const int   per_y[8] = { -1, 1, 0, 0,-1, 1,-1, 1 };  // node y offset
		static const float dist [8] = {
			1.0f, 1.0f, 1.0f, 1.0f,
			1.4142f, 1.4142f, 1.4142f, 1.4142f };             // path cost

		// for diagonal checks
		//                           4     5     6     7
		static int check_diag[] = { 0,2,  1,3,  3,0,  2,1 };

		for( int i = 0; i < 8; ++i )
		{
			if( i > 3 ) // check diagonal passability
			if( !CheckCell(field(currentRef.x + per_x[check_diag[(i-4)*2  ]],
			                     currentRef.y + per_y[check_diag[(i-4)*2  ]]), !!ws) ||
			    !CheckCell(field(currentRef.x + per_x[check_diag[(i-4)*2+1]],
			                     currentRef.y + per_y[check_diag[(i-4)*2+1]]), !!ws) )
			{
				continue;
			}


			RefFieldCell nextRef = { currentRef.x + per_x[i], currentRef.y + per_y[i] };
			FieldCell &next = field(nextRef.x, nextRef.y);
			if( CheckCell(next, !!ws) )
			{
				// increase path cost when travel through the walls
				float dist_mult = 1;
				if( 1 == next.Properties() )
					dist_mult = ws->fDistanceMultipler;

				float before = cn.Before() + dist[i] * dist_mult;

				// penalty for turns
				if (cn._stepX || cn._stepY) // TODO: use initial vehicle direction
				{
					float stepSqr = float(cn._stepX*cn._stepX + cn._stepY*cn._stepY);
					float c = float(cn._stepX * per_x[i] + cn._stepY * per_y[i]) / (dist[i] * std::sqrt(stepSqr));
					if (c < 0.5f) // 0 is 90 deg. turn
						before += 1;
					else if (c < 0.9f) // 0.707 is 45 deg. turn
						before += 0.2f;
				}

				if( !next.IsChecked() )
				{
					next._stepX = per_x[i];
					next._stepY = per_y[i];
					next._before = before;
					next._total = before + EstimatePathLength(nextRef, endRef);
					next.Check();
					if( next.Total() < max_depth )
						open.push(nextRef);
				}
				else if( next._before > before )
				{
					// Do not update _total as it would brake 'open' ordering. It only affects
					// the node selection priority but eventually it will be handled anyway.
					next._before = before;
					next._stepX = per_x[i];
					next._stepY = per_y[i];
					if( next.Total() < max_depth )
						open.push(nextRef);
				}
			}
		}
	}

	if( field(endRef.x, endRef.y).IsChecked() )
	{
		float distance = field(endRef.x, endRef.y).Before();

		if( !bTest )
		{
			ClearPath();

			RefFieldCell currentRef = endRef;
			FieldCell *current = &field(currentRef.x, currentRef.y);

			PathNode node;

			node.coord = to;
			_path.push_front(node);

			while( current->_stepX || current->_stepY )
			{
				// trace back
				currentRef.x -= current->_stepX;
				currentRef.y -= current->_stepY;
				current = &field(currentRef.x, currentRef.y);

				node.coord.x = (float) (currentRef.x * WORLD_BLOCK_SIZE);
				node.coord.y = (float) (currentRef.y * WORLD_BLOCK_SIZE);
				_path.push_front(node);

				for( int i = 0; i < current->GetObjectsCount(); ++i )
				{
					assert(ws);
					assert(current->Properties() > 0);

					GC_RigidBodyStatic *object = current->GetObject(i);

					if( team && !_attackFriendlyTurrets)
					{
						auto turret = dynamic_cast<GC_Turret*>(object);
						if( turret && (turret->GetTeam() == team) )
						{
							continue;
						}
					}
					_attackList.push_front(object);
				}
			}

			assert(startRef == currentRef);
		}

		return distance;
	}

	// path not found
	return -1;
}

void DrivingAgent::SmoothPath()
{
	if( _path.size() < 4 )
		return;

	vec2d vn[4];
	std::list<PathNode>::iterator it[4], tmp;

	//
	// smooth angles
	//
	if( _path.size() > 4 )
	{
		it[1] = _path.begin();
		it[0] = it[1]++;
		while( it[1] != _path.end() )
		{
			PathNode new_node;
			new_node.coord = (it[0]->coord + it[1]->coord) * 0.5f;
			_path.insert(it[1], new_node);
			if( it[0] != _path.begin() )
				_path.erase(it[0]);
			it[0] = it[1]++;
		}
	}


	//
	// spline interpolation
	//

	tmp = _path.begin();
	for( int i = 0; i < 4; ++i )
	{
		it[i] = tmp++;
		vn[i] = it[i]->coord;
	}

	for(;;)
	{
		PathNode new_node;

		for( int i = 1; i < 4; ++i )
		{
			CatmullRom(vn[0], vn[1], vn[2], vn[3], new_node.coord, (float) i / 4.0f);
			_path.insert(it[2], new_node);
		}

		for( int i = 0; i < 3; ++i )
		{
			it[i] = it[i+1];
			vn[i] = vn[i+1];
		}

		if( ++it[3] == _path.end() )
			break;

		vn[3] = it[3]->coord;
	}
}

std::list<DrivingAgent::PathNode>::const_iterator DrivingAgent::FindNearPathNode(const vec2d &pos, vec2d *projection, float *offset) const
{
	assert(!_path.empty());
	std::list<PathNode>::const_iterator it = _path.begin(), result = it;
	float rr_min = (it->coord - pos).sqr();
	while( ++it != _path.end() )
	{
		float rr = (it->coord - pos).sqr();
		if( rr <= rr_min )
		{
			result = it;
			rr_min = rr;
		}
	}

	assert(_path.end() != result);

	if( projection )
	{
		vec2d dev = pos - result->coord;

		float prevL = -1;
		float nextL = -1;

		vec2d prevPos;
		vec2d nextPos;

		vec2d nextDir;
		vec2d prevDir;

		float nextDir2;
		float nextDot;

		float prevDir2;
		float prevDot;

		std::list<PathNode>::const_iterator prevIt = result;
		if( _path.begin() != result )
		{
			--prevIt;
			prevDir  = prevIt->coord - result->coord;
			prevDir2 = prevDir.sqr();
			prevDot  = Vec2dDot(prevDir, dev);
			prevL    = prevDot / sqrtf(prevDir2);
		}

		std::list<PathNode>::const_iterator nextIt = result; ++nextIt;
		if( _path.end() != nextIt )
		{
			nextDir  = nextIt->coord - result->coord;
			nextDir2 = nextDir.sqr();
			nextDot  = Vec2dDot(nextDir, dev);
			nextL    = nextDot / sqrtf(nextDir2);
		}

		if( prevL > 0 && prevL > nextL )
		{
			vec2d d = prevDir * (prevDot / prevDir2);
			prevPos  = result->coord + d;
			*projection = prevPos;
//			result = prevIt;
			if( offset )
			{
				*offset = d.len();
			}
		}
		else
		if( nextL > 0 && nextL > prevL )
		{
			vec2d d = nextDir * (nextDot / nextDir2);
			nextPos  = result->coord + d;
			*projection = nextPos;
			if( offset )
			{
				*offset = -d.len();
			}
		}
		else
		{
			*projection = result->coord;
			if( offset )
			{
				*offset = 0;
			}
		}
	}

	return result;
}

void DrivingAgent::ClearPath()
{
	_path.clear();
	_attackList.clear();
}

static void RotateTo(const GC_Vehicle &vehicle, VehicleState *outState, const vec2d &x, bool moveForvard, bool moveBack)
{
	assert(!std::isnan(x.x) && !std::isnan(x.y));
	assert(std::isfinite(x.x) && std::isfinite(x.y));

	vec2d newDirection = x - vehicle.GetPos();
	newDirection.Normalize();

	float cosDiff = Vec2dDot(newDirection, vehicle.GetDirection());
	float minDiff = std::cos(MIN_PATH_ANGLE);

	if (moveForvard)
	{
		outState->gas = cosDiff > minDiff ? 1.f : 0.f;
	}
	else if (moveBack)
	{
		outState->gas = cosDiff > minDiff ? -1.f : 0.f;
	}
	outState->steering = newDirection;
}

void DrivingAgent::ComputeState(World &world, const GC_Vehicle &vehicle, float dt, VehicleState &vs)
{
	vec2d brake = vehicle.GetBrakingLength();
//	float brake_len = brake.len();
	float brakeSqr = brake.sqr();

	vec2d currentDir = vehicle.GetDirection();
	vec2d currentPos = vehicle.GetPos();
//	vec2d predictedPos = GetVehicle()->GetPos() + brake;

	if (!_path.empty())
	{
//		vec2d predictedProj;
//		std::list<PathNode>::const_iterator predictedNodeIt = FindNearPathNode(predictedPos, &predictedProj, nullptr);

		vec2d currentProj;
		float offset;
		std::list<PathNode>::const_iterator currentNodeIt = FindNearPathNode(currentPos, &currentProj, &offset);

		float desiredProjOffsetLen = vehicle.GetMaxBrakingLength() * 2;//(1 + vehicle._lv.len() / vehicle.GetMaxSpeed());
		vec2d desiredProjOffset;

		std::list<PathNode>::const_iterator it = currentNodeIt;
		if (offset > 0)
		{
			vec2d d = it->coord;
			--it;
			d -= it->coord;
			offset -= d.len();
		}
		offset += std::min((currentPos - currentProj).len(), desiredProjOffsetLen);
		for (;;)
		{
			vec2d d = it->coord;
			if (++it == _path.end())
			{
				desiredProjOffset = d;
				break;
			}
			d -= it->coord;
			float len = d.len();
			if (offset + len < desiredProjOffsetLen)
			{
				offset += len;
			}
			else
			{
				float ratio = 1 - (desiredProjOffsetLen - offset) / len;
				desiredProjOffset = it->coord + d * ratio;
				break;
			}
		}

		if (++currentNodeIt == _path.end() && (currentProj - currentPos).sqr() < WORLD_BLOCK_SIZE*WORLD_BLOCK_SIZE / 16)
		{
			_path.clear(); // end of path
		}
		//else
		//{
		//	if( (predictedPos - predictedProj).sqr() < WORLD_BLOCK_SIZE*WORLD_BLOCK_SIZE && brake_len > 1 )
		//	{
		//		_arrivalPoint = predictedProj + brake;
		//	}
		//	else
		//	{
		_arrivalPoint = desiredProjOffset;
		//	}
		//}

	}


	/*
	while( !_path.empty() )
	{
	float desired = _path.size()>1 ? (_pickupCurrent ?
	_pickupCurrent->GetRadius() : 30.0f) : (float) WORLD_BLOCK_SIZE / 2;
	float current = (GetVehicle()->GetPos() - _path.front().coord).len();
	if( current > desired )
	{
	break;
	}
	_path.pop_front();
	}

	if( !_path.empty() )
	destPoint = _path.front().coord;
	*/

	vs.rotateWeapon = true;
	vs.weaponAngle = 0;


	//
	// avoid obstacles
	//

	if (brake.sqr() > 0)
	{
		vec2d angle[] = { Vec2dDirection(PI / 4), Vec2dDirection(0), Vec2dDirection(-PI / 4) };
		float len[] = { 1,2,1 };

		float min_d = -1;
		vec2d min_hit, min_norm;
		for (int i = 0; i < 3; ++i)
		{
			vec2d tmp = Vec2dAddDirection(vehicle.GetDirection(), vec2d(angle[i]));

			vec2d x0 = vehicle.GetPos() + tmp * vehicle.GetRadius();
			vec2d a = brake * len[i];

			vec2d hit, norm;
			GC_Object *o = world.TraceNearest(
				world.grid_rigid_s,
				&vehicle,
				x0,
				a,
				&hit,
				&norm
			);

			if (o)
			{
				if (1 == i && (hit - currentPos).len() < vehicle.GetRadius())
				{
					_backTime = 0.5f;
				}

				float d = (hit - x0).sqr();
				if (min_d < 0 || d < min_d)
				{
					min_d = d;
					min_hit = hit;
					min_norm = norm;
				}
			}
		}

		if (min_d > 0)
		{
			vec2d hit_dir = currentDir; // (min_hit - currentPos).Normalize();
			min_norm = (min_norm - hit_dir * Vec2dDot(min_norm, hit_dir)).Normalize() + min_norm;
			min_norm.Normalize();
			min_norm *= 1.4142f;// sqrt(2)
			_arrivalPoint = min_hit + min_norm * vehicle.GetRadius();
//			DbgLine(min_hit, _arrivalPoint, 0xff0000ff);
		}
	}


	//
	// follow the path
	//

	float dst = (currentPos - _arrivalPoint).sqr();
	if (dst > WORLD_BLOCK_SIZE*WORLD_BLOCK_SIZE / 16)
	{
		if (_backTime <= 0)
		{
			RotateTo(vehicle, &vs, _arrivalPoint, dst > brakeSqr, false);
		}
		else
		{
			vs.gas = -1;
		}
	}


	_backTime -= dt;

	if ((vs.gas != 0) && vehicle._lv.len() < vehicle.GetMaxSpeed() * 0.3f)
	{
		_stickTime += dt;
		if (_stickTime > 0.6f)
		{
			_backTime = world.net_frand(0.5f);
		}
	}
	else
	{
		_stickTime = 0;
	}
}

void DrivingAgent::StayAway(const GC_Vehicle &vehicle, vec2d fromCenter, float radius)
{
	vec2d d = fromCenter - vehicle.GetPos();
	float d_len = d.len();
	if (d_len < radius)
	{
		if (d_len < 1)
		{
			d = -vehicle.GetDirection();
			d_len = 1;
		}
		_arrivalPoint = vehicle.GetPos() + d * (1 - radius / d_len);
	}
}
