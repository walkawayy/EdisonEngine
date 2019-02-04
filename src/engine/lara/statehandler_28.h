#pragma once

#include "abstractstatehandler.h"
#include "engine/collisioninfo.h"

namespace engine
{
namespace lara
{
class StateHandler_28 final
        : public AbstractStateHandler
{
public:
    explicit StateHandler_28(LaraNode& lara)
            : AbstractStateHandler{lara, LaraStateId::JumpUp}
    {
    }

    void handleInput(CollisionInfo& /*collisionInfo*/) override
    {
        if( getLara().m_state.fallspeed > core::FreeFallSpeedThreshold )
        {
            setGoalAnimState( LaraStateId::FreeFall );
        }
    }

    void postprocessFrame(CollisionInfo& collisionInfo) override
    {
        collisionInfo.badPositiveDistance = core::HeightLimit;
        collisionInfo.badNegativeDistance = -core::ClimbLimit2ClickMin;
        collisionInfo.badCeilingDistance = 192_len;
        collisionInfo.facingAngle = getLara().m_state.rotation.Y;
        setMovementAngle( collisionInfo.facingAngle );
        collisionInfo.initHeightInfo( getLara().m_state.position.position, getLevel(), 870_len ); //! @todo MAGICK 870

        if( tryGrabEdge( collisionInfo ) )
        {
            return;
        }

        jumpAgainstWall( collisionInfo );
        if( getLara().m_state.fallspeed <= 0_len || collisionInfo.mid.floor.y > 0_len )
        {
            return;
        }

        if( applyLandingDamage() )
        {
            setGoalAnimState( LaraStateId::Death );
        }
        else
        {
            setGoalAnimState( LaraStateId::Stop );
        }
        getLara().m_state.fallspeed = 0_len;
        placeOnFloor( collisionInfo );
        getLara().m_state.falling = false;
    }
};
}
}
