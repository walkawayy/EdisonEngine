#include "progressbar.h"

#include "engine/system.h"
#include "gui.h"
#include "world/character.h"

namespace gui
{
ProgressBar::ProgressBar(engine::Engine* engine)
    : m_engine(engine)
{
    // Initialize parameters.
    // By default, bar is initialized with TR5-like health bar properties.
    setPosition(HorizontalAnchor::Left, 20, VerticalAnchor::Top, 20);
    setSize(250, 25, 3);
    setColor(BarColorType::BaseMain, 255, 50, 50, 150);
    setColor(BarColorType::BaseFade, 100, 255, 50, 150);
    setColor(BarColorType::AltMain, 255, 180, 0, 220);
    setColor(BarColorType::AltFade, 255, 255, 0, 220);
    setColor(BarColorType::BackMain, 0, 0, 0, 160);
    setColor(BarColorType::BackFade, 60, 60, 60, 130);
    setColor(BarColorType::BorderMain, 200, 200, 200, 50);
    setColor(BarColorType::BorderFade, 80, 80, 80, 100);
    setValues(1000, 300);
    setBlink(util::MilliSeconds(300));
    setExtrude(true, 100);
    setAutoshow(true, util::MilliSeconds(5000), true, util::MilliSeconds(1000));
}

// Resize bar.
// This function should be called every time resize event occurs.

void ProgressBar::resize()
{
    recalculateSize();
    recalculatePosition();
}

// Set specified color.
void ProgressBar::setColor(BarColorType colType,
                           uint8_t R, uint8_t G, uint8_t B, uint8_t A)
{
    switch(colType)
    {
        case BarColorType::BaseMain:
            m_baseMainColor.set(A,R,G,B);
            m_baseMainColorAlpha = A;
            break;
        case BarColorType::BaseFade:
            m_baseFadeColor.set(A,R,G,B);
            m_baseFadeColorAlpha = A;
            break;
        case BarColorType::AltMain:
            m_altMainColor.set(A,R,G,B);
            m_altMainColorAlpha = A;
            break;
        case BarColorType::AltFade:
            m_altFadeColor.set(A,R,G,B);
            m_altFadeColorAlpha = A;
            break;
        case BarColorType::BackMain:
            m_backMainColor.set(A,R,G,B);
            m_backMainColorAlpha = A;
            break;
        case BarColorType::BackFade:
            m_backFadeColor.set(A,R,G,B);
            m_backFadeColorAlpha = A;
            break;
        case BarColorType::BorderMain:
            m_borderMainColor.set(A,R,G,B);
            m_borderMainColorAlpha = A;
            break;
        case BarColorType::BorderFade:
            m_borderFadeColor.set(A,R,G,B);
            m_borderFadeColorAlpha = A;
            break;
        default:
            break;
    }
}

void ProgressBar::setPosition(HorizontalAnchor anchor_X, irr::f32 offset_X, VerticalAnchor anchor_Y, irr::f32 offset_Y)
{
    m_xAnchor = anchor_X;
    m_yAnchor = anchor_Y;
    m_absXoffset = offset_X;
    m_absYoffset = offset_Y;

    recalculatePosition();
}

// Set bar size
void ProgressBar::setSize(irr::f32 width, irr::f32 height, irr::f32 borderSize)
{
    // Absolute values are needed to recalculate actual bar size according to resolution.
    m_absWidth = width;
    m_absHeight = height;
    m_absBorderSize = borderSize;

    recalculateSize();
}

// Recalculate size, according to viewport resolution.
void ProgressBar::recalculateSize()
{
    m_width = static_cast<float>(m_absWidth)  * m_engine->m_screenInfo.scale_factor;
    m_height = static_cast<float>(m_absHeight) * m_engine->m_screenInfo.scale_factor;

    m_borderWidth = static_cast<float>(m_absBorderSize)  * m_engine->m_screenInfo.scale_factor;
    m_borderHeight = static_cast<float>(m_absBorderSize)  * m_engine->m_screenInfo.scale_factor;

    // Calculate range unit, according to maximum bar value set up.
    // If bar alignment is set to horizontal, calculate it from bar width.
    // If bar is vertical, then calculate it from height.

    m_rangeUnit = !m_vertical ? m_width / m_maxValue : m_height / m_maxValue;
}

// Recalculate position, according to viewport resolution.
void ProgressBar::recalculatePosition()
{
    switch(m_xAnchor)
    {
        case HorizontalAnchor::Left:
            m_x = static_cast<float>(m_absXoffset + m_absBorderSize) * m_engine->m_screenInfo.scale_factor;
            break;
        case HorizontalAnchor::Center:
            m_x = (static_cast<float>(m_engine->m_screenInfo.w) - static_cast<float>(m_absWidth + m_absBorderSize * 2) * m_engine->m_screenInfo.scale_factor) / 2 +
                static_cast<float>(m_absXoffset) * m_engine->m_screenInfo.scale_factor;
            break;
        case HorizontalAnchor::Right:
            m_x = static_cast<float>(m_engine->m_screenInfo.w) - static_cast<float>(m_absXoffset + m_absWidth + m_absBorderSize * 2) * m_engine->m_screenInfo.scale_factor;
            break;
    }

    switch(m_yAnchor)
    {
        case VerticalAnchor::Top:
            m_y = static_cast<float>(m_engine->m_screenInfo.h) - static_cast<float>(m_absYoffset + m_absHeight + m_absBorderSize * 2) * m_engine->m_screenInfo.scale_factor;
            break;
        case VerticalAnchor::Center:
            m_y = (static_cast<float>(m_engine->m_screenInfo.h) - static_cast<float>(m_absHeight + m_absBorderSize * 2) * m_engine->m_screenInfo.h_unit) / 2 +
                static_cast<float>(m_absYoffset) * m_engine->m_screenInfo.scale_factor;
            break;
        case VerticalAnchor::Bottom:
            m_y = (m_absYoffset + m_absBorderSize) * m_engine->m_screenInfo.scale_factor;
            break;
    }
}

// Set maximum and warning state values.
void ProgressBar::setValues(irr::f32 maxValue, irr::f32 warnValue)
{
    m_maxValue = maxValue;
    m_warnValue = warnValue;

    recalculateSize();  // We need to recalculate size, because max. value is changed.
}

// Set warning state blinking interval.
void ProgressBar::setBlink(util::Duration interval)
{
    m_blinkInterval = interval;
    m_blinkCnt = interval;  // Also reset blink counter.
}

// Set extrude overlay effect parameters.
void ProgressBar::setExtrude(bool enabled, uint8_t depth)
{
    m_extrude = enabled;
    m_extrudeDepth.set(depth, 0, 0, 0);    // Set all colors to 0.
    m_extrudeDepthAlpha = depth;
}

// Set autoshow and fade parameters.
// Please note that fade parameters are actually independent of autoshow.
void ProgressBar::setAutoshow(bool enabled, util::Duration delay, bool fade, util::Duration fadeDelay)
{
    m_autoShow = enabled;

    m_autoShowDelay = delay;
    m_autoShowCnt = delay;     // Also reset autoshow counter.

    m_autoShowFade = fade;
    m_autoShowFadeDelay = fadeDelay;
    m_autoShowFadeLength = util::Duration(0); // Initially, it's 0.
}

// Main bar show procedure.
// Draws a bar with a given value. Please note that it also accepts float,
// so effectively you can create bars for floating-point parameters.
void ProgressBar::show(irr::f32 value)
{
    // Initial value limiters (to prevent bar overflow).
    value = irr::core::clamp(value, 0.0f, m_maxValue);

    // Enable blink mode, if value is gone below warning value.
    m_blink = value <= m_warnValue;

    if(m_autoShow)   // Check autoshow visibility conditions.
    {
        // 0. If bar drawing was forced, then show a bar without additional
        //    autoshow delay set. This condition has to be overwritten by
        //    any other conditions, that's why it is set first.
        if(m_forced)
        {
            m_visible = true;
            m_forced = false;
        }
        else
        {
            m_visible = false;
        }

        // 1. If bar value gone less than warning value, we show it
        //    in any case, bypassing all other conditions.
        if(value <= m_warnValue)
            m_visible = true;

        // 2. Check if bar's value changed,
        //    and if so, start showing it automatically for a given delay time.
        if(m_lastValue != value)
        {
            m_lastValue = value;
            m_visible = true;
            m_autoShowCnt = m_autoShowDelay;
        }

        // 3. If autoshow time is up, then we hide bar,
        //    otherwise decrease delay counter.
        if(m_autoShowCnt.count() > 0)
        {
            m_visible = true;
            m_autoShowCnt -= m_engine->getFrameTime();

            if(m_autoShowCnt.count() <= 0)
            {
                m_autoShowCnt = util::Duration(0);
                m_visible = false;
            }
        }
    } // end if(AutoShow)

    if(m_autoShowFade)   // Process fade-in and fade-out effect, if enabled.
    {
        if(!m_visible)
        {
            // If visibility flag is off and bar is still on-screen, gradually decrease
            // fade counter, else simply don't draw anything and exit.
            if(m_autoShowFadeLength.count() == 0)
            {
                return;
            }
            else
            {
                m_autoShowFadeLength -= m_engine->getFrameTime();
                if(m_autoShowFadeLength.count() < 0)
                    m_autoShowFadeLength = util::Duration(0);
            }
        }
        else
        {
            // If visibility flag is on, and bar is not yet fully visible, gradually
            // increase fade counter, until it's 1 (i. e. fully opaque).
            if(m_autoShowFadeLength < m_autoShowFadeDelay)
            {
                m_autoShowFadeLength += m_engine->getFrameTime();
                if(m_autoShowFadeLength > m_autoShowFadeDelay)
                    m_autoShowFadeLength = m_autoShowFadeDelay;
            }
        } // end if(!Visible)

        // Multiply all layers' alpha by current fade counter.
        m_baseMainColor.setAlpha( m_baseMainColorAlpha * m_autoShowFadeLength / m_autoShowFadeDelay );
        m_baseFadeColor.setAlpha( m_baseFadeColorAlpha * m_autoShowFadeLength / m_autoShowFadeDelay );
        m_altMainColor.setAlpha( m_altMainColorAlpha * m_autoShowFadeLength / m_autoShowFadeDelay );
        m_altFadeColor.setAlpha( m_altFadeColorAlpha * m_autoShowFadeLength / m_autoShowFadeDelay );
        m_backMainColor.setAlpha( m_backMainColorAlpha * m_autoShowFadeLength / m_autoShowFadeDelay );
        m_backFadeColor.setAlpha( m_backFadeColorAlpha * m_autoShowFadeLength / m_autoShowFadeDelay );
        m_borderMainColor.setAlpha( m_borderMainColorAlpha * m_autoShowFadeLength / m_autoShowFadeDelay );
        m_borderFadeColor.setAlpha( m_borderFadeColorAlpha * m_autoShowFadeLength / m_autoShowFadeDelay );
        m_extrudeDepth.setAlpha( m_extrudeDepthAlpha * m_autoShowFadeLength / m_autoShowFadeDelay );
    }
    else
    {
        if(!m_visible)
            return;   // Obviously, quit, if bar is not visible.
    } // end if(mAutoShowFade)

    // Draw border rect.
    // Border rect should be rendered first, as it lies beneath actual bar,
    // and additionally, we need to show it in any case, even if bar is in
    // warning state (blinking).
    m_engine->m_gui.drawRect(m_x, m_y, m_width + m_borderWidth * 2, m_height + m_borderHeight * 2,
                            m_borderMainColor, m_borderMainColor,
                            m_borderFadeColor, m_borderFadeColor,
                            loader::BlendingMode::Solid);

    // SECTION FOR BASE BAR RECTANGLE.

    // We check if bar is in a warning state. If it is, we blink it continously.
    if(m_blink)
    {
        m_blinkCnt -= m_engine->getFrameTime();
        if(m_blinkCnt > m_blinkInterval)
        {
            value = 0; // Force zero value, which results in empty bar.
        }
        else if(m_blinkCnt.count() <= 0)
        {
            m_blinkCnt = m_blinkInterval * 2;
        }
    }

    // If bar value is zero, just render background overlay and immediately exit.
    // It is needed in case bar is used as a simple UI box to bypass unnecessary calculations.
    if(!value)
    {
        // Draw full-sized background rect (instead of base bar rect)
        m_engine->m_gui.drawRect(m_x + m_borderWidth, m_y + m_borderHeight, m_width, m_height,
                                m_backMainColor, m_vertical ? m_backFadeColor : m_backMainColor,
                                m_vertical ? m_backMainColor : m_backFadeColor, m_backFadeColor,
                                loader::BlendingMode::Solid);
        return;
    }

    // Calculate base bar width, according to current value and range unit.
    m_baseSize = m_rangeUnit * value;
    m_baseRatio = value / m_maxValue;

    irr::f32 rectAnchor;           // Anchor to stick base bar rect, according to Invert flag.
    irr::video::SColor rectFirstColor;    // Used to recalculate gradient, according to current value.
    irr::video::SColor rectSecondColor;

    // If invert decrease direction style flag is set, we position bar in a way
    // that it seems like it's decreasing to another side, and also swap main / fade colours.
    if(m_invert)
    {
        rectFirstColor = m_alternate ? m_altMainColor : m_baseMainColor;

        // Main-fade gradient is recalculated according to current / maximum value ratio.
        rectSecondColor = m_alternate
            ? m_altFadeColor.getInterpolated(m_altMainColor, m_baseRatio)
            : m_baseFadeColor.getInterpolated(m_baseMainColor, m_baseRatio);
    }
    else
    {
        rectSecondColor = m_alternate ? m_altMainColor : m_baseMainColor;

        // Main-fade gradient is recalculated according to current / maximum value ratio.
          rectSecondColor = m_alternate
              ? m_altFadeColor.getInterpolated(m_altMainColor, m_baseRatio)
              : m_baseFadeColor.getInterpolated(m_baseMainColor, m_baseRatio);
    } // end if(Invert)

    // We need to reset Alternate flag each frame, cause behaviour is immediate.

    m_alternate = false;

    // If vertical style flag is set, we draw bar base top-bottom, else we draw it left-right.
    if(m_vertical)
    {
        rectAnchor = (m_invert ? m_y + m_height - m_baseSize : m_y) + m_borderHeight;

        // Draw actual bar base.
        m_engine->m_gui.drawRect(m_x + m_borderWidth, rectAnchor,
                                m_width, m_baseSize,
                                rectFirstColor, rectFirstColor,
                                rectSecondColor, rectSecondColor,
                                loader::BlendingMode::Solid);

        // Draw background rect.
        m_engine->m_gui.drawRect(m_x + m_borderWidth,
                                m_invert ? m_y + m_borderHeight : rectAnchor + m_baseSize,
                                m_width, m_height - m_baseSize,
                                m_backMainColor, m_backFadeColor,
                                m_backMainColor, m_backFadeColor,
                                loader::BlendingMode::Solid);

        if(m_extrude)    // Draw extrude overlay, if flag is set.
        {
            irr::video::SColor transparentColor{ 0, 0, 0, 0 };  // Used to set counter-shade to transparent.

            m_engine->m_gui.drawRect(m_x + m_borderWidth, rectAnchor,
                                    m_width / 2, m_baseSize,
                                    m_extrudeDepth, transparentColor,
                                    m_extrudeDepth, transparentColor,
                                    loader::BlendingMode::Solid);
            m_engine->m_gui.drawRect(m_x + m_borderWidth + m_width / 2, rectAnchor,
                                    m_width / 2, m_baseSize,
                                    transparentColor, m_extrudeDepth,
                                    transparentColor, m_extrudeDepth,
                                    loader::BlendingMode::Solid);
        }
    }
    else
    {
        rectAnchor = (m_invert ? m_x + m_width - m_baseSize : m_x) + m_borderWidth;

        // Draw actual bar base.
        m_engine->m_gui.drawRect(rectAnchor, m_y + m_borderHeight,
                                m_baseSize, m_height,
                                rectSecondColor, rectFirstColor,
                                rectSecondColor, rectFirstColor,
                                loader::BlendingMode::Solid);

        // Draw background rect.
        m_engine->m_gui.drawRect(m_invert ? m_x + m_borderWidth : rectAnchor + m_baseSize,
                                m_y + m_borderHeight,
                                m_width - m_baseSize, m_height,
                                m_backMainColor, m_backMainColor,
                                m_backFadeColor, m_backFadeColor,
                                loader::BlendingMode::Solid);

        if(m_extrude)    // Draw extrude overlay, if flag is set.
        {
            irr::video::SColor transparentColor{ 0, 0, 0, 0 };  // Used to set counter-shade to transparent.

            m_engine->m_gui.drawRect(rectAnchor, m_y + m_borderHeight,
                                    m_baseSize, m_height / 2,
                                    transparentColor, transparentColor,
                                    m_extrudeDepth, m_extrudeDepth,
                                    loader::BlendingMode::Solid);
            m_engine->m_gui.drawRect(rectAnchor, m_y + m_borderHeight + m_height / 2,
                                    m_baseSize, m_height / 2,
                                    m_extrudeDepth, m_extrudeDepth,
                                    transparentColor, transparentColor,
                                    loader::BlendingMode::Solid);
        }
    } // end if(Vertical)
}
} // namespace gui
