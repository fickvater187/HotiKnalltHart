#pragma once


class ShotRecord;

class Resolver {
public:
	enum Modes : size_t {
		RESOLVE_NONE = 0,
		RESOLVE_WALK,
		RESOLVE_STAND,
		RESOLVE_STAND1,
		RESOLVE_STAND2,
		RESOLVE_AIR,
		RESOLVE_LASTMOVE,
		RESOLVE_OVERRIDE,
		RESOLVE_UNKNOWM,
		RESOLVE_BODY,
		RESOLVE_STOPPED_MOVING,
		RESOLVE_LBY_UPDATE,
		RESOLVE_LAST_LBY,
	};

	struct AntiFreestandingRecord
	{
		int right_damage = 0, left_damage = 0, back_damage = 0;
		float right_fraction = 0.f, left_fraction = 0.f, back_fraction = 0.f;
	};

public:
	LagRecord* FindIdealRecord(AimPlayer* data);
	LagRecord* FindLastRecord(AimPlayer* data);

	LagRecord* FindFirstRecord(AimPlayer* data);

	void OnBodyUpdate(Player* player, float value);
	float GetAwayAngle(LagRecord* record);

	void MatchShot(AimPlayer* data, LagRecord* record);
	void SetMode(LagRecord* record);

	void DetectSide(Player* player, int* side);

	void ResolveAngles(Player* player, LagRecord* record);
	void ResolveWalk(AimPlayer* data, LagRecord* record);
	float GetLBYRotatedYaw(float lby, float yaw);
	bool IsYawSideways(Player* entity, float yaw);
	void ResolveYawBruteforce(LagRecord* record, Player* player, AimPlayer* data);
	float GetDirectionAngle(int index, Player* player);
	void LastMoveLby(LagRecord* record, AimPlayer* data, Player* player);
	void ResolveStand(AimPlayer* data, LagRecord* record);
	void StandNS(AimPlayer* data, LagRecord* record);
	bool AntiFreestanding(Player* entity, AimPlayer* data, float& yaw);
	void ResolveAir(AimPlayer* data, LagRecord* record, Player* player);
	void ResolveAir(AimPlayer* data, LagRecord* record);
	void resolve(Player* entity, LagRecord* record);

	void collect_wall_detect(const Stage_t stage);
	void AirNS(AimPlayer* data, LagRecord* record);
	void ResolvePoses(Player* player, LagRecord* record);

	bool IdealFreestand(Player* entity, float& yaw, int damage_toleranc);
	bool lby_updated(LagRecord* a, AimPlayer* b, Player* entity);
	bool can_backtrack(Player* entity);
	void lby_update_checks(Player* entity, LagRecord* a, AimPlayer* b);
	void store(Player* entity, float yaw, AimPlayer* b);

	void ResolveOverride(Player* player, LagRecord* record, AimPlayer* data);

	void AntiFreestand(Player* pEnemy, float& y, float flLeftDamage, float flRightDamage, float flRightFraction, float flLeftFraction, float flToMe, int& iShotsMissed);

public:
	std::array< vec3_t, 64 > m_impacts;

	// check if the players yaw is sideways.
	__forceinline bool IsLastMoveValid(LagRecord* record, float m_yaw) {
		const auto away = GetAwayAngle(record);
		const float delta = fabs(math::NormalizedAngle(away - m_yaw));
		return delta > 20.f && delta < 160.f;
	}

	AntiFreestandingRecord anti_freestanding_record;

	class PlayerResolveRecord
	{
	public:
		struct AntiFreestandingRecord
		{
			int right_damage = 0, left_damage = 0;
			float right_fraction = 0.f, left_fraction = 0.f;
		};

	public:
		AntiFreestandingRecord m_sAntiEdge;
	};

	vec3_t last_eye;

	bool using_anti_freestand;

	float left_damage[64];
	float right_damage[64];
	float back_damage[64];

	std::vector<vec3_t> last_eye_positions;

};

extern Resolver g_resolver;