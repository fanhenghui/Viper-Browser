#include "AdBlockManager.h"
#include "AuthDialog.h"
#include "AutoFill.h"
#include "AutoFillBridge.h"
#include "BrowserApplication.h"
#include "BrowserTabWidget.h"
#include "CommonUtil.h"
#include "ExtStorage.h"
#include "FaviconStoreBridge.h"
#include "MainWindow.h"
#include "SecurityManager.h"
#include "Settings.h"
#include "URL.h"
#include "UserScriptManager.h"
#include "WebDialog.h"
#include "WebHistory.h"
#include "WebPage.h"
#include "WebView.h"

#include <QAuthenticator>
#include <QFile>
#include <QMessageBox>
#include <QTimer>
#include <QWebChannel>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QtWebEngineCoreVersion>

#include <iostream>

WebPage::WebPage(QObject *parent) :
    QWebEnginePage(parent),
    m_history(new WebHistory(this)),
    m_mainFrameHost(),
    m_domainFilterStyle(),
    m_mainFrameAdBlockScript(),
    m_needInjectAdBlockScript(true)
{
    setupSlots();
}

WebPage::WebPage(QWebEngineProfile *profile, QObject *parent) :
    QWebEnginePage(profile, parent),
    m_history(new WebHistory(this)),
    m_mainFrameHost(),
    m_domainFilterStyle(),
    m_needInjectAdBlockScript(true)
{
    setupSlots();
}

WebPage::~WebPage()
{
}

void WebPage::setupSlots()
{
    QWebChannel *channel = new QWebChannel(this);
    channel->registerObject(QLatin1String("extStorage"), sBrowserApplication->getExtStorage());
    channel->registerObject(QLatin1String("autofill"), new AutoFillBridge(this));
    channel->registerObject(QLatin1String("favicons"), new FaviconStoreBridge(this));
    setWebChannel(channel, QWebEngineScript::ApplicationWorld);

    connect(this, &WebPage::authenticationRequired,      this, &WebPage::onAuthenticationRequired);
    connect(this, &WebPage::proxyAuthenticationRequired, this, &WebPage::onProxyAuthenticationRequired);
    connect(this, &WebPage::loadProgress,                this, &WebPage::onLoadProgress);
    connect(this, &WebPage::loadFinished,                this, &WebPage::onLoadFinished);
    connect(this, &WebPage::urlChanged,                  this, &WebPage::onMainFrameUrlChanged);
    connect(this, &WebPage::featurePermissionRequested,  this, &WebPage::onFeaturePermissionRequested);
    connect(this, &WebPage::renderProcessTerminated,     this, &WebPage::onRenderProcessTerminated);

#if (QTWEBENGINECORE_VERSION >= QT_VERSION_CHECK(5, 11, 0))
    connect(this, &WebPage::quotaRequested, this, &WebPage::onQuotaRequested);
    connect(this, &WebPage::registerProtocolHandlerRequested, this, &WebPage::onRegisterProtocolHandlerRequested);
#endif
}

WebHistory *WebPage::getHistory() const
{
    return m_history;
}

bool WebPage::acceptNavigationRequest(const QUrl &url, QWebEnginePage::NavigationType type, bool isMainFrame)
{
    // Check if special url such as "viper:print"
    if (url.scheme().compare(QLatin1String("viper")) == 0)
    {
        if (url.path().compare(QLatin1String("print")) == 0)
        {
            emit printPageRequest();
            return false;
        }
    }

    // Check if the request is for a PDF and try to render with PDF.js
    const QString urlString = url.toString(QUrl::FullyEncoded);
    if (urlString.endsWith(QLatin1String(".pdf")))
    {
        QFile resource(":/viewer.html");
        bool opened = resource.open(QIODevice::ReadOnly);
        if (opened)
        {
            QString data = QString::fromUtf8(resource.readAll().constData());
            int endTag = data.indexOf("</html>");
            data.insert(endTag - 1, QString("<script>window.Response = undefined; document.addEventListener(\"DOMContentLoaded\", function() {"
                                            " PDFJS.verbosity = PDFJS.VERBOSITY_LEVELS.info; "
                                            " window.PDFViewerApplication.open(\"%1\");});</script>").arg(urlString));
            QByteArray bytes;
            bytes.append(data);
            setHtml(bytes, url);
            return false;
        }
    }

    if (isMainFrame && type == QWebEnginePage::NavigationTypeLinkClicked)
    {
        // If only change in URL is fragment, try to update URL bar by emitting url changed signal
        if (this->url().toString(QUrl::RemoveFragment).compare(url.toString(QUrl::RemoveFragment)) == 0)
            emit urlChanged(url);
    }

    if (!QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame))
        return false;

    if (isMainFrame && type != QWebEnginePage::NavigationTypeReload)
    {
        QWebEngineScriptCollection &scriptCollection = scripts();
        scriptCollection.clear();
        auto pageScripts = sBrowserApplication->getUserScriptManager()->getAllScriptsFor(url);
        for (auto &script : pageScripts)
            scriptCollection.insert(script);
    }

    //TODO: if !isMainFrame, check the url and inject the AutoFill script if enabled & applicable

    return true;
}

void WebPage::runJavaScriptNonBlocking(const QString &scriptSource, QVariant &result)
{
    runJavaScript(scriptSource, [&](const QVariant &returnValue) {
        result.setValue(returnValue);
    });
}

QVariant WebPage::runJavaScriptBlocking(const QString &scriptSource)
{
    QEventLoop loop;
    QVariant result;

    runJavaScript(scriptSource, [&](const QVariant &returnValue){
        result = returnValue;
        loop.quit();
    });

    connect(this, &WebPage::destroyed, &loop, &QEventLoop::quit);

    loop.exec();
    return result;
}

void WebPage::javaScriptConsoleMessage(WebPage::JavaScriptConsoleMessageLevel level, const QString &message, int lineId, const QString &sourceId)
{
    QWebEnginePage::javaScriptConsoleMessage(level, message, lineId, sourceId);
    //std::cout << "[JS Console] [Source " << sourceId.toStdString() << "] Line " << lineId << ", message: " << message.toStdString() << std::endl;
}

bool WebPage::certificateError(const QWebEngineCertificateError &certificateError)
{
    return SecurityManager::instance().onCertificateError(certificateError, view()->window());
}

QWebEnginePage *WebPage::createWindow(QWebEnginePage::WebWindowType type)
{
    WebView *webView = qobject_cast<WebView*>(view());
    const bool isPrivate = profile() != QWebEngineProfile::defaultProfile();

    switch (type)
    {
        case QWebEnginePage::WebBrowserWindow:    // Open a new window
        {
            MainWindow *win = isPrivate ? sBrowserApplication->getNewPrivateWindow() : sBrowserApplication->getNewWindow();
            return win->getTabWidget()->currentWebWidget()->page();
        }
        case QWebEnginePage::WebBrowserBackgroundTab:
        case QWebEnginePage::WebBrowserTab:
        {
            // Get main window
            MainWindow *win = qobject_cast<MainWindow*>(webView->window());
            if (!win)
            {
                QObject *obj = webView->parent();
                while (obj->parent() != nullptr)
                    obj = obj->parent();

                win = qobject_cast<MainWindow*>(obj);

                if (!win)
                    return nullptr;
            }

            const bool switchToNewTab = (type == QWebEnginePage::WebBrowserTab
                                         && !sBrowserApplication->getSettings()->getValue(BrowserSetting::OpenAllTabsInBackground).toBool());

            WebWidget *webWidget = switchToNewTab ? win->getTabWidget()->newTab() : win->getTabWidget()->newBackgroundTab();
            return webWidget->page();
        }
        case QWebEnginePage::WebDialog:     // Open a web dialog
        {
            class WebDialog *dialog = new class WebDialog(isPrivate);
            dialog->show();
            return dialog->getView()->page();
        }
        default: break;
    }
    return nullptr;
}

void WebPage::onAuthenticationRequired(const QUrl &requestUrl, QAuthenticator *authenticator)
{
    AuthDialog authDialog(view()->window());
    authDialog.setMessage(tr("%1 is requesting your username and password for %2")
                          .arg(requestUrl.host()).arg(authenticator->realm()));

    if (authDialog.exec() == QDialog::Accepted)
    {
        authenticator->setUser(authDialog.getUsername());
        authenticator->setPassword(authDialog.getPassword());
    }
    else
    {
        *authenticator = QAuthenticator();
    }
}

void WebPage::onProxyAuthenticationRequired(const QUrl &/*requestUrl*/, QAuthenticator *authenticator, const QString &proxyHost)
{
    AuthDialog authDialog(view()->window());
    authDialog.setMessage(tr("Authentication is required to connect to proxy %1").arg(proxyHost));

    if (authDialog.exec() == QDialog::Accepted)
    {
        authenticator->setUser(authDialog.getUsername());
        authenticator->setPassword(authDialog.getPassword());
    }
    else
    {
        *authenticator = QAuthenticator();
    }
}

void WebPage::onFeaturePermissionRequested(const QUrl &securityOrigin, WebPage::Feature feature)
{
    QString featureStr = tr("Allow %1 to ");
    switch (feature)
    {
        case WebPage::Geolocation:
            featureStr.append("access your location?");
            break;
        case WebPage::MediaAudioCapture:
            featureStr.append("access your microphone?");
            break;
        case WebPage::MediaVideoCapture:
            featureStr = tr("access your webcam?");
            break;
        case WebPage::MediaAudioVideoCapture:
            featureStr = tr("access your microphone and webcam?");
            break;
        case WebPage::MouseLock:
            featureStr = tr("lock your mouse?");
            break;
#if (QTWEBENGINECORE_VERSION >= QT_VERSION_CHECK(5, 10, 0))
        case WebPage::DesktopVideoCapture:
            featureStr = tr("capture video of your desktop?");
            break;
        case WebPage::DesktopAudioVideoCapture:
            featureStr = tr("capture audio and video of your desktop?");
            break;
#endif
        default:
            featureStr = QString();
            break;
    }

    // Deny unknown features
    if (featureStr.isEmpty())
    {
        setFeaturePermission(securityOrigin, feature, PermissionDeniedByUser);
        return;
    }

    QString requestStr = featureStr.arg(securityOrigin.host());
    auto choice = QMessageBox::question(view()->window(), tr("Web Permission Request"), requestStr);
    PermissionPolicy policy = choice == QMessageBox::Yes ? PermissionGrantedByUser : PermissionDeniedByUser;
    setFeaturePermission(securityOrigin, feature, policy);
}

void WebPage::onMainFrameUrlChanged(const QUrl &url)
{
    URL urlCopy(url);
    QString urlHost = urlCopy.host().toLower();
    if (!urlHost.isEmpty() && m_mainFrameHost.compare(urlHost) != 0)
    {
        m_mainFrameHost = urlHost;
        m_domainFilterStyle = QString("<style>%1</style>").arg(AdBlockManager::instance().getDomainStylesheet(urlCopy));
        m_domainFilterStyle.replace("'", "\\'");
    }
}

void WebPage::onLoadProgress(int percent)
{
    if (percent > 0 && percent < 100 && m_needInjectAdBlockScript)
    {
        URL pageUrl(url());
        if (pageUrl.host().isEmpty())
            return;

        m_needInjectAdBlockScript = false;

        m_mainFrameAdBlockScript = AdBlockManager::instance().getDomainJavaScript(pageUrl);
        if (!m_mainFrameAdBlockScript.isEmpty())
            runJavaScript(m_mainFrameAdBlockScript, QWebEngineScript::ApplicationWorld);
    }

    //if (percent == 100)
    //    emit loadFinished(true);
}

void WebPage::onLoadFinished(bool ok)
{
    if (!ok)
        return;

    URL pageUrl(url());

    QString adBlockStylesheet = AdBlockManager::instance().getStylesheet(pageUrl);
    adBlockStylesheet.replace("'", "\\'");
    runJavaScript(QString("document.body.insertAdjacentHTML('beforeend', '%1');").arg(adBlockStylesheet));
    runJavaScript(QString("document.body.insertAdjacentHTML('beforeend', '%1');").arg(m_domainFilterStyle));

    if (m_needInjectAdBlockScript)
        m_mainFrameAdBlockScript = AdBlockManager::instance().getDomainJavaScript(pageUrl);

    if (!m_mainFrameAdBlockScript.isEmpty())
        runJavaScript(m_mainFrameAdBlockScript, QWebEngineScript::UserWorld);

    m_needInjectAdBlockScript = true;

    sBrowserApplication->getAutoFill()->onPageLoaded(this, url());
}

#if (QTWEBENGINECORE_VERSION >= QT_VERSION_CHECK(5, 11, 0))
void WebPage::onQuotaRequested(QWebEngineQuotaRequest quotaRequest)
{
    auto response = QMessageBox::question(view()->window(), tr("Permission Request"),
                                          tr("Allow %1 to increase its storage quota to %2?")
                                          .arg(quotaRequest.origin().host())
                                          .arg(CommonUtil::bytesToUserFriendlyStr(quotaRequest.requestedSize())));
    if (response == QMessageBox::Yes)
        quotaRequest.accept();
    else
        quotaRequest.reject();
}

void WebPage::onRegisterProtocolHandlerRequested(QWebEngineRegisterProtocolHandlerRequest request)
{
    //todo: save user response, add a "Site Preferences" UI where site permissions and such can be modified
    auto response = QMessageBox::question(view()->window(), tr("Permission Request"),
                                          tr("Allow %1 to open all %2 links?").arg(request.origin().host()).arg(request.scheme()));
    if (response == QMessageBox::Yes)
        request.accept();
    else
        request.reject();
}
#endif

void WebPage::onRenderProcessTerminated(RenderProcessTerminationStatus terminationStatus, int exitCode)
{
    if (terminationStatus == WebPage::NormalTerminationStatus)
        return;

    qDebug() << "Render process terminated with status " << static_cast<int>(terminationStatus)
             << ", exit code " << exitCode;

    QTimer::singleShot(50, this, &WebPage::showTabCrashedPage);
}

void WebPage::showTabCrashedPage()
{
    QFile resource(":/crash");
    if (resource.open(QIODevice::ReadOnly))
    {
        QString pageHtml = QString::fromUtf8(resource.readAll().constData()).arg(title());
        setHtml(pageHtml, url());
    }
}

void WebPage::injectUserJavaScript(ScriptInjectionTime injectionTime)
{
    // Attempt to get URL associated with frame
    QUrl pageUrl = url();
    if (pageUrl.isEmpty())
        return;

    QString userJS = sBrowserApplication->getUserScriptManager()->getScriptsFor(pageUrl, injectionTime, true);
    if (!userJS.isEmpty())
        runJavaScript(userJS);
}
