/*
 * Copyright 2009, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RenderThemeAndroid.h"

#include "Color.h"
#include "Element.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "HTMLSelectElement.h"
#include "Node.h"
#include "PlatformGraphicsContext.h"
#if ENABLE(VIDEO)
#include "RenderMediaControls.h"
#endif
#include "RenderObject.h"
#include "RenderSkinAndroid.h"
#include "RenderSkinMediaButton.h"
#include "RenderSlider.h"
#include "RoundedIntRect.h"
#include "SkCanvas.h"
#include "UserAgentStyleSheets.h"
#include "WebCoreFrameBridge.h"

namespace WebCore {

// Add padding to the fontSize of ListBoxes to get their maximum sizes.
// Listboxes often have a specified size.  Since we change them into
// dropdowns, we want a much smaller height, which encompasses the text.
const int listboxPadding = 5;

// This is the color of selection in a textfield.  It was computed from
// frameworks/base/core/res/res/values/colors.xml, which uses #9983CC39
// (decimal a = 153, r = 131, g = 204, b = 57)
// for all four highlighted text values. Blending this with white yields:
// R = (131 * 153 + 255 * (255 - 153)) / 255  -> 180.6
// G = (204 * 153 + 255 * (255 - 153)) / 255  -> 224.4
// B = ( 57 * 153 + 255 * (255 - 153)) / 255  -> 136.2

const RGBA32 selectionColor = makeRGB(181, 224, 136);

// Colors copied from the holo resources
const RGBA32 defaultBgColor = makeRGBA(204, 204, 204, 197);
const RGBA32 defaultBgBright = makeRGBA(213, 213, 213, 221);
const RGBA32 defaultBgDark = makeRGBA(92, 92, 92, 160);
const RGBA32 defaultBgMedium = makeRGBA(132, 132, 132, 111);
const RGBA32 defaultFgColor = makeRGBA(101, 101, 101, 225);
const RGBA32 defaultCheckColor = makeRGBA(0, 153, 204, 255);
const RGBA32 defaultCheckColorShadow = makeRGBA(29, 123, 154, 192);

const RGBA32 disabledBgColor = makeRGBA(205, 205, 205, 107);
const RGBA32 disabledBgBright = makeRGBA(213, 213, 213, 133);
const RGBA32 disabledBgDark = makeRGBA(92, 92, 92, 96);
const RGBA32 disabledBgMedium = makeRGBA(132, 132, 132, 111);
const RGBA32 disabledFgColor = makeRGBA(61, 61, 61, 68);
const RGBA32 disabledCheckColor = makeRGBA(61, 61, 61, 128);
const RGBA32 disabledCheckColorShadow = disabledCheckColor;

const int paddingButton = 2;
const int cornerButton = 2;

// scale factors for various resolutions
const float scaleFactor[RenderSkinAndroid::ResolutionCount] = {
    1.0f, // medium res
    1.5f, // high res
    2.0f  // extra high res
};

static android::WebFrame* getWebFrame(const Node* node)
{
    if (!node)
        return 0;
    return android::WebFrame::getWebFrame(node->document()->frame());
}

// Draws a nice, mitered line.
// This is a partial copy from RenderObject::drawLineForBoxSide
static void drawLineForBoxSide(GraphicsContext* graphicsContext, int x1, int y1,
                               int x2, int y2, BoxSide side, Color color,
                               int adjacentWidth1, int adjacentWidth2)
{
    static const bool antialias = false;
    graphicsContext->setFillColor(color, graphicsContext->fillColorSpace());
    if (!adjacentWidth1 && !adjacentWidth2) {
        // Turn off antialiasing to match the behavior of drawConvexPolygon();
        // this matters for rects in transformed contexts.
        bool wasAntialiased = graphicsContext->shouldAntialias();
        graphicsContext->setShouldAntialias(antialias);
        graphicsContext->drawRect(IntRect(x1, y1, x2 - x1, y2 - y1));
        graphicsContext->setShouldAntialias(wasAntialiased);
        return;
    }
    FloatPoint quad[4];
    switch (side) {
        case BSTop:
            quad[0] = FloatPoint(x1 + max(-adjacentWidth1, 0), y1);
            quad[1] = FloatPoint(x1 + max(adjacentWidth1, 0), y2);
            quad[2] = FloatPoint(x2 - max(adjacentWidth2, 0), y2);
            quad[3] = FloatPoint(x2 - max(-adjacentWidth2, 0), y1);
            break;
        case BSBottom:
            quad[0] = FloatPoint(x1 + max(adjacentWidth1, 0), y1);
            quad[1] = FloatPoint(x1 + max(-adjacentWidth1, 0), y2);
            quad[2] = FloatPoint(x2 - max(-adjacentWidth2, 0), y2);
            quad[3] = FloatPoint(x2 - max(adjacentWidth2, 0), y1);
            break;
        case BSLeft:
            quad[0] = FloatPoint(x1, y1 + max(-adjacentWidth1, 0));
            quad[1] = FloatPoint(x1, y2 - max(-adjacentWidth2, 0));
            quad[2] = FloatPoint(x2, y2 - max(adjacentWidth2, 0));
            quad[3] = FloatPoint(x2, y1 + max(adjacentWidth1, 0));
            break;
        case BSRight:
            quad[0] = FloatPoint(x1, y1 + max(adjacentWidth1, 0));
            quad[1] = FloatPoint(x1, y2 - max(adjacentWidth2, 0));
            quad[2] = FloatPoint(x2, y2 - max(-adjacentWidth2, 0));
            quad[3] = FloatPoint(x2, y1 + max(-adjacentWidth1, 0));
            break;
    }

    graphicsContext->drawConvexPolygon(4, quad, antialias);
}

RenderTheme* theme()
{
    DEFINE_STATIC_LOCAL(RenderThemeAndroid, androidTheme, ());
    return &androidTheme;
}

PassRefPtr<RenderTheme> RenderTheme::themeForPage(Page* page)
{
    static RenderTheme* rt = RenderThemeAndroid::create().releaseRef();
    return rt;
}

PassRefPtr<RenderTheme> RenderThemeAndroid::create()
{
    return adoptRef(new RenderThemeAndroid());
}

RenderThemeAndroid::RenderThemeAndroid()
{
}

RenderThemeAndroid::~RenderThemeAndroid()
{
}

void RenderThemeAndroid::close()
{
}

bool RenderThemeAndroid::stateChanged(RenderObject* obj, ControlState state) const
{
    if (CheckedState == state) {
        obj->repaint();
        return true;
    }
    return false;
}

Color RenderThemeAndroid::platformActiveSelectionBackgroundColor() const
{
    return Color(selectionColor);
}

Color RenderThemeAndroid::platformInactiveSelectionBackgroundColor() const
{
    return Color(Color::transparent);
}

Color RenderThemeAndroid::platformActiveSelectionForegroundColor() const
{
    return Color::black;
}

Color RenderThemeAndroid::platformInactiveSelectionForegroundColor() const
{
    return Color::black;
}

Color RenderThemeAndroid::platformTextSearchHighlightColor() const
{
    return Color(Color::transparent);
}

Color RenderThemeAndroid::platformActiveListBoxSelectionBackgroundColor() const
{
    return Color(Color::transparent);
}

Color RenderThemeAndroid::platformInactiveListBoxSelectionBackgroundColor() const
{
    return Color(Color::transparent);
}

Color RenderThemeAndroid::platformActiveListBoxSelectionForegroundColor() const
{
    return Color(Color::transparent);
}

Color RenderThemeAndroid::platformInactiveListBoxSelectionForegroundColor() const
{
    return Color(Color::transparent);
}

Color RenderThemeAndroid::platformActiveTextSearchHighlightColor() const
{
    return Color(0x00, 0x99, 0xcc, 0x99); // HOLO_DARK
}

Color RenderThemeAndroid::platformInactiveTextSearchHighlightColor() const
{
    return Color(0x33, 0xb5, 0xe5, 0x66); // HOLO_LIGHT
}

int RenderThemeAndroid::baselinePosition(const RenderObject* obj) const
{
    // From the description of this function in RenderTheme.h:
    // A method to obtain the baseline position for a "leaf" control.  This will only be used if a baseline
    // position cannot be determined by examining child content. Checkboxes and radio buttons are examples of
    // controls that need to do this.
    //
    // Our checkboxes and radio buttons need to be offset to line up properly.
    return RenderTheme::baselinePosition(obj) - 6;
}

void RenderThemeAndroid::addIntrinsicMargins(RenderStyle* style) const
{
    // Cut out the intrinsic margins completely if we end up using a small font size
    if (style->fontSize() < 11)
        return;

    // Intrinsic margin value.
    const int m = 2;

    // FIXME: Using width/height alone and not also dealing with min-width/max-width is flawed.
    if (style->width().isIntrinsicOrAuto()) {
        if (style->marginLeft().quirk())
            style->setMarginLeft(Length(m, Fixed));
        if (style->marginRight().quirk())
            style->setMarginRight(Length(m, Fixed));
    }

    if (style->height().isAuto()) {
        if (style->marginTop().quirk())
            style->setMarginTop(Length(m, Fixed));
        if (style->marginBottom().quirk())
            style->setMarginBottom(Length(m, Fixed));
    }
}

bool RenderThemeAndroid::supportsFocus(ControlPart appearance)
{
    switch (appearance) {
    case PushButtonPart:
    case ButtonPart:
    case TextFieldPart:
        return true;
    default:
        return false;
    }

    return false;
}

void RenderThemeAndroid::adjustButtonStyle(CSSStyleSelector*, RenderStyle* style, WebCore::Element*) const
{
}

bool RenderThemeAndroid::paintCheckbox(RenderObject* obj, const PaintInfo& info, const IntRect& rect)
{
    paintRadio(obj, info, rect);
    return false;
}

bool RenderThemeAndroid::paintButton(RenderObject* obj, const PaintInfo& info, const IntRect& rect)
{
    // If it is a disabled button, simply paint it to the master picture.
    Node* node = obj->node();
    Element* formControlElement = static_cast<Element*>(node);
    if (formControlElement) {
        android::WebFrame* webFrame = getWebFrame(node);
        if (webFrame) {
            GraphicsContext *context = info.context;
            IntRect innerrect = IntRect(rect.x() + paddingButton, rect.y() + paddingButton,
                    rect.width() - 2 * paddingButton, rect.height() - 2 * paddingButton);
            IntSize cornerrect = IntSize(cornerButton, cornerButton);
            Color bg, bright, dark, medium;
            if (formControlElement->isEnabledFormControl()) {
                bg = Color(defaultBgColor);
                bright = Color(defaultBgBright);
                dark = Color(defaultBgDark);
                medium = Color(defaultBgMedium);
            } else {
                bg = Color(disabledBgColor);
                bright = Color(disabledBgBright);
                dark = Color(disabledBgDark);
                medium = Color(disabledBgMedium);
            }
            context->save();
            RoundedIntRect border(rect, cornerrect, cornerrect, cornerrect, cornerrect);
            context->addRoundedRectClip(border);
            context->setStrokeStyle(NoStroke);
            drawLineForBoxSide(context, rect.x(), rect.y(), rect.maxX(), innerrect.y(),
                               BSTop, bright, paddingButton, paddingButton);
            drawLineForBoxSide(context, rect.x(), rect.y(), innerrect.x(), rect.maxY(),
                               BSLeft, medium, paddingButton, paddingButton);
            drawLineForBoxSide(context, innerrect.maxX(), rect.y(), rect.maxX(), rect.maxY(),
                               BSRight, medium, paddingButton, paddingButton);
            drawLineForBoxSide(context, rect.x(), innerrect.maxY(), rect.maxX(), rect.maxY(),
                               BSBottom, dark, paddingButton, paddingButton);
            context->fillRect(innerrect, bg, context->fillColorSpace());
            context->restore();
        }
    }


    // We always return false so we do not request to be redrawn.
    return false;
}

#if ENABLE(VIDEO)

String RenderThemeAndroid::extraMediaControlsStyleSheet()
{
      return String(mediaControlsAndroidUserAgentStyleSheet, sizeof(mediaControlsAndroidUserAgentStyleSheet));
}

bool RenderThemeAndroid::shouldRenderMediaControlPart(ControlPart part, Element* e)
{
      HTMLMediaElement* mediaElement = static_cast<HTMLMediaElement*>(e);
      switch (part) {
      case MediaMuteButtonPart:
          return false;
      case MediaSeekBackButtonPart:
      case MediaSeekForwardButtonPart:
          return false;
      case MediaRewindButtonPart:
          return mediaElement->movieLoadType() != MediaPlayer::LiveStream;
      case MediaReturnToRealtimeButtonPart:
          return mediaElement->movieLoadType() == MediaPlayer::LiveStream;
      case MediaFullscreenButtonPart:
          return mediaElement->supportsFullscreen();
      case MediaToggleClosedCaptionsButtonPart:
          return mediaElement->hasClosedCaptions();
      default:
          return true;
      }
}

bool RenderThemeAndroid::paintMediaFullscreenButton(RenderObject* o, const PaintInfo& paintInfo, const IntRect& rect)
{
      bool translucent = false;
      if (o && toParentMediaElement(o) && toParentMediaElement(o)->hasTagName(HTMLNames::videoTag))
          translucent = true;
      paintInfo.context->platformContext()->drawMediaButton(rect, RenderSkinMediaButton::FULLSCREEN, translucent);
      return false;
}

bool RenderThemeAndroid::paintMediaMuteButton(RenderObject* o, const PaintInfo& paintInfo, const IntRect& rect)
{
      bool translucent = false;
      if (o && toParentMediaElement(o) && toParentMediaElement(o)->hasTagName(HTMLNames::videoTag))
          translucent = true;
      paintInfo.context->platformContext()->drawMediaButton(rect, RenderSkinMediaButton::MUTE, translucent);
      return false;
}

bool RenderThemeAndroid::paintMediaPlayButton(RenderObject* o, const PaintInfo& paintInfo, const IntRect& rect)
{
      bool translucent = false;
      if (o && toParentMediaElement(o) && toParentMediaElement(o)->hasTagName(HTMLNames::videoTag))
          translucent = true;
      if (MediaControlPlayButtonElement* btn = static_cast<MediaControlPlayButtonElement*>(o->node())) {
          if (btn->displayType() == MediaPlayButton)
              paintInfo.context->platformContext()->drawMediaButton(rect, RenderSkinMediaButton::PLAY, translucent);
          else
              paintInfo.context->platformContext()->drawMediaButton(rect, RenderSkinMediaButton::PAUSE, translucent);
          return false;
      }
      return true;
}

bool RenderThemeAndroid::paintMediaSeekBackButton(RenderObject* o, const PaintInfo& paintInfo, const IntRect& rect)
{
      bool translucent = false;
      if (o && toParentMediaElement(o) && toParentMediaElement(o)->hasTagName(HTMLNames::videoTag))
          translucent = true;
      paintInfo.context->platformContext()->drawMediaButton(rect, RenderSkinMediaButton::REWIND, translucent);
      return false;
}

bool RenderThemeAndroid::paintMediaSeekForwardButton(RenderObject* o, const PaintInfo& paintInfo, const IntRect& rect)
{
      bool translucent = false;
      if (o && toParentMediaElement(o) && toParentMediaElement(o)->hasTagName(HTMLNames::videoTag))
          translucent = true;
      paintInfo.context->platformContext()->drawMediaButton(rect, RenderSkinMediaButton::FORWARD, translucent);
      return false;
}

bool RenderThemeAndroid::paintMediaControlsBackground(RenderObject* o, const PaintInfo& paintInfo, const IntRect& rect)
{
      bool translucent = false;
      if (o && toParentMediaElement(o) && toParentMediaElement(o)->hasTagName(HTMLNames::videoTag))
          translucent = true;
      paintInfo.context->platformContext()->drawMediaButton(rect,
                                  RenderSkinMediaButton::BACKGROUND_SLIDER,
                                  translucent, false);
      return false;
}

bool RenderThemeAndroid::paintMediaSliderTrack(RenderObject* o, const PaintInfo& paintInfo, const IntRect& rect)
{
      bool translucent = false;
      if (o && toParentMediaElement(o) && toParentMediaElement(o)->hasTagName(HTMLNames::videoTag))
          translucent = true;
      IntRect thumb;
      if (o && o->isSlider())
          thumb = toRenderSlider(o)->thumbRect();
      paintInfo.context->platformContext()->drawMediaButton(rect,
                                  RenderSkinMediaButton::SLIDER_TRACK, translucent, true, thumb);
      return false;
}

bool RenderThemeAndroid::paintMediaSliderThumb(RenderObject* o, const PaintInfo& paintInfo, const IntRect& rect)
{
      bool translucent = false;
      if (o && toParentMediaElement(o) && toParentMediaElement(o)->hasTagName(HTMLNames::videoTag))
          translucent = true;
      paintInfo.context->platformContext()->drawMediaButton(rect,
                                  RenderSkinMediaButton::SLIDER_THUMB,
                                  translucent, false);
      return false;
}

void RenderThemeAndroid::adjustSliderThumbSize(RenderObject* o) const
{
    static const int sliderThumbWidth = RenderSkinMediaButton::sliderThumbWidth();
    static const int sliderThumbHeight = RenderSkinMediaButton::sliderThumbHeight();
    o->style()->setWidth(Length(sliderThumbWidth, Fixed));
    o->style()->setHeight(Length(sliderThumbHeight, Fixed));
}

#endif

bool RenderThemeAndroid::paintRadio(RenderObject* obj, const PaintInfo& info, const IntRect& rect)
{
    Node* node = obj->node();
    Element* element = static_cast<Element*>(node);
    if (element) {
        InputElement* input = element->toInputElement();
        GraphicsContext* context = info.context;
        context->save();
        Color borderColor = defaultFgColor;
        Color checkColor = defaultCheckColor;
        Color checkColorShadow = defaultCheckColorShadow;
        if (!element->isEnabledFormControl()) {
            borderColor = disabledFgColor;
            checkColor = disabledCheckColor;
            checkColorShadow = disabledCheckColorShadow;
        }
        IntRect borderRect = rect;
        borderRect.inflate(-3);
        const float cx = borderRect.center().x();
        const float cy = borderRect.center().y() - 1;
        context->setStrokeStyle(SolidStroke);
        context->setStrokeColor(borderColor, context->strokeColorSpace());
        context->setStrokeThickness(1);
        context->setFillColor(Color::transparent, context->fillColorSpace());
        context->setShadow(FloatSize(), 1.0f, borderColor, context->fillColorSpace());
        if (input->isCheckbox()) {
            if (input->isChecked()) {
                Path clip;
                clip.moveTo(FloatPoint(cx, cy - 1));
                clip.addLineTo(FloatPoint(rect.maxX() - 3, rect.y() + 1));
                clip.addLineTo(FloatPoint(rect.maxX(), rect.y() + 4));
                clip.addLineTo(FloatPoint(cx, cy + 5));
                clip.closeSubpath();
                context->save();
                context->clipOut(clip);
            }
            context->drawRect(borderRect);
            if (input->isChecked())
                context->restore();
        } else
            context->drawEllipse(borderRect);
        if (input->isChecked()) {
            context->setFillColor(checkColor, context->fillColorSpace());
            context->setStrokeColor(Color::transparent, context->strokeColorSpace());
            context->setShadow(FloatSize(), 2, checkColorShadow, context->fillColorSpace());
            if (input->isCheckbox()) {
                Path checkmark;
                checkmark.moveTo(FloatPoint(cx, cy));
                checkmark.addLineTo(FloatPoint(rect.maxX() - 2, rect.y() + 1));
                checkmark.addLineTo(FloatPoint(rect.maxX(), rect.y() + 3));
                checkmark.addLineTo(FloatPoint(cx, cy + 4));
                checkmark.addLineTo(FloatPoint(cx - 4, cy));
                checkmark.addLineTo(FloatPoint(cx - 2, cy - 2));
                checkmark.closeSubpath();
                context->fillPath(checkmark);
            } else {
                borderRect.inflate(-3);
                context->drawEllipse(borderRect);
            }
        }
        context->restore();
    }
    return false;
}

void RenderThemeAndroid::setCheckboxSize(RenderStyle* style) const
{
    style->setWidth(Length(19, Fixed));
    style->setHeight(Length(19, Fixed));
}

void RenderThemeAndroid::setRadioSize(RenderStyle* style) const
{
    // This is the same as checkboxes.
    setCheckboxSize(style);
}

void RenderThemeAndroid::adjustTextFieldStyle(CSSStyleSelector*, RenderStyle* style, WebCore::Element*) const
{
    addIntrinsicMargins(style);
}

bool RenderThemeAndroid::paintTextField(RenderObject*, const PaintInfo&, const IntRect&)
{
    return true;
}

void RenderThemeAndroid::adjustTextAreaStyle(CSSStyleSelector*, RenderStyle* style, WebCore::Element*) const
{
    addIntrinsicMargins(style);
}

bool RenderThemeAndroid::paintTextArea(RenderObject* obj, const PaintInfo& info, const IntRect& rect)
{
    if (obj->isMenuList())
        paintCombo(obj, info, rect);
    return true;
}

void RenderThemeAndroid::adjustSearchFieldStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    addIntrinsicMargins(style);
}

bool RenderThemeAndroid::paintSearchField(RenderObject*, const PaintInfo&, const IntRect&)
{
    return true;
}

static void adjustMenuListStyleCommon(RenderStyle* style)
{
    // Added to make room for our arrow and make the touch target less cramped.
    const int padding = (int)(scaleFactor[RenderSkinAndroid::DrawableResolution()] + 0.5f);
    style->setPaddingLeft(Length(padding,Fixed));
    style->setPaddingTop(Length(padding, Fixed));
    style->setPaddingBottom(Length(padding, Fixed));
    // allocate height as arrow size
    int arrow = std::max(18, style->fontMetrics().height() + 2 * padding);
    style->setPaddingRight(Length(arrow, Fixed));
    style->setMinHeight(Length(arrow, Fixed));
    style->setHeight(Length(arrow, Fixed));
}

void RenderThemeAndroid::adjustListboxStyle(CSSStyleSelector*, RenderStyle* style, Element* e) const
{
    adjustMenuListButtonStyle(0, style, 0);
}

void RenderThemeAndroid::adjustMenuListStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    adjustMenuListStyleCommon(style);
    addIntrinsicMargins(style);
}

bool RenderThemeAndroid::paintCombo(RenderObject* obj, const PaintInfo& info,  const IntRect& rect)
{
  if (obj->style() && !obj->style()->visitedDependentColor(CSSPropertyBackgroundColor).alpha())
        return true;
    Node* node = obj->node();
    Element* element = static_cast<Element*>(node);
    if (element) {
        InputElement* input = element->toInputElement();
        GraphicsContext* context = info.context;
        context->save();
        if (!element->isEnabledFormControl())
            context->setAlpha(0.5f);
        IntRect bounds = IntRect(rect.x(), rect.y(), rect.width(), rect.height());
        // paint bg color
        RenderStyle* style = obj->style();
        context->setFillColor(style->visitedDependentColor(CSSPropertyBackgroundColor),
                context->fillColorSpace());
        context->fillRect(FloatRect(bounds));
        // copied form the original RenderSkinCombo:
        // If this is an appearance where RenderTheme::paint returns true
        // without doing anything, this means that
        // RenderBox::PaintBoxDecorationWithSize will end up painting the
        // border, so we shouldn't paint a border here.
        if (style->appearance() != MenulistButtonPart &&
                style->appearance() != ListboxPart &&
                style->appearance() != TextFieldPart &&
                style->appearance() != TextAreaPart) {
            const int arrowSize = bounds.height();
            // dropdown button bg
            context->setFillColor(Color(defaultBgColor), context->fillColorSpace());
            context->fillRect(FloatRect(bounds.maxX() - arrowSize + 0.5f, bounds.y() + .5f,
                    arrowSize - 1, bounds.height() - 1));
            // outline
            context->setStrokeStyle(SolidStroke);
            context->setStrokeThickness(1.0f);
            context->setStrokeColor(Color(defaultBgDark), context->strokeColorSpace());
            context->strokeRect(bounds, 1.0f);
            // arrow
            context->setFillColor(Color(defaultFgColor), context->fillColorSpace());
            Path tri = Path();
            tri.clear();
            const float aw = arrowSize - 10;
            FloatPoint br = FloatPoint(bounds.maxX() - 4, bounds.maxY() - 4);
            tri.moveTo(br);
            tri.addLineTo(FloatPoint(br.x() - aw, br.y()));
            tri.addLineTo(FloatPoint(br.x(), br.y() - aw));
            context->fillPath(tri);
        }
        context->restore();
    }
    return false;
}

bool RenderThemeAndroid::paintMenuList(RenderObject* obj, const PaintInfo& info, const IntRect& rect)
{
    return paintCombo(obj, info, rect);
}

void RenderThemeAndroid::adjustMenuListButtonStyle(CSSStyleSelector*,
        RenderStyle* style, Element*) const
{
    // Copied from RenderThemeSafari.
    const float baseFontSize = 11.0f;
    const int baseBorderRadius = 5;
    float fontScale = style->fontSize() / baseFontSize;

    style->resetPadding();
    style->setBorderRadius(IntSize(int(baseBorderRadius + fontScale - 1), int(baseBorderRadius + fontScale - 1))); // FIXME: Round up?

    const int minHeight = 15;
    style->setMinHeight(Length(minHeight, Fixed));

    style->setLineHeight(RenderStyle::initialLineHeight());
    // Found these padding numbers by trial and error.
    const int padding = 4;
    style->setPaddingTop(Length(padding, Fixed));
    style->setPaddingLeft(Length(padding, Fixed));
    adjustMenuListStyleCommon(style);
}

bool RenderThemeAndroid::paintMenuListButton(RenderObject* obj, const PaintInfo& info, const IntRect& rect)
{
    return paintCombo(obj, info, rect);
}

bool RenderThemeAndroid::paintSliderTrack(RenderObject* o, const PaintInfo& i, const IntRect& r)
{
    static const bool translucent = true;
    i.context->platformContext()->drawMediaButton(r,
                                RenderSkinMediaButton::SLIDER_TRACK,
                                translucent, false);
    return false;
}

bool RenderThemeAndroid::paintSliderThumb(RenderObject* o, const PaintInfo& i, const IntRect& r)
{
    static const bool translucent = true;
    i.context->platformContext()->drawMediaButton(r,
                                RenderSkinMediaButton::SLIDER_THUMB,
                                translucent, false);
    return false;
}

Color RenderThemeAndroid::platformFocusRingColor() const
{
    static Color focusRingColor(0x33, 0xB5, 0xE5, 0x66);
    return focusRingColor;
}

bool RenderThemeAndroid::supportsFocusRing(const RenderStyle* style) const
{
    // Draw the focus ring ourselves unless it is a text area (webkit does borders better)
    if (!style || !style->hasAppearance())
        return true;
    return style->appearance() != TextFieldPart && style->appearance() != TextAreaPart;
}

} // namespace WebCore
