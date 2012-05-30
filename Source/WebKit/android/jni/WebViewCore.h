/*
 * Copyright 2006, The Android Open Source Project
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

#ifndef WebViewCore_h
#define WebViewCore_h

#include "DeviceMotionAndOrientationManager.h"
#include "DOMSelection.h"
#include "FileChooser.h"
#include "FocusDirection.h"
#include "HitTestResult.h"
#include "PicturePile.h"
#include "PlatformGraphicsContext.h"
#include "Position.h"
#include "ScrollTypes.h"
#include "SkColor.h"
#include "SkTDArray.h"
#include "SkRegion.h"
#include "Text.h"
#include "Timer.h"
#include "WebCoreRefObject.h"
#include "WebCoreJni.h"
#include "WebRequestContext.h"
#include "android_npapi.h"
#include "VisiblePosition.h"
#include "SelectText.h"

#include <jni.h>
#include <androidfw/KeycodeLabels.h>
#include <ui/PixelFormat.h>
#include <utils/threads.h>
#include <wtf/Threading.h>

namespace WebCore {
    class Color;
    class GraphicsOperationCollection;
    class FrameView;
    class HTMLAnchorElement;
    class HTMLElement;
    class HTMLImageElement;
    class HTMLSelectElement;
    class RenderPart;
    class RenderText;
    class Node;
    class PlatformKeyboardEvent;
    class QualifiedName;
    class RenderTextControl;
    class ScrollView;
    class TimerBase;
    class PageGroup;
}

#if USE(ACCELERATED_COMPOSITING)
namespace WebCore {
    class GraphicsLayerAndroid;
    class LayerAndroid;
}
#endif

namespace WebCore {
    class BaseLayerAndroid;
}

struct PluginWidgetAndroid;
class SkPicture;
class SkIRect;

namespace android {

    enum Direction {
        DIRECTION_BACKWARD = 0,
        DIRECTION_FORWARD = 1
    };

    enum NavigationAxis {
        AXIS_CHARACTER = 0,
        AXIS_WORD = 1,
        AXIS_SENTENCE = 2,
        AXIS_HEADING = 3,
        AXIS_SIBLING = 4,
        AXIS_PARENT_FIRST_CHILD = 5,
        AXIS_DOCUMENT = 6
    };

    class ListBoxReply;
    class AndroidHitTestResult;

    class WebCoreReply : public WebCoreRefObject {
    public:
        virtual ~WebCoreReply() {}

        virtual void replyInt(int value) {
            SkDEBUGF(("WebCoreReply::replyInt(%d) not handled\n", value));
        }

        virtual void replyIntArray(const int* array, int count) {
            SkDEBUGF(("WebCoreReply::replyIntArray() not handled\n"));
        }
            // add more replyFoo signatures as needed
    };

    // one instance of WebViewCore per page for calling into Java's WebViewCore
    class WebViewCore : public WebCoreRefObject, public WebCore::PicturePainter {
    public:
        /**
         * Initialize the native WebViewCore with a JNI environment, a Java
         * WebViewCore object and the main frame.
         */
        WebViewCore(JNIEnv* env, jobject javaView, WebCore::Frame* mainframe);
        ~WebViewCore();

        // helper function
        static WebViewCore* getWebViewCore(const WebCore::FrameView* view);
        static WebViewCore* getWebViewCore(const WebCore::ScrollView* view);

        // Followings are called from native WebCore to Java

        void focusNodeChanged(WebCore::Node*);

        /**
         * Scroll to an absolute position.
         * @param x The x coordinate.
         * @param y The y coordinate.
         * @param animate If it is true, animate to the new scroll position
         *
         * This method calls Java to trigger a gradual scroll event.
         */
        void scrollTo(int x, int y, bool animate = false);

        /**
         * Record the invalid rectangle
         */
        void contentInvalidate(const WebCore::IntRect &rect);
        void contentInvalidateAll();

        /**
         * Satisfy any outstanding invalidates, so that the current state
         * of the DOM is drawn.
         */
        void contentDraw();

#if USE(ACCELERATED_COMPOSITING)
        WebCore::GraphicsLayerAndroid* graphicsRootLayer() const;
#endif

        /** Invalidate the view/screen, NOT the content/DOM, but expressed in
         *  content/DOM coordinates (i.e. they need to eventually be scaled,
         *  by webview into view.java coordinates
         */
        void viewInvalidate(const WebCore::IntRect& rect);

        /**
         * Invalidate part of the content that may be offscreen at the moment
         */
        void offInvalidate(const WebCore::IntRect &rect);

        /**
         * Called by webcore when the progress indicator is done
         * used to rebuild and display any changes in focus
         */
        void notifyProgressFinished();

        /**
         * Notify the view that WebCore did its first layout.
         */
        void didFirstLayout();

        /**
         * Notify the view to update the viewport.
         */
        void updateViewport();

        /**
         * Notify the view to restore the screen width, which in turn restores
         * the scale. Also restore the scale for the text wrap.
         */
        void restoreScale(float scale, float textWrapScale);

        /**
         * Tell the java side to update the focused textfield
         * @param pointer   Pointer to the node for the input field.
         * @param   changeToPassword  If true, we are changing the textfield to
         *          a password field, and ignore the WTF::String
         * @param text  If changeToPassword is false, this is the new text that
         *              should go into the textfield.
         */
        void updateTextfield(WebCore::Node* pointer,
                bool changeToPassword, const WTF::String& text);

        /**
         * Tell the java side to update the current selection in the focused
         * textfield to the WebTextView.  This function finds the currently
         * focused textinput, and passes its selection to java.
         * If there is no focus, or it is not a text input, this does nothing.
         */
        void updateTextSelection();

        /**
         * Updates the java side with the node's content size and scroll
         * position.
         */
        void updateTextSizeAndScroll(WebCore::Node* node);

        void clearTextEntry();
        // JavaScript support
        void jsAlert(const WTF::String& url, const WTF::String& text);
        bool jsConfirm(const WTF::String& url, const WTF::String& text);
        bool jsPrompt(const WTF::String& url, const WTF::String& message,
                const WTF::String& defaultValue, WTF::String& result);
        bool jsUnload(const WTF::String& url, const WTF::String& message);
        bool jsInterrupt();

        /**
         * Posts a message to the UI thread to inform the Java side that the
         * origin has exceeded its database quota.
         * @param url The URL of the page that caused the quota overflow
         * @param databaseIdentifier the id of the database that caused the
         *     quota overflow.
         * @param currentQuota The current quota for the origin
         * @param estimatedSize The estimated size of the database
         * @return Whether the message was successfully sent.
         */
        bool exceededDatabaseQuota(const WTF::String& url,
                                   const WTF::String& databaseIdentifier,
                                   const unsigned long long currentQuota,
                                   const unsigned long long estimatedSize);

        /**
         * Posts a message to the UI thread to inform the Java side that the
         * appcache has exceeded its max size.
         * @param spaceNeeded is the amount of disk space that would be needed
         * in order for the last appcache operation to succeed.
         * @return Whether the message was successfully sent.
         */
        bool reachedMaxAppCacheSize(const unsigned long long spaceNeeded);

        /**
         * Set up the PageGroup's idea of which links have been visited,
         * with the browser history.
         * @param group the object to deliver the links to.
         */
        void populateVisitedLinks(WebCore::PageGroup*);

        /**
         * Instruct the browser to show a Geolocation permission prompt for the
         * specified origin.
         * @param origin The origin of the frame requesting Geolocation
         *     permissions.
         */
        void geolocationPermissionsShowPrompt(const WTF::String& origin);
        /**
         * Instruct the browser to hide the Geolocation permission prompt.
         */
        void geolocationPermissionsHidePrompt();

        jobject getDeviceMotionService();
        jobject getDeviceOrientationService();

        void addMessageToConsole(const WTF::String& message, unsigned int lineNumber, const WTF::String& sourceID, int msgLevel);

        /**
         * Tell the Java side of the scrollbar mode
         */
        void setScrollbarModes(WebCore::ScrollbarMode horizontalMode,
                               WebCore::ScrollbarMode verticalMode);

        //
        // Followings support calls from Java to native WebCore
        //

        WTF::String retrieveHref(int x, int y);
        WTF::String retrieveAnchorText(int x, int y);
        WTF::String retrieveImageSource(int x, int y);
        WTF::String requestLabel(WebCore::Frame* , WebCore::Node* );

        // If the focus is a textfield (<input>), textarea, or contentEditable,
        // scroll the selection on screen (if necessary).
        void revealSelection();

        void moveMouse(int x, int y, WebCore::HitTestResult* hoveredNode = 0,
                       bool isClickCandidate = false);

        // set the scroll amount that webview.java is currently showing
        void setScrollOffset(bool sendScrollEvent, int dx, int dy);

        void setGlobalBounds(int x, int y, int h, int v);

        void setSizeScreenWidthAndScale(int width, int height, int screenWidth,
            float scale, int realScreenWidth, int screenHeight, int anchorX,
            int anchorY, bool ignoreHeight);

        /**
         * Handle key events from Java.
         * @return Whether keyCode was handled by this class.
         */
        bool key(const WebCore::PlatformKeyboardEvent& event);
        bool chromeCanTakeFocus(WebCore::FocusDirection direction);
        void chromeTakeFocus(WebCore::FocusDirection direction);
        void setInitialFocus(const WebCore::PlatformKeyboardEvent& event);

        /**
         * Handle touch event
         * Returns an int with the following flags:
         * bit 0: hit an event handler
         * bit 1: preventDefault was called
         */
        int handleTouchEvent(int action, WTF::Vector<int>& ids,
                              WTF::Vector<WebCore::IntPoint>& points,
                              int actionIndex, int metaState);

        /**
         * Clicks the mouse at its current location
         */
        bool performMouseClick();

        /**
         * Sets the index of the label from a popup
         */
        void popupReply(int index);
        void popupReply(const int* array, int count);

        /**
         *  Delete text from start to end in the focused textfield.
         *  If start == end, set the selection, but perform no deletion.
         *  If there is no focus, silently fail.
         *  If start and end are out of order, swap them.
         */
        void deleteSelection(int start, int end, int textGeneration);

        /**
         *  Set the selection of the currently focused textfield to (start, end).
         *  If start and end are out of order, swap them.
         */
        void setSelection(int start, int end);

        /**
         * Modifies the current selection.
         *
         * Note: Accessibility support.
         *
         * direction - The direction in which to alter the selection.
         * granularity - The granularity of the selection modification.
         *
         * returns - The selected HTML as a WTF::String. This is not a well formed
         *           HTML, rather the selection annotated with the tags of all
         *           intermediary elements it crosses.
         */
        WTF::String modifySelection(const int direction, const int granularity);

        /**
         * Moves the selection to the given node in a given frame i.e. selects that node.
         *
         * Note: Accessibility support.
         *
         * frame - The frame in which to select is the node to be selected.
         * node - The node to be selected.
         *
         * returns - The selected HTML as a WTF::String. This is not a well formed
         *           HTML, rather the selection annotated with the tags of all
         *           intermediary elements it crosses.
         */
        WTF::String moveSelection(WebCore::Frame* frame, WebCore::Node* node);

        /**
         *  In the currently focused textfield, replace the characters from oldStart to oldEnd
         *  (if oldStart == oldEnd, this will be an insert at that position) with replace,
         *  and set the selection to (start, end).
         */
        void replaceTextfieldText(int oldStart,
            int oldEnd, const WTF::String& replace, int start, int end,
            int textGeneration);
        void passToJs(int generation,
            const WTF::String& , const WebCore::PlatformKeyboardEvent& );
        /**
         * Scroll the focused textfield to (x, y) in document space
         */
        WebCore::IntRect scrollFocusedTextInput(float x, int y);
        /**
         * Set the FocusController's active and focused states, so that
         * the caret will draw (true) or not.
         */
        void setFocusControllerActive(bool active);

        void saveDocumentState(WebCore::Frame* frame);

        void addVisitedLink(const UChar*, int);

        // TODO: I don't like this hack but I need to access the java object in
        // order to send it as a parameter to java
        AutoJObject getJavaObject();

        // Return the parent WebView Java object associated with this
        // WebViewCore.
        jobject getWebViewJavaObject();

        void setBackgroundColor(SkColor c);

        void dumpDomTree(bool);
        void dumpRenderTree(bool);

        /*  We maintain a list of active plugins. The list is edited by the
            pluginview itself. The list is used to service invals to the plugin
            pageflipping bitmap.
         */
        void addPlugin(PluginWidgetAndroid*);
        void removePlugin(PluginWidgetAndroid*);
        // returns true if the pluginwidgit is in our active list
        bool isPlugin(PluginWidgetAndroid*) const;
        void invalPlugin(PluginWidgetAndroid*);
        void drawPlugins();

        // send the current screen size/zoom to all of the plugins in our list
        void sendPluginVisibleScreen();

        // notify plugin that a new drawing surface was created in the UI thread
        void sendPluginSurfaceReady();

        // send onLoad event to plugins who are descendents of the given frame
        void notifyPluginsOnFrameLoad(const WebCore::Frame*);

        // gets a rect representing the current on-screen portion of the document
        void getVisibleScreen(ANPRectI&);

        // send this event to all of the plugins in our list
        void sendPluginEvent(const ANPEvent&);

        // lookup the plugin widget struct given an NPP
        PluginWidgetAndroid* getPluginWidget(NPP npp);

        // Notify the Java side whether it needs to pass down the touch events
        void needTouchEvents(bool);

        void requestKeyboardWithSelection(const WebCore::Node*, int selStart, int selEnd);
        // Notify the Java side that webkit is requesting a keyboard
        void requestKeyboard(bool showKeyboard);

        // Generates a class loader that contains classes from the plugin's apk
        jclass getPluginClass(const WTF::String& libName, const char* className);

        // Creates a full screen surface for a plugin
        void showFullScreenPlugin(jobject webkitPlugin, int32_t orientation, NPP npp);

        // Instructs the UI thread to discard the plugin's full-screen surface
        void hideFullScreenPlugin();

        // Creates a childView for the plugin but does not attach to the view hierarchy
        jobject createSurface(jobject view);

        // Adds the plugin's view (aka surface) to the view hierarchy
        jobject addSurface(jobject view, int x, int y, int width, int height);

        // Updates a Surface coordinates and dimensions for a plugin
        void updateSurface(jobject childView, int x, int y, int width, int height);

        // Destroys a SurfaceView for a plugin
        void destroySurface(jobject childView);

        // Returns the context (android.content.Context) of the WebView
        jobject getContext();

        // Manages requests to keep the screen on while the WebView is visible
        void keepScreenOn(bool screenOn);

        // Make the rect (left, top, width, height) visible. If it can be fully
        // fit, center it on the screen. Otherwise make sure the point specified
        // by (left + xPercentInDoc * width, top + yPercentInDoc * height)
        // pinned at the screen position (xPercentInView, yPercentInView).
        void showRect(int left, int top, int width, int height, int contentWidth,
            int contentHeight, float xPercentInDoc, float xPercentInView,
            float yPercentInDoc, float yPercentInView);

        // Scale the rect (x, y, width, height) to make it just fit and centered
        // in the current view.
        void centerFitRect(int x, int y, int width, int height);

        // return a list of rects matching the touch point (x, y) with the slop
        WTF::Vector<WebCore::IntRect> getTouchHighlightRects(int x, int y, int slop,
                WebCore::Node** node, WebCore::HitTestResult* hitTestResult);
        // This does a sloppy hit test
        AndroidHitTestResult hitTestAtPoint(int x, int y, int slop, bool doMoveMouse = false);
        static bool nodeIsClickableOrFocusable(WebCore::Node* node);

        // Open a file chooser for selecting a file to upload
        void openFileChooser(PassRefPtr<WebCore::FileChooser> );

        // reset the picture set to empty
        void clearContent();

        bool focusBoundsChanged();

        // record content in a new BaseLayerAndroid, copying the layer tree as well
        WebCore::BaseLayerAndroid* recordContent(SkIPoint* );

        // This creates a new BaseLayerAndroid by copying the current m_content
        // and doing a copy of the layers. The layers' content may be updated
        // as we are calling layersSync().
        WebCore::BaseLayerAndroid* createBaseLayer(GraphicsLayerAndroid* root);
        bool updateLayers(WebCore::LayerAndroid*);
        void notifyAnimationStarted();

        int textWrapWidth() const { return m_textWrapWidth; }
        float scale() const { return m_scale; }
        float textWrapScale() const { return m_screenWidth * m_scale / m_textWrapWidth; }
        WebCore::Frame* mainFrame() const { return m_mainFrame; }
        WebCore::Frame* focusedFrame() const;

        void notifyWebAppCanBeInstalled();

        void deleteText(int startX, int startY, int endX, int endY);
        WTF::String getText(int startX, int startY, int endX, int endY);
        void insertText(const WTF::String &text);

        // find on page
        void resetFindOnPage();
        int findTextOnPage(const WTF::String &text);
        int findNextOnPage(bool forward);
        void updateMatchCount() const;

#if ENABLE(VIDEO)
        void enterFullscreenForVideoLayer(int layerId, const WTF::String& url);
        void exitFullscreenVideo();
#endif

        void setWebTextViewAutoFillable(int queryId, const string16& previewSummary);

        DeviceMotionAndOrientationManager* deviceMotionAndOrientationManager() { return &m_deviceMotionAndOrientationManager; }

        void listBoxRequest(WebCoreReply* reply, const uint16_t** labels,
                size_t count, const int enabled[], size_t enabledCount,
                bool multiple, const int selected[], size_t selectedCountOrSelection);
        bool isPaused() const { return m_isPaused; }
        void setIsPaused(bool isPaused) { m_isPaused = isPaused; }
        bool drawIsPaused() const;
        // The actual content (without title bar) size in doc coordinate
        int  screenWidth() const { return m_screenWidth; }
        int  screenHeight() const { return m_screenHeight; }
        void setWebRequestContextUserAgent();
        void setWebRequestContextCacheMode(int mode);
        WebRequestContext* webRequestContext();
        // Attempts to scroll the layer to the x,y coordinates of rect. The
        // layer is the id of the LayerAndroid.
        void scrollRenderLayer(int layer, const SkRect& rect);
        // call only from webkit thread (like add/remove), return true if inst
        // is still alive
        static bool isInstance(WebViewCore*);
        // if there exists at least one WebViewCore instance then we return the
        // application context, otherwise NULL is returned.
        static jobject getApplicationContext();
        // Check whether a media mimeType is supported in Android media framework.
        static bool isSupportedMediaMimeType(const WTF::String& mimeType);

        /**
         * Returns all text ranges consumed by the cursor points referred
         * to by startX, startY, endX, and endY. The vector will be empty
         * if no text is in the given area or if the positions are invalid.
         */
        Vector<WebCore::VisibleSelection> getTextRanges(
                int startX, int startY, int endX, int endY);
        static int platformLayerIdFromNode(WebCore::Node* node,
                                           WebCore::LayerAndroid** outLayer = 0);
        void selectText(int startX, int startY, int endX, int endY);
        bool selectWordAt(int x, int y);

        // Converts from the global content coordinates that WebView sends
        // to frame-local content coordinates using the focused frame
        WebCore::IntPoint convertGlobalContentToFrameContent(const WebCore::IntPoint& point, WebCore::Frame* frame = 0);
        static void layerToAbsoluteOffset(const WebCore::LayerAndroid* layer,
                                          WebCore::IntPoint& offset);

        // Retrieves the current locale from system properties
        void getLocale(String& language, WTF::String& region);

        // Handles changes in system locale
        void updateLocale();

        // these members are shared with webview.cpp
        int m_touchGeneration; // copy of state in WebViewNative triggered by touch
        int m_lastGeneration; // last action using up to date cache
        // end of shared members

        void setPrerenderingEnabled(bool enable);

        // internal functions
    private:
        enum InputType {
            NONE = -1,
            NORMAL_TEXT_FIELD = 0,
            TEXT_AREA = 1,
            PASSWORD = 2,
            SEARCH = 3,
            EMAIL = 4,
            NUMBER = 5,
            TELEPHONE = 6,
            URL = 7,
        };

        WebCore::Node* currentFocus();
        void layout();
        // Create a set of pictures to represent the drawn DOM, driven by
        // the invalidated region and the time required to draw (used to draw)
        void recordPicturePile();

        virtual void paintContents(WebCore::GraphicsContext* gc, WebCore::IntRect& dirty);
        virtual SkCanvas* createPrerenderCanvas(WebCore::PrerenderedInval* prerendered);
#ifdef CONTEXT_RECORDING
        WebCore::GraphicsOperationCollection* rebuildGraphicsOperationCollection(const SkIRect& inval);
#endif
        void sendNotifyProgressFinished();
        /*
         * Handle a mouse click, either from a touch or trackball press.
         * @param frame Pointer to the Frame containing the node that was clicked on.
         * @param node Pointer to the Node that was clicked on.
         * @param fake This is a fake mouse click, used to put a textfield into focus. Do not
         *      open the IME.
         */
        WebCore::HTMLAnchorElement* retrieveAnchorElement(int x, int y);
        WebCore::HTMLElement* retrieveElement(int x, int y,
            const WebCore::QualifiedName& );
        WebCore::HTMLImageElement* retrieveImageElement(int x, int y);
        // below are members responsible for accessibility support
        WTF::String modifySelectionTextNavigationAxis(WebCore::DOMSelection* selection,
                                                 int direction, int granularity);
        WTF::String modifySelectionDomNavigationAxis(WebCore::DOMSelection* selection,
                                                int direction, int granularity);
        WebCore::Text* traverseNextContentTextNode(WebCore::Node* fromNode,
                                                   WebCore::Node* toNode,
                                                   int direction);
        bool isVisible(WebCore::Node* node);
        bool isHeading(WebCore::Node* node);
        WTF::String formatMarkup(WebCore::DOMSelection* selection);
        void selectAt(int x, int y);

        void scrollNodeIntoView(WebCore::Frame* frame, WebCore::Node* node);
        bool isContentTextNode(WebCore::Node* node);
        WebCore::Node* getIntermediaryInputElement(WebCore::Node* fromNode,
                                                   WebCore::Node* toNode,
                                                   int direction);
        bool isContentInputElement(WebCore::Node* node);
        bool isDescendantOf(WebCore::Node* parent, WebCore::Node* node);
        void advanceAnchorNode(WebCore::DOMSelection* selection, int direction,
                               WTF::String& markup, bool ignoreFirstNode,
                               WebCore::ExceptionCode& ec);
        WebCore::Node* getNextAnchorNode(WebCore::Node* anchorNode,
                                         bool skipFirstHack, int direction);
        WebCore::Node* getImplicitBoundaryNode(WebCore::Node* node,
                                               unsigned offset, int direction);
        jobject createTextFieldInitData(WebCore::Node* node);
        /**
         * Calls into java to reset the text edit field with the
         * current contents and selection.
         */
        void initEditField(WebCore::Node* node);

        /**
         * If node is not a text input field or if it explicitly requests
         * not to have keyboard input, then the soft keyboard is closed. If
         * it is a text input field then initEditField is called and
         * auto-fill information is requested for HTML form input fields.
         */
        void initializeTextInput(WebCore::Node* node, bool fake);

        /**
         * Gets the input type a Node. NONE is returned if it isn't an
         * input field.
         */
        InputType getInputType(WebCore::Node* node);

        /**
         * If node is an input field, the spellcheck value for the
         * field is returned. Otherwise true is returned.
         */
        static bool isSpellCheckEnabled(WebCore::Node* node);

        /**
         * Returns the offsets of the selection area for both normal text
         * fields and content editable fields. start and end are modified
         * by this method.
         */
        static void getSelectionOffsets(WebCore::Node* node, int& start, int& end);
        /**
         * Gets the plain text of the specified editable text field. node
         * may be content-editable or a plain text fields.
         */
        static WTF::String getInputText(WebCore::Node* node);
        /**
         * Gets the RenderTextControl for the given node if it has one.
         * If its renderer isn't a RenderTextControl, then NULL is returned.
         */
        static WebCore::RenderTextControl* toRenderTextControl(WebCore::Node *node);
        /**
         * Sets the selection for node's editable field to the offsets
         * between start (inclusive) and end (exclusive).
         */
        static void setSelection(WebCore::Node* node, int start, int end);
        /**
         * Returns the Position for the given offset for an editable
         * field. The offset is relative to the node start.
         */
        static WebCore::Position getPositionForOffset(WebCore::Node* node, int offset);

        WebCore::VisiblePosition visiblePositionForContentPoint(int x, int y);
        WebCore::VisiblePosition visiblePositionForContentPoint(const WebCore::IntPoint& point);
        bool selectWordAroundPosition(WebCore::Frame* frame,
                                      WebCore::VisiblePosition pos);
        SelectText* createSelectText(const WebCore::VisibleSelection&);
        void setSelectionCaretInfo(SelectText* selectTextContainer,
                const WebCore::Position& position,
                const WebCore::IntPoint& frameOffset,
                SelectText::HandleId handleId, int offset,
                EAffinity affinity);
        static int getMaxLength(WebCore::Node* node);
        static WTF::String getFieldName(WebCore::Node* node);
        static bool isAutoCompleteEnabled(WebCore::Node* node);
        WebCore::IntRect absoluteContentRect(WebCore::Node* node,
                WebCore::LayerAndroid* layer);
        static WebCore::IntRect positionToTextRect(const WebCore::Position& position,
                WebCore::EAffinity affinity, const WebCore::IntPoint& offset);
        static bool isLtr(const WebCore::Position& position);
        static WebCore::Position trimSelectionPosition(
                const WebCore::Position& start, const WebCore::Position& stop);

        // called from constructor, to add this to a global list
        static void addInstance(WebViewCore*);
        // called from destructor, to remove this from a global list
        static void removeInstance(WebViewCore*);

        bool prerenderingEnabled();

        friend class ListBoxReply;
        struct JavaGlue;
        struct JavaGlue*       m_javaGlue;
        struct TextFieldInitDataGlue;
        struct TextFieldInitDataGlue* m_textFieldInitDataGlue;
        WebCore::Frame*        m_mainFrame;
        WebCoreReply*          m_popupReply;
        WebCore::PicturePile m_content; // the set of pictures to draw
        // Used in passToJS to avoid updating the UI text field until after the
        // key event has been processed.
        bool m_blockTextfieldUpdates;
        bool m_focusBoundsChanged;
        bool m_skipContentDraw;
        // Passed in with key events to know when they were generated.  Store it
        // with the cache so that we can ignore stale text changes.
        int m_textGeneration;
        int m_maxXScroll;
        int m_maxYScroll;
        int m_scrollOffsetX; // webview.java's current scroll in X
        int m_scrollOffsetY; // webview.java's current scroll in Y
        double m_scrollSetTime; // when the scroll was last set
        WebCore::IntPoint m_mousePos;
        // This is the location at which we will click. This is tracked
        // separately from m_mousePos, because m_mousePos may be updated
        // in the interval between ACTION_UP and when the click fires since
        // that occurs after a delay. This also works around potential hardware
        // issues if we get onHoverEvents when using the touch screen, as that
        // will nullify the slop checking we do in hitTest (aka, ACTION_DOWN)
        WebCore::IntPoint m_mouseClickPos;
        int m_screenWidth; // width of the visible rect in document coordinates
        int m_screenHeight;// height of the visible rect in document coordinates
        int m_textWrapWidth;
        float m_scale;
        WebCore::PageGroup* m_groupForVisitedLinks;
        bool m_isPaused;
        int m_cacheMode;
        bool m_fullscreenVideoMode;

        // find on page data
        WTF::String m_searchText;
        int m_matchCount;
        int m_activeMatchIndex;
        RefPtr<WebCore::Range> m_activeMatch;

        SkTDArray<PluginWidgetAndroid*> m_plugins;
        WebCore::Timer<WebViewCore> m_pluginInvalTimer;
        void pluginInvalTimerFired(WebCore::Timer<WebViewCore>*) {
            this->drawPlugins();
        }

        int m_screenOnCounter;
        WebCore::Node* m_currentNodeDomNavigationAxis;
        DeviceMotionAndOrientationManager m_deviceMotionAndOrientationManager;

#if ENABLE(TOUCH_EVENTS)
        bool m_forwardingTouchEvents;
#endif

        scoped_refptr<WebRequestContext> m_webRequestContext;

        WTF::Mutex m_prerenderLock;
        bool m_prerenderEnabled;
    };

}   // namespace android

#endif // WebViewCore_h
