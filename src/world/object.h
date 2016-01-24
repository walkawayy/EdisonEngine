#pragma once

#include "util/helpers.h"

#include <cstdint>
#include <iostream>

namespace world
{
struct Room;

enum class CollisionShape
{
    Box,
    BoxBase,      //!< use single box collision
    Sphere,
    TriMesh,      //!< for static objects and room's!
    TriMeshConvex //!< for dynamic objects
};

ENUM_TO_OSTREAM(CollisionShape)

enum class CollisionType
{
    None,
    Static,    //!< static object - never moved
    Kinematic, //!< doors and other moveable statics
    Dynamic,   //!< hellow full physics interaction
    Actor,     //!< actor, enemies, NPC, animals
    Vehicle,   //!< car, moto, bike
    Ghost      //!< no fix character position, but works in collision callbacks and interacts with dynamic objects
};

ENUM_TO_OSTREAM(CollisionType)

using ObjectId = uint32_t;

class Object
{
    Object(const Object&) = delete;
    Object& operator=(const Object&) = delete;

public:
    virtual ~Object() = default;

    Room* getRoom() const noexcept
    {
        return m_room;
    }

    void setRoom(Room* room) noexcept
    {
        m_room = room;
    }

    CollisionType getCollisionType() const noexcept
    {
        return m_collisionType;
    }

    void setCollisionType(CollisionType type) noexcept
    {
        m_collisionType = type;
    }

    CollisionShape getCollisionShape() const noexcept
    {
        return m_collisionShape;
    }

    void setCollisionShape(CollisionShape shape) noexcept
    {
        m_collisionShape = shape;
    }

    ObjectId getId() const noexcept
    {
        return m_id;
    }

protected:
    explicit Object(ObjectId id, Room* room = nullptr)
        : m_id(id)
        , m_room(room)
    {
    }

private:
    const ObjectId m_id;                 // Unique entity ID

    world::Room* m_room = nullptr;
    CollisionType m_collisionType = CollisionType::None;
    CollisionShape m_collisionShape = CollisionShape::Box;
};

class BulletObject : public Object
{
public:
    explicit BulletObject(Room* room)
        : Object(0, room)
    {
    }
};
} // namespace world
