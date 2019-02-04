#pragma once

#include "abstractstatehandler.h"
#include "engine/collisioninfo.h"
#include "engine/inputstate.h"
#include "level/level.h"
#include "engine/laranode.h"

namespace engine
{
namespace lara
{
class StateHandler_1 final
        : public AbstractStateHandler
{
public:
    explicit StateHandler_1(LaraNode& lara)
            : AbstractStateHandler{lara, LaraStateId::RunForward}
    {
    }

    void handleInput(CollisionInfo& /*collisionInfo*/) override
    {
        if( getLara().m_state.health <= 0_hp )
        {
            setGoalAnimState( LaraStateId::Death );
            return;
        }

        if( getLevel().m_inputHandler->getInputState().roll )
        {
            setAnimation( loader::AnimationId::ROLL_BEGIN, 3857_frame );
            setGoalAnimState( LaraStateId::Stop );
            return;
        }

        if( getLevel().m_inputHandler->getInputState().xMovement == AxisMovement::Left )
        {
            subYRotationSpeed( 2.25_deg, -8_deg );
            const core::Angle z = std::max( -11_deg, getLara().m_state.rotation.Z - 1.5_deg );
            getLara().m_state.rotation.Z = z;
        }
        else if( getLevel().m_inputHandler->getInputState().xMovement == AxisMovement::Right )
        {
            addYRotationSpeed( 2.25_deg, 8_deg );
            const core::Angle z = std::min( +11_deg, getLara().m_state.rotation.Z + 1.5_deg );
            getLara().m_state.rotation.Z = z;
        }

        if( getLevel().m_inputHandler->getInputState().jump && !getLara().m_state.falling )
        {
            setGoalAnimState( LaraStateId::JumpForward );
            return;
        }

        if( getLevel().m_inputHandler->getInputState().zMovement != AxisMovement::Forward )
        {
            setGoalAnimState( LaraStateId::Stop );
            return;
        }

        if( getLevel().m_inputHandler->getInputState().moveSlow )
        {
            setGoalAnimState( LaraStateId::WalkForward );
        }
        else
        {
            setGoalAnimState( LaraStateId::RunForward );
        }
    }

    void postprocessFrame(CollisionInfo& collisionInfo) override
    {
        collisionInfo.facingAngle = getLara().m_state.rotation.Y;
        setMovementAngle( collisionInfo.facingAngle );
        collisionInfo.badPositiveDistance = core::HeightLimit;
        collisionInfo.badNegativeDistance = -core::ClimbLimit2ClickMin;
        collisionInfo.badCeilingDistance = 0_len;
        collisionInfo.policyFlags.set(CollisionInfo::PolicyFlags::SlopesAreWalls);
        collisionInfo.initHeightInfo( getLara().m_state.position.position, getLevel(), core::ScalpHeight );

        if( stopIfCeilingBlocked( collisionInfo ) )
        {
            return;
        }

        if( tryClimb( collisionInfo ) )
        {
            return;
        }

        if( checkWallCollision( collisionInfo ) )
        {
            getLara().m_state.rotation.Z = 0_deg;
            if( collisionInfo.front.floor.slantClass == SlantClass::None
                && collisionInfo.front.floor.y < -core::ClimbLimit2ClickMax )
            {
                if( getLara().m_state.frame_number < 10_frame )
                {
                    setAnimation( loader::AnimationId::WALL_SMASH_LEFT, 800_frame );
                    return;
                }
                if( getLara().m_state.frame_number >= 10_frame && getLara().m_state.frame_number < 22_frame )
                {
                    setAnimation( loader::AnimationId::WALL_SMASH_RIGHT, 815_frame );
                    return;
                }
            }

            setAnimation( loader::AnimationId::STAY_SOLID, 185_frame );
        }

        if( collisionInfo.mid.floor.y > core::ClimbLimit2ClickMin )
        {
            setAnimation( loader::AnimationId::FREE_FALL_FORWARD, 492_frame );
            setGoalAnimState( LaraStateId::JumpForward );
            getLara().m_state.falling = true;
            getLara().m_state.fallspeed = 0_len;
            return;
        }

        if( collisionInfo.mid.floor.y >= -core::ClimbLimit2ClickMin
            && collisionInfo.mid.floor.y < -core::SteppableHeight )
        {
            if( getLara().m_state.frame_number >= 3_frame && getLara().m_state.frame_number <= 14_frame )
            {
                setAnimation( loader::AnimationId::RUN_UP_STEP_LEFT, 837_frame );
            }
            else
            {
                setAnimation( loader::AnimationId::RUN_UP_STEP_RIGHT, 830_frame );
            }
        }

        if( !tryStartSlide( collisionInfo ) )
        {
            getLara().m_state.position.position.Y += std::min( collisionInfo.mid.floor.y, 50_len );
        }
    }
};
}
}
