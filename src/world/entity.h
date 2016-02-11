#pragma once

#include "animation/skeleton.h"
#include "core/orientedboundingbox.h"
#include "engine/engine.h"
#include "object.h"

#include <BulletCollision/BroadphaseCollision/btCollisionAlgorithm.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>

#include <cstdint>
#include <memory>

class btCollisionShape;
class btRigidBody;

namespace engine
{
class BtEngineClosestConvexResultCallback;
} // namespace engine

namespace world
{
class Room;
struct RoomSector;
struct RagdollSetup;
class Character;

namespace core
{
struct OrientedBoundingBox;
} // namespace core

#define ENTITY_TYPE_GENERIC                         (0x0000)    // Just an animating.
#define ENTITY_TYPE_INTERACTIVE                     (0x0001)    // Can respond to other entity's commands.
#define ENTITY_TYPE_TRIGGER_ACTIVATOR               (0x0002)    // Can activate triggers.
#define ENTITY_TYPE_HEAVYTRIGGER_ACTIVATOR          (0x0004)    // Can activate heavy triggers.
#define ENTITY_TYPE_PICKABLE                        (0x0008)    // Can be picked up.
#define ENTITY_TYPE_TRAVERSE                        (0x0010)    // Can be pushed/pulled.
#define ENTITY_TYPE_TRAVERSE_FLOOR                  (0x0020)    // Can be walked upon.
#define ENTITY_TYPE_DYNAMIC                         (0x0040)    // Acts as a physical dynamic object.
#define ENTITY_TYPE_ACTOR                           (0x0080)    // Is actor.
#define ENTITY_TYPE_COLLCHECK                       (0x0100)    // Does collision checks for itself.

#define ENTITY_TYPE_SPAWNED                         (0x8000)    // Was spawned.

#define ENTITY_CALLBACK_NONE                        (0x00000000)
#define ENTITY_CALLBACK_ACTIVATE                    (0x00000001)
#define ENTITY_CALLBACK_DEACTIVATE                  (0x00000002)
#define ENTITY_CALLBACK_COLLISION                   (0x00000004)
#define ENTITY_CALLBACK_STAND                       (0x00000008)
#define ENTITY_CALLBACK_HIT                         (0x00000010)
#define ENTITY_CALLBACK_ROOMCOLLISION               (0x00000020)

enum class Substance
{
    None,
    WaterShallow,
    WaterWade,
    WaterSwim,
    QuicksandShallow,
    QuicksandConsumed
};

#define ENTITY_TLAYOUT_MASK     0x1F    // Activation mask
#define ENTITY_TLAYOUT_EVENT    0x20    // Last trigger event
#define ENTITY_TLAYOUT_LOCK     0x40    // Activity lock
#define ENTITY_TLAYOUT_SSTATUS  0x80    // Sector status

//! Entity movement types
enum class MoveType
{
    StaticPos,
    Kinematic,
    OnFloor,
    Wade,
    Quicksand,
    OnWater,
    Underwater,
    FreeFalling,
    Climbing,
    Monkeyswing,
    WallsClimb,
    Dozy
};

ENUM_TO_OSTREAM(MoveType)

//! Surface movement directions
enum class MoveDirection
{
    Stay,
    Forward,
    Backward,
    Left,
    Right,
    Jump,
    Crouch
};

ENUM_TO_OSTREAM(MoveDirection)

class Entity : public Object
{
public:
    int32_t                             m_OCB = 0;                // Object code bit (since TR4)
    uint8_t                             m_triggerLayout = 0;     // Mask + once + event + sector status flags
    float                               m_timer = 0;              // Set by "timer" trigger field

    uint32_t                            m_callbackFlags = 0;     // information about scripts callbacks
    uint16_t                            m_typeFlags = ENTITY_TYPE_GENERIC;
    bool m_enabled = true;
    bool m_active = true;
    bool m_visible = true;

    MoveDirection                       m_moveDir = MoveDirection::Stay;           // (move direction)
    MoveType                            m_moveType = MoveType::OnFloor;          // on floor / free fall / swim ....

    mutable bool m_wasRendered = false;       // render once per frame trigger
    mutable bool m_wasRenderedLines = false; // same for debug lines

    irr::f32 m_currentSpeed = 0;      // current linear speed from animation info
    irr::core::vector3df m_speed = { 0,0,0 };              // speed of the entity XYZ
    irr::f32 m_vspeed_override = 0;

    btScalar                            m_inertiaLinear = 0;     // linear inertia
    btScalar                            m_inertiaAngular[2] = { 0,0 }; // angular inertia - X and Y axes

    animation::Skeleton m_skeleton;

    irr::core::vector3df m_angles = { 0,0,0 };
    irr::core::matrix4 m_transform; // GL transformation matrix
    irr::core::vector3df m_scaling = { 1,1,1 };

    core::OrientedBoundingBox m_obb;

    const RoomSector* m_currentSector = nullptr;
    const RoomSector* m_lastSector = nullptr;

    irr::core::vector3df m_activationOffset = { 0,256,0 };   // where we can activate object (dx, dy, dz)
    irr::f32 m_activationRadius = 128;

    explicit Entity(ObjectId id, World* world);
    ~Entity();

    void enable();
    void disable();

    void ghostUpdate();
    int getPenetrationFixVector(irr::core::vector3df& reaction, bool hasMove);
    void checkCollisionCallbacks();
    bool wasCollisionBodyParts(uint32_t parts_flags) const;
    void updateRoomPos();
    void updateRigidBody(bool force);
    void rebuildBoundingBox();

    boost::optional<size_t> findTransitionCase(LaraState id) const;

    animation::AnimUpdate advanceTime(util::Duration time);
    virtual void frame(util::Duration time);  // entity frame step

    bool isPlayer()
    {
        // FIXME: isPlayer()
        return reinterpret_cast<Entity*>(getWorld()->m_character.get()) == this;
    }

    void updateInterpolation();

    virtual void updateTransform();
    void updateCurrentSpeed(bool zeroVz = 0);
    void addOverrideAnim(const std::shared_ptr<animation::SkeletalModel>& model);
    void checkActivators();

    virtual Substance getSubstanceState() const
    {
        return Substance::None;
    }

    void doAnimCommand(const animation::AnimCommand& command);
    void processSector();
    void setAnimation(animation::AnimationId animation, int frame = 0);
    void moveForward(irr::f32 dist);
    void moveStrafe(irr::f32 dist);
    void moveVertical(irr::f32 dist);

    irr::f32 findDistance(const Entity& entity_2);

    // Constantly updates some specific parameters to keep hair aligned to entity.
    virtual void updateHair()
    {
    }

    bool createRagdoll(RagdollSetup* setup);
    bool deleteRagdoll();

    virtual void fixPenetrations(const irr::core::vector3df* move);
    virtual irr::core::vector3df getRoomPos() const
    {
        auto res = m_skeleton.getBoundingBox().getCenter();
        m_transform.transformVect(res);
        return res;
    }
    virtual void transferToRoom(Room *room);

    virtual void processSectorImpl()
    {
    }
    virtual void jump(irr::f32 /*vert*/, irr::f32 /*hor*/)
    {
    }
    virtual void kill()
    {
    }
    virtual void updateGhostRigidBody()
    {
    }
    virtual std::shared_ptr<engine::BtEngineClosestConvexResultCallback> callbackForCamera() const;

    virtual irr::core::vector3df camPosForFollowing(irr::f32 dz)
    {
        auto cam_pos = m_skeleton.getRootTransform().getTranslation();
        m_transform.transformVect(cam_pos);
        cam_pos.Z += dz;
        return cam_pos;
    }

    virtual void updatePlatformPreStep()
    {
    }

    irr::core::vector3df applyGravity(util::Duration time);

    animation::Skeleton& getSkeleton()
    {
        return m_skeleton;
    }

    const animation::Skeleton& getSkeleton() const
    {
        return m_skeleton;
    }

private:
    static irr::f32 getInnerBBRadius(const core::BoundingBox& bb)
    {
        auto d = bb.max - bb.min;
        return std::min(d.X, std::min(d.Y, d.Z));
    }

    bool getPenetrationFixVector(btPairCachingGhostObject& ghost, btManifoldArray& manifoldArray, irr::core::vector3df& correction) const;
};

struct InventoryItem : public Entity
{
    explicit InventoryItem(ObjectId id, World* world)
        : Entity(id, world)
    {
    }

    animation::ModelId          world_model_id = 0;
    MenuItemType                type;
    size_t                      count = 0;
    std::string name;

    ~InventoryItem();
};

} // namespace world
