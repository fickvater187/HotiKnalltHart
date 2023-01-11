#include "includes.h"

Resolver g_resolver{};;

LagRecord* Resolver::FindIdealRecord( AimPlayer* data ) {
    LagRecord *first_valid, *current;

	if( data->m_records.empty( ) )
		return nullptr;

    first_valid = nullptr;

    // iterate records.
	for( const auto &it : data->m_records ) {
		if( it->dormant( ) || it->immune( ) || !it->valid( ) )
			continue;

        // get current record.
        current = it.get( );

        // first record that was valid, store it for later.
        if( !first_valid )
            first_valid = current;

        // try to find a record with a shot, lby update, walking or no anti-aim.
		if( it->m_shot || it->m_mode == Modes::RESOLVE_BODY || it->m_mode == Modes::RESOLVE_WALK || it->m_mode == Modes::RESOLVE_NONE )
            return current;
	}

	// none found above, return the first valid record if possible.
	return ( first_valid ) ? first_valid : nullptr;
}

LagRecord* Resolver::FindLastRecord( AimPlayer* data ) {
    LagRecord* current;

	if( data->m_records.empty( ) )
		return nullptr;

	// iterate records in reverse.
	for( auto it = data->m_records.crbegin( ); it != data->m_records.crend( ); ++it ) {
		current = it->get( );

		// if this record is valid.
		// we are done since we iterated in reverse.
		if( current->valid( ) && !current->immune( ) && !current->dormant( ) )
			return current;
	}

	return nullptr;
}




void Resolver::OnBodyUpdate( Player* player, float value ) {
	AimPlayer* data = &g_aimbot.m_players[ player->index( ) - 1 ];

	// set data.
	data->m_old_body = data->m_body;
	data->m_body     = value;
}

float Resolver::GetAwayAngle( LagRecord* record ) {
	float  delta{ std::numeric_limits< float >::max( ) };
	vec3_t pos;
	ang_t  away;

	// other cheats predict you by their own latency.
	// they do this because, then they can put their away angle to exactly
	// where you are on the server at that moment in time.

	// the idea is that you would need to know where they 'saw' you when they created their user-command.
	// lets say you move on your client right now, this would take half of our latency to arrive at the server.
	// the delay between the server and the target client is compensated by themselves already, that is fortunate for us.

	// we have no historical origins.
	// no choice but to use the most recent one.
	//if( g_cl.m_net_pos.empty( ) ) {
		math::VectorAngles( g_cl.m_local->m_vecOrigin( ) - record->m_pred_origin, away );
		return away.y;
	//}

	// half of our rtt.
	// also known as the one-way delay.
	//float owd = ( g_cl.m_latency / 2.f );

	// since our origins are computed here on the client
	// we have to compensate for the delay between our client and the server
	// therefore the OWD should be subtracted from the target time.
	//float target = record->m_pred_time; //- owd;

	// iterate all.
	//for( const auto &net : g_cl.m_net_pos ) {
	//for( const auto &net : g_cl.m_net_pos ) {
		// get the delta between this records time context
		// and the target time.
	//	float dt = std::abs( target - net.m_time );

		// the best origin.
	//	if( dt < delta ) {
	//		delta = dt;
	//		pos   = net.m_pos;
	//	}
	//}

	//math::VectorAngles( pos - record->m_pred_origin, away );
	//return away.y;
}

void Resolver::MatchShot( AimPlayer* data, LagRecord* record ) {
	// do not attempt to do this in nospread mode.
	if( g_menu.main.config.mode.get( ) == 1 )
		return;

	float shoot_time = -1.f;

	Weapon* weapon = data->m_player->GetActiveWeapon( );
	if( weapon ) {
		// with logging this time was always one tick behind.
		// so add one tick to the last shoot time.
		shoot_time = weapon->m_fLastShotTime( ) + g_csgo.m_globals->m_interval;
	}

	// this record has a shot on it.
	if( game::TIME_TO_TICKS( shoot_time ) == game::TIME_TO_TICKS( record->m_sim_time ) ) {
		if( record->m_lag <= 2 )
			record->m_shot = true;
		
		// more then 1 choke, cant hit pitch, apply prev pitch.
		else if( data->m_records.size( ) >= 2 ) {
			LagRecord* previous = data->m_records[ 1 ].get( );

			if( previous && !previous->dormant( ) )
				record->m_eye_angles.x = previous->m_eye_angles.x;
		}
	}
}

void Resolver::SetMode( LagRecord* record ) {
	// the resolver has 3 modes to chose from.
	// these modes will vary more under the hood depending on what data we have about the player
	// and what kind of hack vs. hack we are playing (mm/nospread).

	float speed = record->m_anim_velocity.length( );

	// if on ground, moving, and not fakewalking.
	if( ( record->m_flags & FL_ONGROUND ) && speed > 0.1f && !record->m_fake_walk )
		record->m_mode = Modes::RESOLVE_WALK;

	// if on ground, not moving or fakewalking.
	if( ( record->m_flags & FL_ONGROUND ) && ( speed <= 0.1f || record->m_fake_walk ) )
		record->m_mode = Modes::RESOLVE_STAND;

	// if not on ground.
	else if( !( record->m_flags & FL_ONGROUND ) )
		record->m_mode = Modes::RESOLVE_AIR;
}

void Resolver::AntiFreestand(LagRecord* record)
{
	// constants
	constexpr float STEP{ 4.f };
	constexpr float RANGE{ 32.f };

	// best target.
	vec3_t enemypos = record->m_player->GetShootPosition();
	float away = GetAwayAngle(record);

	// construct vector of angles to test.
	std::vector< AdaptiveAngle > angles{ };
	angles.emplace_back(away - 179.f);
	angles.emplace_back(away + 89.f);
	angles.emplace_back(away - 89.f);

	// start the trace at the your shoot pos.
	vec3_t start = g_cl.m_local->GetShootPosition();

	// see if we got any valid result.
	// if this is false the path was not obstructed with anything.
	bool valid{ false };

	// iterate vector of angles.
	for (auto it = angles.begin(); it != angles.end(); ++it) {

		// compute the 'rough' estimation of where our head will be.
		vec3_t end{ enemypos.x + std::cos(math::deg_to_rad(it->m_yaw)) * RANGE,
			enemypos.y + std::sin(math::deg_to_rad(it->m_yaw)) * RANGE,
			enemypos.z };

		// draw a line for debugging purposes.
		//Color clr = g_menu.main.aimbot.debuglinecolor.get();
		//if (g_menu.main.aimbot.debugline.get()) {
		//	g_csgo.m_debug_overlay->AddLineOverlay(start, end, clr.r(), clr.g(), clr.b(), true, 0.1f);
		//}

		// compute the direction.
		vec3_t dir = end - start;
		float len = dir.normalize();

		// should never happen.
		if (len <= 0.f)
			continue;

		// step thru the total distance, 4 units per step.
		for (float i{ 0.f }; i < len; i += STEP) {
			// get the current step position.
			vec3_t point = start + (dir * i);

			// get the contents at this point.
			int contents = g_csgo.m_engine_trace->GetPointContents(point, MASK_SHOT_HULL);

			// contains nothing that can stop a bullet.
			if (!(contents & MASK_SHOT_HULL))
				continue;

			float mult = 1.f;

			// over 50% of the total length, prioritize this shit.
			if (i > (len * 0.5f))
				mult = 1.25f;

			// over 90% of the total length, prioritize this shit.
			if (i > (len * 0.75f))
				mult = 1.25f;

			// over 90% of the total length, prioritize this shit.
			if (i > (len * 0.9f))
				mult = 2.f;

			// append 'penetrated distance'.
			it->m_dist += (STEP * mult);

			// mark that we found anything.
			valid = true;
		}
	}

	if (!valid) {
		return;
	}

	// put the most distance at the front of the container.a
	std::sort(angles.begin(), angles.end(),
		[](const AdaptiveAngle& a, const AdaptiveAngle& b) {
			return a.m_dist > b.m_dist;
		});

	// the best angle should be at the front now.
	AdaptiveAngle* best = &angles.front();

	record->m_eye_angles.y = best->m_yaw;
}

Resolver::Directions Resolver::HandleDirections(AimPlayer* data) {
	CGameTrace tr;
	CTraceFilterSimple filter{ };

	if (!g_cl.m_processing)
		return Directions::YAW_NONE;

	// best target.
	struct AutoTarget_t { float fov; Player* player; };
	AutoTarget_t target{ 180.f + 1.f, nullptr };

	// get best target based on fov.
	auto origin = data->m_player->m_vecOrigin();
	ang_t view;
	float fov = math::GetFOV(g_cl.m_cmd->m_view_angles, g_cl.m_local->GetShootPosition(), data->m_player->WorldSpaceCenter());

	// set best fov.
	if (fov < target.fov) {
		target.fov = fov;
		target.player = data->m_player;
	}

	// get best player.
	const auto player = target.player;
	if (!player)
		return Directions::YAW_NONE;

	auto& bestOrigin = player->m_vecOrigin();

	// skip this player in our traces.
	filter.SetPassEntity(g_cl.m_local);

	// calculate angle direction from thier best origin to our origin
	ang_t angDirectionAngle;
	math::VectorAngles(g_cl.m_local->m_vecOrigin() - bestOrigin, angDirectionAngle);

	vec3_t forward, right, up;
	math::AngleVectors(angDirectionAngle, &forward, &right, &up);

	auto vecStart = g_cl.m_local->GetShootPosition();
	auto vecEnd = vecStart + forward * 100.0f;

	Ray rightRay(vecStart + right * 35.0f, vecEnd + right * 35.0f), leftRay(vecStart - right * 35.0f, vecEnd - right * 35.0f);

	g_csgo.m_engine_trace->TraceRay(rightRay, MASK_SOLID, &filter, &tr);
	float rightLength = (tr.m_endpos - tr.m_startpos).length();

	g_csgo.m_engine_trace->TraceRay(leftRay, MASK_SOLID, &filter, &tr);
	float leftLength = (tr.m_endpos - tr.m_startpos).length();

	static auto leftTicks = 0;
	static auto rightTicks = 0;
	static auto backTicks = 0;

	if (rightLength - leftLength > 20.0f)
		leftTicks++;
	else
		leftTicks = 0;

	if (leftLength - rightLength > 20.0f)
		rightTicks++;
	else
		rightTicks = 0;

	if (fabs(rightLength - leftLength) <= 20.0f)
		backTicks++;
	else
		backTicks = 0;

	Directions direction = Directions::YAW_NONE;

	if (rightTicks > 10) {
		direction = Directions::YAW_RIGHT;
	}
	else {
		if (leftTicks > 10) {
			direction = Directions::YAW_LEFT;
		}
		else {
			if (backTicks > 10)
				direction = Directions::YAW_BACK;
		}
	}

	return direction;
}

float Resolver::GetLBYRotatedYaw(float lby, float yaw)
{
	float delta = math::NormalizedAngle(yaw - lby);
	if (fabs(delta) < 25.f)
		return lby;

	if (delta > 0.f)
		return yaw + 25.f;

	return yaw;
}

void Resolver::ResolveAngles( Player* player, LagRecord* record ) {
	AimPlayer* data = &g_aimbot.m_players[ player->index( ) - 1 ];

	// mark this record if it contains a shot.
	MatchShot( data, record );

	// next up mark this record with a resolver mode that will be used.
	SetMode( record );

	// if we are in nospread mode, force all players pitches to down.
	// TODO; we should check thei actual pitch and up too, since those are the other 2 possible angles.
	// this should be somehow combined into some iteration that matches with the air angle iteration.
	if( g_menu.main.config.mode.get( ) == 1 )
		record->m_eye_angles.x = 90.f;

	// we arrived here we can do the acutal resolve.
	if( record->m_mode == Modes::RESOLVE_WALK ) 
		ResolveWalk( data, record );

	else if( record->m_mode == Modes::RESOLVE_STAND )
		ResolveStand( data, record );

	else if( record->m_mode == Modes::RESOLVE_AIR )


	// normalize the eye angles, doesn't really matter but its clean.
	math::NormalizeAngle( record->m_eye_angles.y );
}

void Resolver::ResolveWalk( AimPlayer* data, LagRecord* record ) {
	// apply lby to eyeangles.
	record->m_eye_angles.y = record->m_body;

	// delay body update.
	data->m_body_update = record->m_anim_time + 0.22f;

	// reset stand and body index.
	data->m_stand_index  = 0;
	data->m_stand_index2 = 0;
	data->m_body_index   = 0;

	// copy the last record that this player was walking
	// we need it later on because it gives us crucial data.
	std::memcpy( &data->m_walk_record, record, sizeof( LagRecord ) );
}

void Resolver::ResolveStand( AimPlayer* data, LagRecord* record ) {
	// for no-spread call a seperate resolver.
	if( g_menu.main.config.mode.get( ) == 1 ) {
		StandNS( data, record );
		return;
	}

	// get predicted away angle for the player.
	float away = GetAwayAngle( record );

	// pointer for easy access.
	LagRecord* move = &data->m_walk_record;

	// we have a valid moving record.
	if( move->m_sim_time > 0.f ) {
		vec3_t delta = move->m_origin - record->m_origin;

		// check if moving record is close.
		if( delta.length( ) <= 128.f ) {
			// indicate that we are using the moving lby.
			data->m_moved = true;
		}
	}

	// a valid moving context was found
	if( data->m_moved ) {
		float diff = math::NormalizedAngle( record->m_body - move->m_body );
		float delta = record->m_anim_time - move->m_anim_time;

		// it has not been time for this first update yet.
		if( delta < 0.22f ) {
			// set angles to current LBY.
			record->m_eye_angles.y = move->m_body;

			// set resolve mode.
			record->m_mode = Modes::RESOLVE_STOPPED_MOVING;

			// exit out of the resolver, thats it.
			return;
		}

		// LBY SHOULD HAVE UPDATED HERE.
		else if( record->m_anim_time >= data->m_body_update ) {
			// only shoot the LBY flick 3 times.
			// if we happen to miss then we most likely mispredicted.
			if( data->m_body_index <= 3 ) {
				// set angles to current LBY.
				record->m_eye_angles.y = record->m_body;

				// predict next body update.
				data->m_body_update = record->m_anim_time + 1.1f;

				// set the resolve mode.
				record->m_mode = Modes::RESOLVE_BODY;

				return;
			}

			// set to stand1 -> known last move.
			record->m_mode = Modes::RESOLVE_STAND1;

			C_AnimationLayer* curr = &record->m_layers[ 3 ];
			int act = data->m_player->GetSequenceActivity( curr->m_sequence );

			// ok, no fucking update. apply big resolver.
			record->m_eye_angles.y = move->m_body;

			// every third shot do some fuckery.
			if ( !( data->m_stand_index % 3 ) )
				record->m_eye_angles.y += g_csgo.RandomFloat( -35.f, 35.f );

			// jesus we can fucking stop missing can we?
			if( data->m_stand_index > 6 && act != 980 ) {
				// lets just hope they switched ang after move.
				record->m_eye_angles.y = move->m_body + 180.f;
			}

			// we missed 4 shots.
			else if( data->m_stand_index > 4 && act != 980 ) {
				// try backwards.
				record->m_eye_angles.y = away + 180.f;
			}

			return;
		}
	}

	// stand2 -> no known last move.
	record->m_mode = Modes::RESOLVE_STAND2;

	switch( data->m_stand_index2 % 6 ) {

	case 0:
		record->m_eye_angles.y = away + 180.f;
		break;

	case 1:
		record->m_eye_angles.y = record->m_body;
		break;

	case 2:
		record->m_eye_angles.y = record->m_body + 180.f;
		break;

	case 3:
		record->m_eye_angles.y = record->m_body + 110.f;
		break;

	case 4:
		record->m_eye_angles.y = record->m_body - 110.f;
		break;

	case 5:
		record->m_eye_angles.y = away;
		break;

	default:
		break;
	}
}

void Resolver::StandNS( AimPlayer* data, LagRecord* record ) {
	// get away angles.
	float away = GetAwayAngle( record );

	switch( data->m_shots % 8 ) {
	case 0:
		record->m_eye_angles.y = away + 180.f;
		break;

	case 1:
		record->m_eye_angles.y = away + 90.f;
		break;
	case 2:
		record->m_eye_angles.y = away - 90.f;
		break;

	case 3:
		record->m_eye_angles.y = away + 45.f;
		break;
	case 4:
		record->m_eye_angles.y = away - 45.f;
		break;

	case 5:
		record->m_eye_angles.y = away + 135.f;
		break;
	case 6:
		record->m_eye_angles.y = away - 135.f;
		break;

	case 7:
		record->m_eye_angles.y = away + 0.f;
		break;

	default:
		break;
	}

	// force LBY to not fuck any pose and do a true bruteforce.
	record->m_body = record->m_eye_angles.y;
}

void Resolver::ResolveAir(AimPlayer* data, LagRecord* record, Player* player) {
// for no-spread call a seperate resolver.
if (g_menu.main.config.mode.get() == 1) {
	AirNS(data, record);
	return;
}

// else run our matchmaking air resolver.

// we have barely any speed. 
// either we jumped in place or we just left the ground.
// or someone is trying to fool our resolver.
if (record->m_anim_velocity.length_2d() < 60.f) {
	// set this for completion.
	// so the shot parsing wont pick the hits / misses up.
	// and process them wrongly.
	record->m_mode = Modes::RESOLVE_LASTMOVE;

	// invoke our stand resolver.
	LastMoveLby(record, data, player);

	// we are done.
	return;
}

// try to predict the direction of the player based on his velocity direction.
// this should be a rough estimation of where he is looking.
float velyaw = math::rad_to_deg(std::atan2(record->m_anim_velocity.y, record->m_anim_velocity.x));

switch (data->m_shots % 3) {
case 0:
	record->m_eye_angles.y = velyaw + 180.f;
	break;

case 1:
	record->m_eye_angles.y = velyaw - 90.f;
	break;

case 2:
	record->m_eye_angles.y = velyaw + 90.f;
	break;
}
}



bool Resolver::lby_updated(LagRecord* a, AimPlayer* b, Player* entity)
{
if (b->m_body != b->m_old_body) return true;

if ((entity->m_flSimulationTime() + 1.1f + g_csgo.m_globals->m_interval) < entity->m_flSimulationTime()) return true;

bool set = true;

C_AnimationLayer _previous;

for (int i = 0; i < 13; i++)
{
	C_AnimationLayer layer = entity->m_AnimOverlay()[i];
	if (set) {
		_previous = layer;
		set = false;
	}
	const int activity = entity->GetSequenceActivity(layer.m_sequence);
	const int previous_act = entity->GetSequenceActivity(_previous.m_sequence);

	if (activity == 979 && previous_act == 979) {
		if ((_previous.m_cycle != layer.m_cycle) || layer.m_weight == 1.f)
		{
			float
				flAnimTime = layer.m_cycle,
				flSimTime = entity->m_flSimulationTime();

			if (flAnimTime < 0.01f && _previous.m_cycle > 0.01f)
			{
				return true;
			}
		}
	}
	_previous = layer;
}

return false;
}

bool Resolver::can_backtrack(Player* entity)
{
	float lby_update_time = entity->m_flSimulationTime();
	float current_time = g_csgo.m_globals->m_curtime;

	return ((current_time - lby_update_time) <= 0.2f);
}

void Resolver::lby_update_checks(Player* entity, LagRecord* a, AimPlayer* b) {

	// pointer for easy access.
	LagRecord* move = &b->m_walk_record;
	float ave_moving_lby = move->m_body;

	float _delta_a = abs(a->m_eye_angles.y - ave_moving_lby);
	float _delta_b = abs(a->m_body - ave_moving_lby);
	float _delta_c = abs(b->m_body - ave_moving_lby);

	if (_delta_a <= _delta_b && _delta_a <= _delta_c && _delta_a <= 20) {
		return;
	}

	if (b->m_body, ave_moving_lby, 20) (a->m_body, ave_moving_lby, 20); {
		float ave = b->m_body + a->m_body;

		if (ave != 0) ave /= 2;
		a->m_eye_angles.y = ave;
		return;
	}

	if (_delta_b < _delta_c && _delta_b <= 20) {
		a->m_eye_angles.y = b->m_body;
		return;
	}

	if (_delta_c < _delta_b && _delta_c <= 20) {
		a->m_eye_angles.y = a->m_body;
		return;
	}

	a->m_eye_angles.y = entity->GetAbsAngles().y;
}

void Resolver::AirNS(AimPlayer* data, LagRecord* record) {
	// get away angles.
	float away = GetAwayAngle(record);

	switch (data->m_shots % 9) {
	case 0:
		record->m_eye_angles.y = away + 180.f;
		break;

	case 1:
		record->m_eye_angles.y = away + 150.f;
		break;
	case 2:
		record->m_eye_angles.y = away - 150.f;
		break;

	case 3:
		record->m_eye_angles.y = away + 165.f;
		break;
	case 4:
		record->m_eye_angles.y = away - 165.f;
		break;

	case 5:
		record->m_eye_angles.y = away + 135.f;
		break;
	case 6:
		record->m_eye_angles.y = away - 135.f;
		break;

	case 7:
		record->m_eye_angles.y = away + 90.f;
		break;
	case 8:
		record->m_eye_angles.y = away - 90.f;
		break;

	default:
		break;
	}
}

void Resolver::ResolvePoses( Player* player, LagRecord* record ) {
	AimPlayer* data = &g_aimbot.m_players[ player->index( ) - 1 ];

	// only do this bs when in air.
	if( record->m_mode == Modes::RESOLVE_AIR ) {
		// ang = pose min + pose val x ( pose range )

		// lean_yaw
		player->m_flPoseParameter( )[ 2 ]  = g_csgo.RandomInt( 0, 4 ) * 0.25f;   

		// body_yaw
		player->m_flPoseParameter( )[ 11 ] = g_csgo.RandomInt( 1, 3 ) * 0.25f;
	}
}


void Resolver::ResolveYawBruteforce(LagRecord* record, Player* player, AimPlayer* data)
{
	auto local_player = g_cl.m_local;
	if (!local_player)
		return;

	record->m_mode = Modes::RESOLVE_STAND;

	const float at_target_yaw = math::CalcAngle(player->m_vecOrigin(), local_player->m_vecOrigin()).y;

	switch (data->m_stand_index % 3)
	{
	case 0:
		record->m_eye_angles.y = GetLBYRotatedYaw(player->m_flLowerBodyYawTarget(), at_target_yaw + 60.f);
		break;
	case 1:
		record->m_eye_angles.y = at_target_yaw + 140.f;
		break;
	case 2:
		record->m_eye_angles.y = at_target_yaw - 75.f;
		break;
	}
}

void Resolver::LastMoveLby(LagRecord* record, AimPlayer* data, Player* player)
{
	// for no-spread call a seperate resolver.
	if (g_menu.main.config.mode.get() == 1) {
		StandNS(data, record);
		return;
	}

	// pointer for easy access.
	LagRecord* move = &data->m_walk_record;

	// get predicted away angle for the player.
	float away = GetAwayAngle(record);

	C_AnimationLayer* curr = &record->m_layers[3];
	int act = data->m_player->GetSequenceActivity(curr->m_sequence);

	float diff = math::NormalizedAngle(record->m_body - move->m_body);
	float delta = record->m_anim_time - move->m_anim_time;

	ang_t vAngle = ang_t(0, 0, 0);
	math::CalcAngle3(player->m_vecOrigin(), g_cl.m_local->m_vecOrigin(), vAngle);

	float flToMe = vAngle.y;

	const float at_target_yaw = math::CalcAngle(g_cl.m_local->m_vecOrigin(), player->m_vecOrigin()).y;

	/*for (int i = 1; i <= 32; i++)
	{
		Player* pEnemy = g_csgo.m_entlist->GetClientEntity< Player* >(i);
		const auto freestanding_record = player_resolve_records[i].m_sAntiEdge;

		AntiFreestand(player, record->m_eye_angles.y, freestanding_record.left_damage, freestanding_record.right_damage, freestanding_record.right_fraction, freestanding_record.left_fraction, flToMe, data->m_last_move);
	}*/

	const auto freestanding_record = player_resolve_records[player->index()].m_sAntiEdge;

	// we have a valid moving record.
	if (move->m_sim_time > 0.f) {
		vec3_t delta = move->m_origin - record->m_origin;

		// check if moving record is close.
		if (delta.length() <= 128.f) {
			// indicate that we are using the moving lby.
			data->m_moved = true;
		}
	}

	if (!data->m_moved) {

		record->m_mode = Modes::RESOLVE_UNKNOWM;

		//record->m_eye_angles.y = GetDirectionAngle(player->index(), player);

		ResolveYawBruteforce(record, player, data);

		/*
			const auto left_thickness = g_cl.m_left_thickness[index];
			const auto right_thickness = g_cl.m_right_thickness[index];
			const auto at_target_angle = g_cl.m_at_target_angle[index];
		*/

		//AntiFreestand(player, record->m_eye_angles.y, freestanding_record.left_damage, freestanding_record.right_damage, freestanding_record.right_fraction, freestanding_record.left_fraction, at_target_yaw, data->m_last_move);

		if (data->m_body != data->m_old_body)
		{
			record->m_eye_angles.y = record->m_body;

			data->m_body_update = record->m_anim_time + 1.1f;

			iPlayers[record->m_player->index()] = false;
			record->m_mode = Modes::RESOLVE_BODY;
		}
	}
	else if (data->m_moved) {
		float diff = math::NormalizedAngle(record->m_body - move->m_body);
		float delta = record->m_anim_time - move->m_anim_time;

		record->m_mode = Modes::RESOLVE_LASTMOVE;
		//data->m_last_move

		const float at_target_yaw = math::CalcAngle(g_cl.m_local->m_vecOrigin(), player->m_vecOrigin()).y;


		//if (IsYawSideways(player, move->m_body)) // anti-urine
		record->m_eye_angles.y = move->m_body;
		//else
		//	record->m_eye_angles.y = away + 180.f;

		//record->m_eye_angles.y = GetLBYRotatedYaw(player->m_flLowerBodyYawTarget(), move->m_body);

		if (data->m_last_move >= 1)
			ResolveYawBruteforce(record, player, data);

		//record->m_eye_angles.y = GetDirectionAngle(player->index(), player);

		if (data->m_body != data->m_old_body)
		{
			/*auto lby = math::normalize_float(record->m_body);
			if (fabsf(record->m_eye_angles.y - lby) <= 150.f && fabsf(record->m_eye_angles.y - lby) >= 35.f) {
				record->m_eye_angles.y ? lby -= 25.f : lby += 25.f;
			}
			record->m_eye_angles.y = lby;
			player->SetAbsAngles(ang_t(0.f, lby, 0.f));*/

			record->m_eye_angles.y = record->m_body;

			data->m_body_update = record->m_anim_time + 1.1f;
			iPlayers[record->m_player->index()] = false;
			record->m_mode = Modes::RESOLVE_BODY;
		}
		/*else
		{
			// LBY SHOULD HAVE UPDATED HERE.
			if (record->m_anim_time >= data->m_body_update) {
				// only shoot the LBY flick 3 times.
				// if we happen to miss then we most likely mispredicted
				if (data->m_body_index < 1) {
					// set angles to current LBY.
					record->m_eye_angles.y = record->m_body;

					data->m_body_update = record->m_anim_time + 1.1f;

					// set the resolve mode.
					iPlayers[record->m_player->index()] = false;
					record->m_mode = Modes::RESOLVE_BODY;
				}
			}
		}*/
		//if (data->m_last_move > 1)
			//AntiFreestand(player, record->m_eye_angles.y, freestanding_record.left_damage, freestanding_record.right_damage, freestanding_record.right_fraction, freestanding_record.left_fraction, flToMe, data->m_last_move);
	}
}