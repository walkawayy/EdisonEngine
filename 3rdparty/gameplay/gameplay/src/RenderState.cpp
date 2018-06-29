#include "Base.h"
#include "RenderState.h"
#include "Node.h"

namespace gameplay
{
RenderState RenderState::m_currentState;

void RenderState::bind(bool force)
{
    // Update any state that differs from m_currentState
    if( force || (m_blendEnabled.isInitialized() && m_blendEnabled != m_currentState.m_blendEnabled) )
    {
        if( m_blendEnabled.get() )
        {
            GL_ASSERT( glEnable( GL_BLEND ) );
        }
        else
        {
            GL_ASSERT( glDisable( GL_BLEND ) );
        }
        m_currentState.m_blendEnabled = m_blendEnabled;
    }
    if( force || ((m_blendSrc.isInitialized() || m_blendDst.isInitialized())
                  && (m_blendSrc != m_currentState.m_blendSrc || m_blendDst != m_currentState.m_blendDst)) )
    {
        GL_ASSERT( glBlendFunc( m_blendSrc.get(), m_blendDst.get() ) );
        m_currentState.m_blendSrc = m_blendSrc;
        m_currentState.m_blendDst = m_blendDst;
    }
    if( force || (m_cullFaceEnabled.isInitialized() && m_cullFaceEnabled != m_currentState.m_cullFaceEnabled) )
    {
        if( m_cullFaceEnabled.get() )
        {
            GL_ASSERT( glEnable( GL_CULL_FACE ) );
        }
        else
        {
            GL_ASSERT( glDisable( GL_CULL_FACE ) );
        }
        m_currentState.m_cullFaceEnabled = m_cullFaceEnabled;
    }
    if( force || (m_cullFaceSide.isInitialized() && (m_cullFaceSide != m_currentState.m_cullFaceSide)) )
    {
        GL_ASSERT( glCullFace( m_cullFaceSide.get() ) );
        m_currentState.m_cullFaceSide = m_cullFaceSide;
    }
    if( force || (m_frontFace.isInitialized() && m_frontFace != m_currentState.m_frontFace) )
    {
        GL_ASSERT( glFrontFace( m_frontFace.get() ) );
        m_currentState.m_frontFace = m_frontFace;
    }
    if( force || (m_depthTestEnabled.isInitialized() && (m_depthTestEnabled != m_currentState.m_depthTestEnabled)) )
    {
        if( m_depthTestEnabled.get() )
        {
            GL_ASSERT( glEnable( GL_DEPTH_TEST ) );
        }
        else
        {
            GL_ASSERT( glDisable( GL_DEPTH_TEST ) );
        }
        m_currentState.m_depthTestEnabled = m_depthTestEnabled;
    }
    if( force || (m_depthWriteEnabled.isInitialized() && (m_depthWriteEnabled != m_currentState.m_depthWriteEnabled)) )
    {
        GL_ASSERT( glDepthMask( m_depthWriteEnabled.get() ? GL_TRUE : GL_FALSE ) );
        m_currentState.m_depthWriteEnabled = m_depthWriteEnabled;
    }
    if( force || (m_depthFunction.isInitialized() && (m_depthFunction != m_currentState.m_depthFunction)) )
    {
        GL_ASSERT( glDepthFunc( m_depthFunction.get() ) );
        m_currentState.m_depthFunction = m_depthFunction;
    }
}

void RenderState::enableDepthWrite()
{
    // Internal method used by Game::clear() to restore depth writing before a
    // clear operation. This is necessary if the last code to draw before the
    // next frame leaves depth writing disabled.
    GL_ASSERT( glDepthMask( GL_TRUE ) );
    m_currentState.m_depthWriteEnabled = true;
    GL_ASSERT( glEnable( GL_DEPTH_TEST ) );
    m_currentState.m_depthTestEnabled = true;
}

void RenderState::setBlend(bool enabled)
{
    m_blendEnabled = enabled;
}

void RenderState::setBlendSrc(GLenum blend)
{
    m_blendSrc = blend;
}

void RenderState::setBlendDst(GLenum blend)
{
    m_blendDst = blend;
}

void RenderState::setCullFace(bool enabled)
{
    m_cullFaceEnabled = enabled;
}

void RenderState::setCullFaceSide(GLenum side)
{
    m_cullFaceSide = side;
}

void RenderState::setFrontFace(GLenum winding)
{
    m_frontFace = winding;
}

void RenderState::setDepthTest(bool enabled)
{
    m_depthTestEnabled = enabled;
}

void RenderState::setDepthWrite(bool enabled)
{
    m_depthWriteEnabled = enabled;
}

void RenderState::setDepthFunction(GLenum func)
{
    m_depthFunction = func;
}
}
