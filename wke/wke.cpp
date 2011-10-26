
#include <WebCore/config.h>

#include <JavaScriptCore/InitializeThreading.h>
#include <WebCore/ChromeClient.h>
#include <WebCore/FrameLoaderClient.h>
#include <WebCore/ResourceError.h>
#include <WebCore/Page.h>
#include <WebCore/Frame.h>
#include <WebCore/FileChooser.h>
#include <WebCore/FormState.h>
#include <WebCore/HTMLFormElement.h>

#include <WebCore/PageGroup.h>
#include <WebCore/IconDatabase.h>
#include <WebCore/FrameView.h>
#include <WebCore/BitmapInfo.h>
#include <WebCore/Settings.h>
#include <WebCore/PlatformWheelEvent.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/FocusController.h>
#include <WebCore/ScriptValue.h>
#include <WebCore/BackForwardList.h>
#include <WebCore/WebCoreInstanceHandle.h>
#include <WebCore/RenderThemeWin.h>
#include <WebCore/TextEncoding.h>
#include <WebCore/ResourceHandleManager.h>

#include "wke.h"

#pragma warning(disable : 4481)

#include "wkeDebug.inl"
#include "wkeChromeClient.inl"
#include "wkeFrameLoaderClient.inl"
#include "wkeContextMenuClient.inl"
#include "wkeInspectorClient.inl"
#include "wkeEditorClient.inl"
#include "wkeDragClient.inl"
#include "wkePlatformStrategies.inl"

#include "icuwin.h"

namespace wke
{
    #define SPI_GETWHEELSCROLLCHARS (0x006C)
    static int horizontalScrollChars()
    {
        static ULONG scrollChars;
        if (!scrollChars && !SystemParametersInfo(SPI_GETWHEELSCROLLCHARS, 0, &scrollChars, 0))
            scrollChars = 1;
        return scrollChars;
    }

    static int verticalScrollLines()
    {
        static ULONG scrollLines;
        if (!scrollLines && !SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &scrollLines, 0))
            scrollLines = 3;
        return scrollLines;
    }

    class CWebView : public IWebView
    {
    public:
        CWebView()
            :dirty_(false)
            ,width_(0)
            ,height_(0)
            ,gfxContext_(NULL)
        {
            WebCore::Page::PageClients pageClients;
            pageClients.chromeClient = new ChromeClient(this);
            pageClients.contextMenuClient = new ContextMenuClient;
            pageClients.inspectorClient = new InspectorClient;
            pageClients.editorClient = new EditorClient;
            pageClients.dragClient = new DragClient;

            page_ = adoptPtr(new WebCore::Page(pageClients));
            WebCore::Settings* settings = page_->settings();
            settings->setMinimumFontSize(1);
            settings->setMinimumLogicalFontSize(9);
            settings->setDefaultFontSize(16);
            settings->setDefaultFixedFontSize(13);
            settings->setJavaScriptEnabled(true);
            settings->setPluginsEnabled(true);
            settings->setLoadsImagesAutomatically(true);
            settings->setDefaultTextEncodingName(icuwin_getDefaultEncoding());

            FrameLoaderClient* loader = new FrameLoaderClient(this, page_.get());
            mainFrame_ = WebCore::Frame::create(page_.get(), NULL, loader).get();
            loader->setFrame(mainFrame_);
            mainFrame_->init();

            hdc_ = adoptPtr(CreateCompatibleDC(0));
            resize(100, 100);
        }

        virtual ~CWebView()
        {
            delete gfxContext_;
            mainFrame_->loader()->detachFromParent();
        }

        virtual void destroy()
        {
            delete this;
        }

        virtual void loadURL(const utf8* inUrl)
        {
            WebCore::KURL url(WebCore::KURL(), inUrl, WebCore::UTF8Encoding());
            if (!url.isValid())
                url.setProtocol("http:");

            if (!url.isValid())
                return;

            WebCore::ResourceRequest request(url);
            request.setCachePolicy(WebCore::UseProtocolCachePolicy);
            request.setTimeoutInterval(60.f);
            request.setHTTPMethod("GET");
            mainFrame_->loader()->load(request, false);
        }

        virtual void loadURL(const wchar_t* inUrl)
        {
            String url(inUrl, wcslen(inUrl));
            loadURL(url.utf8().data());
        }

        virtual void loadHTML(const utf8* html)
        {
            String mime = "text/html";

            RefPtr<WebCore::SharedBuffer> sharedBuffer = WebCore::SharedBuffer::create(html, strlen(html));

            WebCore::KURL url(WebCore::KURL(), "");
            WebCore::ResourceRequest request(url);
            WebCore::SubstituteData substituteData(sharedBuffer.release(), mime, WebCore::UTF8Encoding().name(), url);

            mainFrame_->loader()->load(request, substituteData, false);
        }

        virtual void loadHTML(const wchar_t* html)
        {
            String mime = "text/html";

            RefPtr<WebCore::SharedBuffer> sharedBuffer = WebCore::SharedBuffer::create((const char*)html, wcslen(html)*2);

            WebCore::KURL url(WebCore::KURL(), "");
            WebCore::ResourceRequest request(url);
            WebCore::SubstituteData substituteData(sharedBuffer.release(), mime, WebCore::UTF16LittleEndianEncoding().name(), url);

            mainFrame_->loader()->load(request, substituteData, false);
        }

        virtual bool isLoading() const
        {
            bool loading = false;
            if (mainFrame()->loader()->documentLoader())
                loading = mainFrame()->loader()->documentLoader()->isLoadingInAPISense();

            if (loading)
                return true;

            if (mainFrame()->loader()->provisionalDocumentLoader())
                loading = mainFrame()->loader()->provisionalDocumentLoader()->isLoadingInAPISense();

            return loading;
        }

        virtual void stopLoading()
        {
            mainFrame()->loader()->stopAllLoaders();
        }

        virtual void reload()
        {
            mainFrame()->loader()->reload();
        }

        const utf8* title()
        {
            if (mainFrame()->loader()->documentLoader())
            {
                title_ = mainFrame()->loader()->documentLoader()->title().string().utf8();
            }
            else
            {
                title_ = WTF::CString("notitle");
            }

            return title_.data();
        }

        virtual void resize(int w, int h)
        {
            if (w != width_ || h != height_)
            {
                mainFrame_->view()->resize(w, h);

                width_ = w;
                height_ = h;

                BITMAPINFO bmp;
                memset(&bmp, 0, sizeof(bmp));
                bmp.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bmp.bmiHeader.biWidth = w;
                bmp.bmiHeader.biHeight = -h;
                bmp.bmiHeader.biPlanes = 1;
                bmp.bmiHeader.biBitCount = 32;
                bmp.bmiHeader.biCompression = BI_RGB;

                HBITMAP hbmp = ::CreateDIBSection(0, &bmp, DIB_RGB_COLORS, &pixels_, NULL, 0);
                SelectObject(hdc_.get(), hbmp);
                hbmp_ = adoptPtr(hbmp);

                delete gfxContext_;
                gfxContext_ = new WebCore::GraphicsContext(hdc_.get());

                dirtyArea_ = WebCore::IntRect(0, 0, w, h);
                setDirty(true);
            }
        }

        virtual int width() const { return width_; }
        virtual int height() const { return height_; }

        virtual void setDirty(bool dirty)
        {
            dirty_ = dirty;
        }

        virtual bool isDirty() const
        {
            return dirty_;
        }

        virtual void addDirtyArea(int x, int y, int w, int h)
        {
            if (w > 0 && h > 0)
            {
                dirtyArea_.unite(WebCore::IntRect(x, y, w, h));
                dirty_ = true;
            }
        }

        virtual void layoutIfNeeded()
        {
            if (mainFrame_->view())
                mainFrame_->view()->updateLayoutAndStyleIfNeededRecursive();
        }

        virtual void paint(void* dst, int pitch)
        {
            layoutIfNeeded();

            gfxContext_->save();
            gfxContext_->clip(dirtyArea_);

            mainFrame_->view()->paint(gfxContext_, dirtyArea_);

            gfxContext_->restore();

            dirty_ = false;
            dirtyArea_ = WebCore::IntRect(0, 0, 0, 0);

            if (pitch == 0 || pitch == width_*4)
            {
                memcpy(dst, pixels_, width_*height_*4);
            }
            else 
            {
                unsigned char* pixels = (unsigned char*)pixels_; 
                unsigned char* dst_pixels = (unsigned char*)dst; 
                for(int i = 0; i < height_; ++i) 
                {
                    memcpy(dst_pixels, pixels, width_*4);
                    pixels += width_*4;
                    dst_pixels += pitch;
                }
            }
        }

        virtual bool canGoBack() const
        {
            return page()->backForwardList()->backItem() && !page()->defersLoading();
        }

        virtual bool goBack()
        {
            return page()->goBack();
        }

        virtual bool canGoForward() const
        {
            return page()->backForwardList()->forwardItem() && !page()->defersLoading();
        }

        virtual bool goForward()
        {
            page()->goForward();
            return true;
        }

        virtual void selectAll()
        {
            mainFrame()->editor()->command("SelectAll").execute();
        }

        virtual void copy()
        {
            page()->focusController()->focusedOrMainFrame()->editor()->command("Copy").execute();
        }

        virtual void cut()
        {
            page()->focusController()->focusedOrMainFrame()->editor()->command("Cut").execute();
        }

        virtual void paste()
        {
            page()->focusController()->focusedOrMainFrame()->editor()->command("Paste").execute();
        }

        virtual void delete_()
        {
            page()->focusController()->focusedOrMainFrame()->editor()->command("Delete").execute();
        }

        virtual void setCookieEnabled(bool enable)
        {
            if (page_)
                page_->setCookieEnabled(enable);
        }

        virtual bool cookieEnabled() const
        {
            if (page_)
                return page_->cookieEnabled();
                
            return true;
        }

        virtual void setMediaVolume(float volume)
        {
            if (page_)
                page_->setMediaVolume(volume);
        }

        virtual float mediaVolume() const
        {
            if (page_)
                return page_->mediaVolume();

            return 1.f;
        }

        static WebCore::MouseEventType messageToEventType(unsigned int message)
        {
            switch (message) 
            {
            case WM_LBUTTONDBLCLK:
            case WM_RBUTTONDBLCLK:
            case WM_MBUTTONDBLCLK:
            //MSDN docs say double click is sent on mouse down
            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN:
                return WebCore::MouseEventPressed;

            case WM_LBUTTONUP:
            case WM_RBUTTONUP:
            case WM_MBUTTONUP:
                return WebCore::MouseEventReleased;

            case WM_MOUSELEAVE:
            case WM_MOUSEMOVE:
                return WebCore::MouseEventMoved;

            default:
                ASSERT_NOT_REACHED();
                //Move is relatively harmless
                return WebCore::MouseEventMoved;
            }
        }

        static WebCore::MouseButton messageToButtonType(unsigned int message, unsigned int wParam)
        {
            switch (message) 
            {
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_LBUTTONDBLCLK:
                return WebCore::LeftButton;

            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_RBUTTONDBLCLK:
                return WebCore::RightButton;

            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
            case WM_MBUTTONDBLCLK:
                return WebCore::MiddleButton;

            case WM_MOUSEMOVE:
            case WM_MOUSELEAVE:
                if (wParam & MK_LBUTTON)
                    return WebCore::LeftButton;

                if (wParam & MK_MBUTTON)
                    return WebCore::MiddleButton;

                if (wParam & MK_RBUTTON)
                    return WebCore::RightButton;
                
                return WebCore::NoButton;

            default:
                ASSERT_NOT_REACHED();
                return WebCore::NoButton;
            }
        }

        virtual bool mouseEvent(unsigned int message, unsigned int wParam, int x, int y, int globalX, int globalY)
        {
            static LONG globalClickCount;
            static WebCore::IntPoint globalPrevPoint;
            static WebCore::MouseButton globalPrevButton;
            static LONG globalPrevMouseDownTime;

            if (message == WM_CANCELMODE) {
                mainFrame()->eventHandler()->lostMouseCapture();
                return true;
            }

            // Create our event.
            // On WM_MOUSELEAVE we need to create a mouseout event, so we force the position
            // of the event to be at (MINSHORT, MINSHORT).
            if (message == WM_MOUSELEAVE)
            {
                x = MINSHORT;
                y = MINSHORT;
            }

            WebCore::IntPoint pos(x, y);
            WebCore::IntPoint globalPos(globalX, globalY);

            WebCore::MouseButton button = messageToButtonType(message, wParam);
            WebCore::MouseEventType eventType = messageToEventType(message);

            int clickCount = 0;
            bool shift = wParam & MK_SHIFT;
            bool ctrl = wParam & MK_CONTROL;
            bool alt = GetKeyState(VK_MENU) & 0x8000;
            bool meta = alt;
            double timestamp = ::GetTickCount()*0.001;

            WebCore::PlatformMouseEvent mouseEvent(pos, globalPos, button, eventType, clickCount, shift, ctrl, alt, meta, timestamp);

            bool insideThreshold = abs(globalPrevPoint.x() - mouseEvent.pos().x()) < ::GetSystemMetrics(SM_CXDOUBLECLK) &&
                abs(globalPrevPoint.y() - mouseEvent.pos().y()) < ::GetSystemMetrics(SM_CYDOUBLECLK);
            LONG messageTime = ::GetMessageTime();

            bool handled = false;

            if (message == WM_LBUTTONDOWN || message == WM_MBUTTONDOWN || message == WM_RBUTTONDOWN) {
                if (((messageTime - globalPrevMouseDownTime) < (LONG)::GetDoubleClickTime()) && 
                    insideThreshold &&
                    mouseEvent.button() == globalPrevButton)
                    globalClickCount++;
                else
                    // Reset the click count.
                    globalClickCount = 1;
                globalPrevMouseDownTime = messageTime;
                globalPrevButton = mouseEvent.button();
                globalPrevPoint = mouseEvent.pos();

                mouseEvent.setClickCount(globalClickCount);

                page()->focusController()->setActive(true);
                handled = mainFrame()->eventHandler()->handleMousePressEvent(mouseEvent);
            } else if (message == WM_LBUTTONDBLCLK || message == WM_MBUTTONDBLCLK || message == WM_RBUTTONDBLCLK) {
                globalClickCount++;
                mouseEvent.setClickCount(globalClickCount);

                page()->focusController()->setActive(true);
                handled = mainFrame()->eventHandler()->handleMousePressEvent(mouseEvent);
            } else if (message == WM_LBUTTONUP || message == WM_MBUTTONUP || message == WM_RBUTTONUP) {
                // Record the global position and the button of the up.
                globalPrevButton = mouseEvent.button();
                globalPrevPoint = mouseEvent.pos();
                mouseEvent.setClickCount(globalClickCount);
                mainFrame()->eventHandler()->handleMouseReleaseEvent(mouseEvent);
            } else if (message == WM_MOUSELEAVE) {
                mainFrame()->eventHandler()->mouseMoved(mouseEvent);
                handled = true;
            } else if (message == WM_MOUSEMOVE) {
                if (!insideThreshold)
                    globalClickCount = 0;
                mouseEvent.setClickCount(globalClickCount);
                handled = mainFrame()->eventHandler()->mouseMoved(mouseEvent);
            }
            return handled;
        }

        bool mouseWheel(WPARAM wParam, int x, int y, int globalX, int globalY)
        {
            WebCore::IntPoint pos(x, y);
            WebCore::IntPoint globalPos(globalX, globalY);

            static const float cScrollbarPixelsPerLine = 100.0f / 3.0f;
            float delta = GET_WHEEL_DELTA_WPARAM(wParam) / static_cast<float>(WHEEL_DELTA);
            float wheelTicksX = 0.f;
            float wheelTicksY = delta;

            float deltaX = 0.f;
            float deltaY = 0.f;

            bool shiftKey = wParam & MK_SHIFT;
            bool ctrlKey = wParam & MK_CONTROL;
            bool altKey = GetKeyState(VK_MENU) & 0x8000;

            WebCore::PlatformWheelEventGranularity granularity = WebCore::ScrollByPageWheelEvent;

            if (shiftKey) {
                deltaX = delta * static_cast<float>(horizontalScrollChars()) * cScrollbarPixelsPerLine;
                deltaY = 0;
                granularity = WebCore::ScrollByPixelWheelEvent;
            } else {
                deltaX = 0;
                deltaY = delta;
                int verticalMultiplier = verticalScrollLines();
                granularity = (verticalMultiplier == WHEEL_PAGESCROLL) ? WebCore::ScrollByPageWheelEvent : WebCore::ScrollByPixelWheelEvent;
                if (granularity == WebCore::ScrollByPixelWheelEvent)
                    deltaY *= static_cast<float>(verticalMultiplier) * cScrollbarPixelsPerLine;
            }

            WebCore::PlatformWheelEvent wheelEvent(pos, globalPos, deltaX, deltaY, wheelTicksX, wheelTicksY, granularity, shiftKey, ctrlKey, altKey, altKey);
            return mainFrame()->eventHandler()->handleWheelEvent(wheelEvent);
        }

        virtual bool keyUp(unsigned int virtualKeyCode, int keyData, bool systemKey)
        {
            WebCore::PlatformKeyboardEvent keyEvent(0, virtualKeyCode, keyData, WebCore::PlatformKeyboardEvent::KeyUp, systemKey);

            WebCore::Frame* frame = page()->focusController()->focusedOrMainFrame();
            return frame->eventHandler()->keyEvent(keyEvent);
        }

        bool keyDown(WPARAM virtualKeyCode, LPARAM keyData, bool systemKey)
        {
            WebCore::Frame* frame = page()->focusController()->focusedOrMainFrame();

            WebCore::PlatformKeyboardEvent keyEvent(0, virtualKeyCode, keyData, WebCore::PlatformKeyboardEvent::RawKeyDown, systemKey);
            bool handled = frame->eventHandler()->keyEvent(keyEvent);

            // These events cannot be canceled, and we have no default handling for them.
            // FIXME: match IE list more closely, see <http://msdn2.microsoft.com/en-us/library/ms536938.aspx>.
            if (systemKey && virtualKeyCode != VK_RETURN)
                return false;

            if (handled) {
                MSG msg;
                ::PeekMessage(&msg, NULL, WM_CHAR, WM_CHAR, PM_REMOVE);
                return true;
            }

            // We need to handle back/forward using either Ctrl+Left/Right Arrow keys.
            // FIXME: This logic should probably be in EventHandler::defaultArrowEventHandler().
            // FIXME: Should check that other modifiers aren't pressed.
            if (virtualKeyCode == VK_RIGHT && keyEvent.ctrlKey())
                return page()->goForward();
            if (virtualKeyCode == VK_LEFT && keyEvent.ctrlKey())
                return page()->goBack();

            // Need to scroll the page if the arrow keys, pgup/dn, or home/end are hit.
            WebCore::ScrollDirection direction;
            WebCore::ScrollGranularity granularity;
            switch (virtualKeyCode) {
                case VK_LEFT:
                    granularity = WebCore::ScrollByLine;
                    direction = WebCore::ScrollLeft;
                    break;
                case VK_RIGHT:
                    granularity = WebCore::ScrollByLine;
                    direction = WebCore::ScrollRight;
                    break;
                case VK_UP:
                    granularity = WebCore::ScrollByLine;
                    direction = WebCore::ScrollUp;
                    break;
                case VK_DOWN:
                    granularity = WebCore::ScrollByLine;
                    direction = WebCore::ScrollDown;
                    break;
                case VK_HOME:
                    granularity = WebCore::ScrollByDocument;
                    direction = WebCore::ScrollUp;
                    break;
                case VK_END:
                    granularity = WebCore::ScrollByDocument;
                    direction = WebCore::ScrollDown;
                    break;
                case VK_PRIOR:
                    granularity = WebCore::ScrollByPage;
                    direction = WebCore::ScrollUp;
                    break;
                case VK_NEXT:
                    granularity = WebCore::ScrollByPage;
                    direction = WebCore::ScrollDown;
                    break;
                default:
                    return false;
            }

            if (frame->eventHandler()->scrollRecursively(direction, granularity))
            {
                MSG msg;
                ::PeekMessage(&msg, NULL, WM_CHAR, WM_CHAR, PM_REMOVE);
                return true;
            }

            return false;
        }

        virtual bool keyPress(unsigned int charCode, int keyData, bool systemKey)
        {
            WebCore::Frame* frame = page()->focusController()->focusedOrMainFrame();

            WebCore::PlatformKeyboardEvent keyEvent(0, charCode, keyData, WebCore::PlatformKeyboardEvent::Char, systemKey);
            // IE does not dispatch keypress event for WM_SYSCHAR.
            if (systemKey)
                return frame->eventHandler()->handleAccessKey(keyEvent);
            
            return frame->eventHandler()->keyEvent(keyEvent);
        }

        virtual void onSetFocus()
        {
            WebCore::FocusController* focusController = page()->focusController();
            focusController->setFocused(true);
        }

        virtual void onKillFocus()
        {
            WebCore::FocusController* focusController = page()->focusController();
            focusController->setFocused(false);
        }

        virtual void getCaret(wkeRect& rect)
        {
            rect.x = rect.y = 0;
            rect.w = rect.h = 0;
            WebCore::Frame* targetFrame = page()->focusController()->focusedOrMainFrame();
            if (!targetFrame)
                return;

            WebCore::IntRect caret;
            if (RefPtr<WebCore::Range> range = targetFrame->selection()->selection().toNormalizedRange()) {
                WebCore::ExceptionCode ec = 0;
                RefPtr<WebCore::Range> tempRange = range->cloneRange(ec);
                caret = targetFrame->editor()->firstRectForRange(tempRange.get());
            }

            caret = targetFrame->view()->contentsToWindow(caret);
            rect.x = caret.x();
            rect.y = caret.y();
            rect.w = caret.width();
            rect.h = caret.height();
        }

        virtual void runJS(const utf8* script)
        {
            WTF::String string = WTF::String::fromUTF8(script);
            mainFrame_->script()->executeScript(string, true);
        }

        WebCore::Page* page() const { return page_.get(); }
        WebCore::Frame* mainFrame() const { return mainFrame_; }

    protected:
        OwnPtr<WebCore::Page> page_;
        WebCore::Frame* mainFrame_;

        int width_;
        int height_;
        
        bool dirty_;
        WebCore::IntRect dirtyArea_;

        WebCore::GraphicsContext* gfxContext_;
        OwnPtr<HDC> hdc_;
        OwnPtr<HBITMAP> hbmp_;
        void* pixels_;

        WTF::CString title_;
    };
}

WKE_API void wkeInit()
{
    CoInitialize(NULL);

    icuwin_init();
    JSC::initializeThreading();
    WTF::initializeMainThread();
    wke::PlatformStrategies::initialize();

    //WebCore::ResourceHandleManager::sharedInstance()->setCookieJarFileName("cookie.txt");
}

WKE_API void wkeShutdown()
{
    HWND hTimer = FindWindow(L"TimerWindowClass", NULL);
    if (hTimer)
    {
        MSG msg;
        while(PeekMessage(&msg, hTimer, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    WebCore::iconDatabase().close();
    WebCore::PageGroup::closeLocalStorage();

    CoUninitialize();
}

#define MAJOR_VERSION   (1)
#define MINOR_VERSION   (0)
#define WEBKIT_BUILD    (98096)

WKE_API unsigned int wkeVersion()
{
    return (MAJOR_VERSION << 8) + MINOR_VERSION;
}

WKE_API const utf8* wkeVersionString()
{
    static utf8 s_versionString[128];
    sprintf(s_versionString, "wke version %d.%02d\n"
                             "webkit build %d\n"
                             "build time %s\n",
                             MAJOR_VERSION, MINOR_VERSION,
                             WEBKIT_BUILD,
                             __TIMESTAMP__);

    return s_versionString;
}

WKE_API wkeWebView wkeCreateWebView()
{
    return new wke::CWebView();
}

WKE_API void wkeDestroy(wkeWebView webView)
{
    return webView->destroy();
}

WKE_API void wkeLoadURL(wkeWebView webView, const utf8* url)
{
    return webView->loadURL(url);
}

WKE_API void wkeLoadURL_Unicode(wkeWebView webView, const wchar_t* url)
{
    return webView->loadURL(url);
}

WKE_API void wkeLoadHTML(wkeWebView webView, const utf8* html)
{
    return webView->loadHTML(html);
}

WKE_API void wkeLoadHTML_Unicode(wkeWebView webView, const wchar_t* html)
{
    return webView->loadHTML(html);
}

WKE_API bool wkeIsLoading(wkeWebView webView)
{
    return webView->isLoading();
}

WKE_API void wkeStopLoading(wkeWebView webView)
{
    return webView->stopLoading();
}

WKE_API void wkeReload(wkeWebView webView)
{
    return webView->reload();
}

WKE_API const utf8* wkeTitle(wkeWebView webView)
{
    return webView->title();
}

WKE_API void wkeResize(wkeWebView webView, int w, int h)
{
    return webView->resize(w, h);
}

WKE_API int wkeWidth(wkeWebView webView)
{
    return webView->width();
}

WKE_API int wkeHeight(wkeWebView webView)
{
    return webView->height();
}

WKE_API void wkeSetDirty(wkeWebView webView, bool dirty)
{
    return webView->setDirty(dirty);
}

WKE_API bool wkeIsDirty(wkeWebView webView)
{
    return webView->isDirty();
}

WKE_API void wkeAddDirtyArea(wkeWebView webView, int x, int y, int w, int h)
{
    return webView->addDirtyArea(x, y, w, h);
}

WKE_API void wkeLayoutIfNeeded(wkeWebView webView)
{
    return webView->layoutIfNeeded();
}

WKE_API void wkePaint(wkeWebView webView, void* dst, int pitch)
{
    return webView->paint(dst, pitch);
}

WKE_API bool wkeCanGoBack(wkeWebView webView)
{
    return webView->canGoBack();
}

WKE_API bool wkeGoBack(wkeWebView webView)
{
    return webView->goBack();
}

WKE_API bool wkeCanGoForward(wkeWebView webView)
{
    return webView->canGoForward();
}

WKE_API bool wkeGoForward(wkeWebView webView)
{
    return webView->goForward();
}

WKE_API void wkeSelectAll(wkeWebView webView)
{
    return webView->selectAll();
}

WKE_API void wkeCopy(wkeWebView webView)
{
    return webView->copy();
}

WKE_API void wkeCut(wkeWebView webView)
{
    return webView->cut();
}

WKE_API void wkePaste(wkeWebView webView)
{
    return webView->paste();
}

WKE_API void wkeDelete(wkeWebView webView)
{
    return webView->delete_();
}

WKE_API void wkeSetCookieEnabled(wkeWebView webView, bool enable)
{
    return webView->setCookieEnabled(enable);
}

WKE_API bool wkeCookieEnabled(wkeWebView webView)
{
    return webView->cookieEnabled();
}

WKE_API void wkeSetMediaVolume(wkeWebView webView, float volume)
{
    return webView->setMediaVolume(volume);
}

WKE_API float wkeMediaVolume(wkeWebView webView)
{
    return webView->mediaVolume();
}

WKE_API bool wkeMouseEvent(wkeWebView webView, unsigned int message, unsigned int wParam, int x, int y, int globalX, int globalY)
{
    return webView->mouseEvent(message, wParam, x, y, globalX, globalY);
}

WKE_API bool wkeMouseWheel(wkeWebView webView, WPARAM wParam, int x, int y, int globalX, int globalY)
{
    return webView->mouseWheel(wParam, x, y, globalX, globalY);
}

WKE_API bool wkeKeyUp(wkeWebView webView, unsigned int virtualKeyCode, int keyData, bool systemKey)
{
    return webView->keyUp(virtualKeyCode, keyData, systemKey);
}

WKE_API bool wkeKeyDown(wkeWebView webView, WPARAM virtualKeyCode, LPARAM keyData, bool systemKey)
{
    return webView->keyDown(virtualKeyCode, keyData, systemKey);
}

WKE_API bool wkeKeyPress(wkeWebView webView, unsigned int charCode, int keyData, bool systemKey)
{
    return webView->keyPress(charCode, keyData, systemKey);
}

WKE_API void wkeOnSetFocus(wkeWebView webView)
{
    return webView->onSetFocus();
}

WKE_API void wkeOnKillFocus(wkeWebView webView)
{
    return webView->onKillFocus();
}

WKE_API void wkeGetCaret(wkeWebView webView, wkeRect* rect)
{
    return webView->getCaret(*rect);
}

WKE_API void wkeRunJS(wkeWebView webView, const utf8* script)
{
    return webView->runJS(script);
}




//FIXME: We should consider moving this to a new file for cross-project functionality
PassRefPtr<WebCore::SharedBuffer> loadResourceIntoBuffer(const char* name)
{
    return 0;
}

extern void __CFInitialize(void);

void init_libs()
{
    _putenv("WEBKIT_IGNORE_SSL_ERRORS=1");
    pthread_win32_process_attach_np ();
    __CFInitialize();
}

typedef void (__cdecl* _PVFV) ();
#pragma section(".CRT$XCG", long, read)
__declspec(allocate(".CRT$XCG")) _PVFV init_section[] = { init_libs };


extern "C" BOOL WINAPI CoreFoundationDllMain( HINSTANCE hInstance, DWORD dwReason, LPVOID pReserved );
STDAPI_(BOOL) DllMain( HMODULE hModule, DWORD  ul_reason_for_call, LPVOID /*lpReserved*/)
{
    BOOL ret = FALSE;
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            WebCore::setInstanceHandle(hModule);
            ret = TRUE;
            break;

        case DLL_PROCESS_DETACH:
            WebCore::RenderThemeWin::setWebKitIsBeingUnloaded();
            pthread_win32_thread_detach_np ();
            break;

        case DLL_THREAD_ATTACH:
            pthread_win32_thread_attach_np ();
            break;

        case DLL_THREAD_DETACH:
            pthread_win32_thread_detach_np ();
            break;
    }

    CoreFoundationDllMain(hModule, ul_reason_for_call, 0);
    return ret;
}