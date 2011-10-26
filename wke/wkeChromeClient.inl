
#include <WebCore/FileIconLoader.h>
#include <WebCore/Icon.h>

namespace wke
{
    class EmptyPopupMenu : public WebCore::PopupMenu {
    public:
        virtual void show(const WebCore::IntRect&, WebCore::FrameView*, int) {}
        virtual void hide() {}
        virtual void updateFromElement() {}
        virtual void disconnectClient() {}
    };

    class EmptySearchPopupMenu : public WebCore::SearchPopupMenu {
    public:
        virtual WebCore::PopupMenu* popupMenu() { return m_popup.get(); }
        virtual void saveRecentSearches(const AtomicString&, const Vector<String>&) {}
        virtual void loadRecentSearches(const AtomicString&, Vector<String>&) {}
        virtual bool enabled() { return false; }

    private:
        RefPtr<EmptyPopupMenu> m_popup;
    };

    class ChromeClient : public WebCore::ChromeClient
    {
    public:
        ChromeClient(IWebView* webView)
            :webView_(webView)
        {}

        virtual void chromeDestroyed() override
        {
            dbgMsg(L"frameLoaderDestroyed\n");
            delete this;
        }
        
        virtual void setWindowRect(const WebCore::FloatRect& rect) override
        {
            rect_ = rect;
        }

        virtual WebCore::FloatRect windowRect() override
        {
            return rect_;
        }

        virtual WebCore::FloatRect pageRect() override
        {
            return rect_;
        }
        
        virtual void focus() override
        {
        }

        virtual void unfocus() override
        {
        }

        virtual bool canTakeFocus(WebCore::FocusDirection) override
        {
            return true;
        }

        virtual void takeFocus(WebCore::FocusDirection) override
        {
        }

        virtual void focusedNodeChanged(WebCore::Node*) override
        {
        }

        void focusedFrameChanged(WebCore::Frame*) override
        {
        }

        virtual WebCore::Page* createWindow(WebCore::Frame*, const WebCore::FrameLoadRequest&, const WebCore::WindowFeatures&, const WebCore::NavigationAction&) override
        {
            dbgMsg(L"createWindow\n");
            return NULL;
        }

        virtual void show() override
        {
        }

        virtual bool canRunModal() override
        {
            return true;
        }

        virtual void runModal() override
        {
        }

        virtual void setToolbarsVisible(bool) override
        {
        }

        virtual bool toolbarsVisible() override
        {
            return false;
        }
        
        virtual void setStatusbarVisible(bool) override
        {
        }

        virtual bool statusbarVisible() override
        {
            return false;
        }
        
        virtual void setScrollbarsVisible(bool) override
        {
        }

        virtual bool scrollbarsVisible() override
        {
            return false;
        }
        
        virtual void setMenubarVisible(bool) override
        {
        }

        virtual bool menubarVisible() override
        {
            return false;
        }

        virtual void setResizable(bool) override
        {
        }
        
        virtual void addMessageToConsole(WebCore::MessageSource, WebCore::MessageType, WebCore::MessageLevel, const WTF::String& message, unsigned int lineNumber, const WTF::String& url) override
        {
            outputMsg(L"console message %s %d %s\n", CSTR(message), lineNumber, CSTR(url));
        }

        virtual bool canRunBeforeUnloadConfirmPanel() override
        {
            return true;
        }

        virtual bool runBeforeUnloadConfirmPanel(const WTF::String& message, WebCore::Frame* frame) override
        {
            return true;
        }

        virtual void closeWindowSoon() override
        {
        }
        
        virtual void runJavaScriptAlert(WebCore::Frame*, const WTF::String& msg) override
        {
            outputMsg(L"JavaScript Alert %s\n", CSTR(msg));
        }

        virtual bool runJavaScriptConfirm(WebCore::Frame*, const WTF::String& msg) override
        {
            outputMsg(L"JavaScript Confirm %s\n", CSTR(msg));
            return true;
        }

        virtual bool runJavaScriptPrompt(WebCore::Frame*, const WTF::String& msg, const WTF::String& defaultValue, WTF::String& result) override
        {
            outputMsg(L"JavaScript Prompt %s %s\n", CSTR(msg), CSTR(defaultValue));
            return true;
        }

        virtual void setStatusbarText(const WTF::String& text) override
        {
            dbgMsg(L"setStatusbarText %s\n", CSTR(text));
        }

        virtual bool shouldInterruptJavaScript() override
        {
            return false;
        }

        WebCore::KeyboardUIMode keyboardUIMode() override
        {
            return WebCore::KeyboardAccessDefault;
        }

        virtual WebCore::IntRect windowResizerRect() const override
        {
            return WebCore::IntRect();
        }

        virtual void invalidateWindow(const WebCore::IntRect& rect, bool immediate) override
        {
            webView_->addDirtyArea(rect.x(), rect.y(), rect.width(), rect.height());
            //dbgMsg(L"invalidateWindow\n");
        }

        virtual void invalidateContentsAndWindow(const WebCore::IntRect& rect, bool immediate) override
        {
            webView_->addDirtyArea(rect.x(), rect.y(), rect.width(), rect.height());
            //dbgMsg(L"invalidateContentsAndWindow\n");
        }

        virtual void invalidateContentsForSlowScroll(const WebCore::IntRect& rect, bool immediate) override
        {
            webView_->addDirtyArea(rect.x(), rect.y(), rect.width(), rect.height());
            //dbgMsg(L"invalidateContentsForSlowScroll\n");
        }

        virtual void scroll(const WebCore::IntSize&, const WebCore::IntRect&, const WebCore::IntRect&) override
        {
        }

        virtual WebCore::IntPoint screenToWindow(const WebCore::IntPoint& pt) const override
        {
            return pt;
        }

        virtual WebCore::IntRect windowToScreen(const WebCore::IntRect& pt) const override
        {
            return pt;
        }

        virtual PlatformPageClient platformPageClient() const override
        {
            dbgMsg(L"platformPageClient\n");
            
            //test...
            return GetDesktopWindow();
        }

        virtual void scrollbarsModeDidChange() const override
        {
        }

        virtual void setCursor(const WebCore::Cursor& cursor) override
        {
            HCURSOR platformCursor = cursor.platformCursor()->nativeCursor();
            if (!platformCursor)
                return;

            ::SetCursor(platformCursor);
        }

        virtual void setCursorHiddenUntilMouseMoves(bool) override
        {
        }

        virtual void contentsSizeChanged(WebCore::Frame*, const WebCore::IntSize&) const override
        {
        }

        virtual void mouseDidMoveOverElement(const WebCore::HitTestResult&, unsigned modifierFlags) override
        {
        }

        virtual void setToolTip(const WTF::String& toolTip, WebCore::TextDirection) override
        {
            if (!toolTip.isEmpty())
            {
                dbgMsg(L"setToolTip %s\n", CSTR(toolTip));
            }
        }

        virtual void print(WebCore::Frame*) override
        {
        }

#if ENABLE(SQL_DATABASE)
        virtual void exceededDatabaseQuota(WebCore::Frame*, const WTF::String& databaseName) override
        {
        }
#endif

        virtual void reachedMaxAppCacheSize(int64_t spaceNeeded) override
        {
        }

        virtual void reachedApplicationCacheOriginQuota(WebCore::SecurityOrigin*, int64_t totalSpaceNeeded) override
        {
        }

#if ENABLE(CONTEXT_MENUS)
        virtual void showContextMenu() override { }
#endif

#if ENABLE(NOTIFICATIONS)
        virtual WebCore::NotificationPresenter* notificationPresenter() override
        {
            return 0;
        }
#endif
        virtual void requestGeolocationPermissionForFrame(WebCore::Frame*, WebCore::Geolocation*) override
        {
        }

        virtual void cancelGeolocationPermissionRequestForFrame(WebCore::Frame*, WebCore::Geolocation*) override
        {
        }

        virtual void runOpenPanel(WebCore::Frame*, PassRefPtr<WebCore::FileChooser>) override
        {
        }

        virtual void loadIconForFiles(const Vector<WTF::String>& filenames, WebCore::FileIconLoader* loader) override
        {
            loader->notifyFinished(WebCore::Icon::createIconForFiles(filenames));
        }

        virtual void formStateDidChange(const WebCore::Node*) override
        {
        }

#if USE(ACCELERATED_COMPOSITING)
        virtual void attachRootGraphicsLayer(WebCore::Frame*, WebCore::GraphicsLayer*) override
        {
        }

        virtual void setNeedsOneShotDrawingSynchronization() override
        {
        }

        virtual void scheduleCompositingLayerSync() override
        {
        }
#endif


#if PLATFORM(WIN)
        virtual void setLastSetCursorToCurrentCursor() override
        {
        }
#endif

        virtual bool selectItemWritingDirectionIsNatural() override
        {
            return true;
        }

        bool selectItemAlignmentFollowsMenuWritingDirection() override
        {
            return false;
        }

        virtual PassRefPtr<WebCore::PopupMenu> createPopupMenu(WebCore::PopupMenuClient* client) const override
        {
            return adoptRef(new EmptyPopupMenu);
        }

        virtual PassRefPtr<WebCore::SearchPopupMenu> createSearchPopupMenu(WebCore::PopupMenuClient* client) const override
        {
            return adoptRef(new EmptySearchPopupMenu);
        }

        virtual bool shouldRubberBandInDirection(WebCore::ScrollDirection) const override 
        { 
            return true; 
        }

        virtual void numWheelEventHandlersChanged(unsigned) override 
        { 
        }

        virtual void* webView() const override { return 0; }

    protected:
        WebCore::FloatRect rect_;
        IWebView* webView_;
    };
}
