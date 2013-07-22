// Copyright � 2010-2013 The CefSharp Project. All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.

#include "Stdafx.h"

#include "Internals/JavascriptBinding/BindingHandler.h"
#include "ClientAdapter.h"
#include "Cef.h"
#include "StreamAdapter.h"
#include "DownloadAdapter.h"
#include "IWebBrowser.h"
#include "ILifeSpanHandler.h"
#include "ILoadHandler.h"
#include "IRequestHandler.h"
#include "IMenuHandler.h"
#include "IKeyboardHandler.h"
#include "IJsDialogHandler.h"

using namespace std;
using namespace CefSharp::Internals::JavascriptBinding;

namespace CefSharp
{
    namespace Internals
    {
        bool ClientAdapter::OnBeforePopup(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, const CefString& target_url,
            const CefString& target_frame_name, const CefPopupFeatures& popupFeatures, CefWindowInfo& windowInfo,
            CefRefPtr<CefClient>& client, CefBrowserSettings& settings, bool* no_javascript_access)
        {
            ILifeSpanHandler^ handler = _browserControl->LifeSpanHandler;

            if (handler == nullptr)
            {
                return false;
            }

            return handler->OnBeforePopup(_browserControl, StringUtils::ToClr(target_url),
                windowInfo.x, windowInfo.y, windowInfo.width, windowInfo.height);
        }

        void ClientAdapter::OnAfterCreated(CefRefPtr<CefBrowser> browser)
        {
            if (!browser->IsPopup())
            {
                _browserHwnd = browser->GetHost()->GetWindowHandle();
                _cefBrowser = browser;

                _browserControl->OnInitialized();
            }
        }

        void ClientAdapter::OnBeforeClose(CefRefPtr<CefBrowser> browser)
        {
            if (_browserHwnd == browser->GetHost()->GetWindowHandle())
            {
                ILifeSpanHandler^ handler = _browserControl->LifeSpanHandler;
                if (handler != nullptr)
                {
                    handler->OnBeforeClose(_browserControl);
                }

                _cefBrowser = nullptr;
            }
        }

        void ClientAdapter::OnAddressChange(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, const CefString& url)
        {
            if (frame->IsMain())
            {
                _browserControl->Uri = gcnew Uri(StringUtils::ToClr(url));
            }
        }

        // TODO: How is this done with CEF3?
        /*
        void ClientAdapter::OnContentsSizeChange(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int width, int height)
        {
        if (frame->IsMain())
        {
        _browserControl->ContentsWidth = width;
        _browserControl->ContentsHeight = height;
        }
        }
        */

        void ClientAdapter::OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title)
        {
            _browserControl->Title = StringUtils::ToClr(title);
        }

        bool ClientAdapter::OnTooltip(CefRefPtr<CefBrowser> browser, CefString& text)
        {
            String^ tooltip = StringUtils::ToClr(text);

            if (tooltip != _tooltip)
            {
                _tooltip = tooltip;
                _browserControl->TooltipText = _tooltip;
            }

            return true;
        }

        bool ClientAdapter::OnConsoleMessage(CefRefPtr<CefBrowser> browser, const CefString& message, const CefString& source, int line)
        {
            String^ messageStr = StringUtils::ToClr(message);
            String^ sourceStr = StringUtils::ToClr(source);
            _browserControl->OnConsoleMessage(messageStr, sourceStr, line);

            return true;
        }

        bool ClientAdapter::OnKeyEvent(CefRefPtr<CefBrowser> browser, const CefKeyEvent& event, CefEventHandle os_event)
        {
            IKeyboardHandler^ handler = _browserControl->KeyboardHandler;

            if (handler == nullptr)
            {
                return false;
            }

            // TODO: windows_key_code could possibly be the wrong choice here (the OnKeyEvent signature has changed since CEF1). The
            // other option would be native_key_code.
            return handler->OnKeyEvent(_browserControl, (KeyType) event.type, event.windows_key_code, event.modifiers, event.is_system_key);
        }

        void ClientAdapter::OnLoadStart(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame)
        {
            if (browser->IsPopup())
            {
                return;
            }

            AutoLock lock_scope(this);
            if (frame->IsMain())
            {
                _browserControl->SetNavState(true, false, false);
            }

            _browserControl->OnFrameLoadStart(StringUtils::ToClr(frame->GetURL()));
        }

        void ClientAdapter::OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int httpStatusCode)
        {
            if(browser->IsPopup())
            {
                return;
            }

            AutoLock lock_scope(this);
            if (frame->IsMain())
            {
                _browserControl->SetNavState(false, browser->CanGoBack(), browser->CanGoForward());
            }

            _browserControl->OnFrameLoadEnd(StringUtils::ToClr(frame->GetURL()));
        }

        void ClientAdapter::OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, ErrorCode errorCode, const CefString& errorText, const CefString& failedUrl)
        {
            ILoadHandler^ handler = _browserControl->LoadHandler;

            if (handler == nullptr)
            {
                return;
            }

            handler->OnLoadError(_browserControl, StringUtils::ToClr(failedUrl), errorCode, StringUtils::ToClr(errorText));
        }

        // TODO: Check how we can support this with CEF3.
        /*
        bool ClientAdapter::OnBeforeBrowse(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefRequest> request, NavType navType, bool isRedirect)
        {
        IRequestHandler^ handler = _browserControl->RequestHandler;
        if (handler == nullptr)
        {
        return false;
        }

        CefRequestWrapper^ wrapper = gcnew CefRequestWrapper(request);
        NavigationType navigationType = (NavigationType)navType;

        return handler->OnBeforeBrowse(_browserControl, wrapper, navigationType, isRedirect);
        }
        */

        // TOOD: Try to support with CEF3; seems quite difficult because the method signature has changed greatly with many parts
        // seemingly MIA...
        /*
        bool ClientAdapter::OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefRequest> request)
        {
        IRequestHandler^ handler = _browserControl->RequestHandler;

        if (handler == nullptr)
        {
        return false;
        }

        CefRequestWrapper^ wrapper = gcnew CefRequestWrapper(request);
        RequestResponse^ requestResponse = gcnew RequestResponse(wrapper);

        bool ret = handler->OnBeforeResourceLoad(_browserControl, requestResponse);

        if (requestResponse->Action == ResponseAction::Redirect)
        {
        // TODO: Not supported at the moment; there does not seem any obvious way to give a redirect back in an
        // OnBeforeResourceLoad() handler nowadays.
        //request.redirectUrl = StringUtils::ToNative(requestResponse->RedirectUrl);
        }
        else if (requestResponse->Action == ResponseAction::Respond)
        {
        CefRefPtr<StreamAdapter> adapter = new StreamAdapter(requestResponse->ResponseStream);

        resourceStream = CefStreamReader::CreateForHandler(static_cast<CefRefPtr<CefReadHandler>>(adapter));
        response->SetMimeType(StringUtils::ToNative(requestResponse->MimeType));
        response->SetStatus(requestResponse->StatusCode);
        response->SetStatusText(StringUtils::ToNative(requestResponse->StatusText));

        CefResponse::HeaderMap map;

        if (requestResponse->ResponseHeaders != nullptr)
        {
        for each (KeyValuePair<String^, String^>^ kvp in requestResponse->ResponseHeaders)
        {
        map.insert(pair<CefString,CefString>(StringUtils::ToNative(kvp->Key),StringUtils::ToNative(kvp->Value)));
        }
        }

        response->SetHeaderMap(map);
        }

        return ret;
        }
        */

        CefRefPtr<CefDownloadHandler> ClientAdapter::GetDownloadHandler()
        {
            IRequestHandler^ requestHandler = _browserControl->RequestHandler;
            if (requestHandler == nullptr)
            {
                return false;
            }

            IDownloadHandler^ downloadHandler;
            bool ret = requestHandler->GetDownloadHandler(_browserControl, downloadHandler);
            
            if (ret)
            {
                return new DownloadAdapter(downloadHandler);
            }
            else
            {
                return nullptr;
            }
        }

        bool ClientAdapter::GetAuthCredentials(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, bool isProxy,
            const CefString& host, int port, const CefString& realm, const CefString& scheme, CefRefPtr<CefAuthCallback> callback)
        {
            IRequestHandler^ handler = _browserControl->RequestHandler;
            if (handler == nullptr)
            {
                return false;
            }

            String^ usernameString = nullptr;
            String^ passwordString = nullptr;
            bool handled = handler->GetAuthCredentials(_browserControl, isProxy, StringUtils::ToClr(host), port, StringUtils::ToClr(realm), StringUtils::ToClr(scheme), usernameString, passwordString);

            if (handled)
            {
                CefString username;
                CefString password;

                if (usernameString != nullptr)
                {
                    username = StringUtils::ToNative(usernameString);
                }

                if (passwordString != nullptr)
                {
                    password = StringUtils::ToNative(passwordString);
                }

                callback->Continue(username, password);
            }
            else
            {
                // TOOD: Should we call Cancel() here or not? At first glance, I believe we should since there will otherwise be no
                // way to cancel the auth request from an IRequestHandler.
                callback->Cancel();
            }

            return handled;
        }

        // TODO: Investigate how we can support in CEF3.
        /*
        void ClientAdapter::OnResourceResponse(CefRefPtr<CefBrowser> browser, const CefString& url, CefRefPtr<CefResponse> response, CefRefPtr<CefContentFilter>& filter)
        {
        IRequestHandler^ handler = _browserControl->RequestHandler;
        if (handler == nullptr)
        {
        return;
        }

        WebHeaderCollection^ headers = gcnew WebHeaderCollection();
        CefResponse::HeaderMap map;
        response->GetHeaderMap(map);
        for (CefResponse::HeaderMap::iterator it = map.begin(); it != map.end(); ++it)
        {
        try
        {
        headers->Add(StringUtils::ToClr(it->first), StringUtils::ToClr(it->second));
        }
        catch (Exception ^ex)
        {
        // adding a header with invalid characters can cause an exception to be
        // thrown. we will drop those headers for now.
        // we could eventually use reflection to call headers->AddWithoutValidate().
        }
        }

        handler->OnResourceResponse(
        _browserControl,
        StringUtils::ToClr(url),
        response->GetStatus(),
        StringUtils::ToClr(response->GetStatusText()),
        StringUtils::ToClr(response->GetMimeType()),
        headers);
        }*/

        // TODO: Investigate how we can support in CEF3.
        /*
        void ClientAdapter::OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context)
        {
        for each(KeyValuePair<String^, Object^>^ kvp in CEF::GetBoundObjects())
        {
        BindingHandler::Bind(kvp->Key, kvp->Value, context->GetGlobal());
        }

        for each(KeyValuePair<String^, Object^>^ kvp in _browserControl->GetBoundObjects())
        {
        BindingHandler::Bind(kvp->Key, kvp->Value, context->GetGlobal());
        }
        }
        */

        // TODO: Investigate how we can support in CEF3.
        /*
        bool ClientAdapter::OnBeforeMenu(CefRefPtr<CefBrowser> browser, const CefMenuInfo& menuInfo)
        {
        IMenuHandler^ handler = _browserControl->MenuHandler;
        if (handler == nullptr)
        {
        return false;
        }

        return handler->OnBeforeMenu(_browserControl);
        }
        */

        void ClientAdapter::OnTakeFocus(CefRefPtr<CefBrowser> browser, bool next)
        {
            _browserControl->OnTakeFocus(next);
        }

        bool ClientAdapter::OnJSDialog(CefRefPtr<CefBrowser> browser, const CefString& origin_url, const CefString& accept_lang,
            JSDialogType dialog_type, const CefString& message_text, const CefString& default_prompt_text,
            CefRefPtr<CefJSDialogCallback> callback, bool& suppress_message)
        {
            IJsDialogHandler^ handler = _browserControl->JsDialogHandler;

            if (handler == nullptr)
            {
                return false;
            }

            bool result;
            bool handled;

            switch (dialog_type)
            {
            case JSDIALOGTYPE_ALERT:
                handled = handler->OnJSAlert(_browserControl, StringUtils::ToClr(origin_url), StringUtils::ToClr(message_text));
                break;

            case JSDIALOGTYPE_CONFIRM:
                handled = handler->OnJSConfirm(_browserControl, StringUtils::ToClr(origin_url), StringUtils::ToClr(message_text), result);
                callback->Continue(result, CefString());
                break;

            case JSDIALOGTYPE_PROMPT:
                String^ resultString = nullptr;
                result = handler->OnJSPrompt(_browserControl, StringUtils::ToClr(origin_url), StringUtils::ToClr(message_text),
                    StringUtils::ToClr(default_prompt_text), result, resultString);
                callback->Continue(result, StringUtils::ToNative(resultString));
                break;
            }

            // Unknown dialog type, so we return "not handled".
            return false;
        }
    }
}