#include "MoPubView.hpp"

#include <QUuid>
#include <QtLocationSubset/QGeoPositionInfo>

#include <math.h>

#include <bb/Application>
#include <bb/cascades/Container>
#include <bb/cascades/ScrollView>
#include <bb/cascades/WebView>
#include <bb/cascades/WebNavigationRequest>
#include <bb/cascades/WebSettings>
#include <bb/cascades/DockLayout>
#include <bb/system/InvokeManager>
#include <bb/system/InvokeRequest>
#include <bb/device/HardwareInfo>
#include <bb/device/DeviceInfo>
#include <bb/PackageInfo>
#include <bb/location/PositionErrorCode>


using namespace QtMobilitySubset;
using namespace bb;
using namespace bb::cascades;
using namespace bb::system;
using namespace bb::device;

const QString MoPubView::SDK_VERSION = QString("1.9.0.8");
const QString MoPubView::API_VERSION = QString("8");
#if TESTING
const QString MoPubView::MOPUB_URL = QString("http://testing.ads.mopub.com");
#else
const QString MoPubView::MOPUB_URL = QString("http://ads.mopub.com");
#endif
const QString MoPubView::AD_HANDLER = QString("/m/ad");
const QString MoPubView::IMPRESSION_HANDLER = QString ("/m/imp");
const QString MoPubView::CONVERSION_HANDLER = QString("/m/open");
const int MoPubView::MINIMUM_REFRESH_TIME_MILLISECONDS = 10000;
const int MoPubView::MAXIMUM_REFRESH_TIME_MILLISECONDS = 60000;
const double MoPubView::EXPONENTIAL_BACKOFF_FACTOR = 1.5;

MoPubView::MoPubView()
: mControlContainer(0)
, mScrollView(0)
, mAdView(0)
, mInvokeManager(new InvokeManager(this))
, mNetworkAccessManager(new QNetworkAccessManager(this))
, mPositionSource(QGeoPositionInfoSource::createDefaultSource(this))
, mHardwareInfo(new HardwareInfo(this))
, mDeviceInfo(new DeviceInfo(this))
, mAutoAdRefreshTimer(new QTimer(this))
, mPackageInfo(new PackageInfo(this))
{
    mControlContainer = Container::create();
    mAdView = WebView::create();
    mScrollView = ScrollView::create();
    mScrollView->setContent(mAdView);
    mControlContainer->add(mScrollView);

    bool res = connect(mAdView, SIGNAL(navigationRequested(bb::cascades::WebNavigationRequest*)),
            this, SLOT(onNavigationRequested(bb::cascades::WebNavigationRequest*)));
    Q_ASSERT(res);

    Application* app = Application::instance();
    res = connect(app, SIGNAL(asleep()),
            this, SLOT(cancelRefreshTimer()));
    Q_ASSERT(res);
    res = connect(app, SIGNAL(awake()),
            this, SLOT(scheduleRefreshTimerIfEnabled()));
    Q_ASSERT(res);

    mRefreshTimeMilliseconds = 60000;
    mAutoRefreshEnabled = true;
    res = connect(mAutoAdRefreshTimer, SIGNAL(timeout()), this, SLOT(loadAd()));
    Q_ASSERT(res);
    Q_UNUSED(res);
    setRoot(mControlContainer);
}

void MoPubView::setWidth(int value) {
    mWidth = value;
    mAdView->setPreferredWidth(mWidth);
}
void MoPubView::setHeight(int value) {
    mHeight = value;
    mAdView->setPreferredHeight(mHeight);
}
QString MoPubView::adHtml() {return mAdView->html();}

void MoPubView::loadAd(){

    if (mIsLoading) {
        qDebug() << "Already loading an ad for " + mAdUnitId + ", wait to finish.";
        return;
    }

    if (mAdUnitId.isEmpty()){
        qDebug() << "Can't load an ad in this ad view because the ad unit ID is null. " << "Did you forget to call setAdUnitId()?";
        return;
    }

    if (!(mNetworkAccessManager->networkAccessible())){
        qDebug() << "Can't load an ad because there is no network connectivity.";
        scheduleRefreshTimerIfEnabled();
        return;
    }

    mFailUrl = QUrl();
    mIsLoading = true;

    mUrl = generateAdUrl();
    qDebug() <<  "Fetch Ad for " << mUrl;
    emit adWillLoad(mUrl);
    fetchAd();
}

QString MoPubView::createMoPubAPIUrl(QString handlerPart){
    QString urlString;
    urlString.append(MOPUB_URL + handlerPart);
    urlString.append("?v=" + API_VERSION);
    urlString.append("&id=" + mAdUnitId );
    return urlString;
}

QString MoPubView::getUdid(){
    QByteArray hash = QCryptographicHash::hash(mHardwareInfo->imei().toUtf8(),QCryptographicHash::Sha1);
    return "&udid=sha1imei:bb10" + hash.toHex();
}

QUrl MoPubView::generateAdUrl(){
    QString urlString = createMoPubAPIUrl(AD_HANDLER);
    urlString.append("&nv=" + SDK_VERSION);
    urlString.append(getUdid());

    QGeoPositionInfo loc = mPositionSource->lastKnownPosition();
    if (loc.isValid()) {
        urlString.append("&ll=" + QString().setNum(loc.coordinate().latitude()) + "," + QString().setNum(loc.coordinate().longitude()));
    }

    QString timeZone = getTimeZone();
    if (!timeZone.isEmpty()){
        urlString.append("&z=" + timeZone);
    }

    urlString.append("&o=" + getOrientation());

    bool mraid = false;
    if (mraid) urlString.append("&mr=1");
    return urlString;
}

QString MoPubView::getOrientation(){
    switch (mDeviceInfo->orientation()) {
    case DeviceOrientation::LeftUp:
    case DeviceOrientation::RightUp:
        return QString("l");
    default:
        return QString("p");
    }
}

QString MoPubView::getTimeZone(){
    return QString().setNum(QDateTime::currentDateTime().utcOffset());
}

void MoPubView::onNavigationRequested(bb::cascades::WebNavigationRequest* request){
    Q_CHECK_PTR(request);

    qDebug() << "onNavigationRequested url: " << request->url();
    QString scheme = request->url().scheme();
    QUrl url = request->url();

    // If the URL being loaded shares the redirectUrl prefix, open it in the browser.
    if (!mRedirectUrl.isEmpty()  && url.toString().startsWith(mRedirectUrl.toString())) {
        addClickTrackingRedirect(url);
        mIsLoading = false;
        if (!mInterceptslinks) {
            showBrowserForUrl(url);
            request->ignore();
        }
        return;
    }

    // Handle the special mopub:// scheme calls.
    if (scheme == "mopub") {
        QString host = request->url().host().toLower();
        if (host == "finishload"){      qDebug() << "emit finishload";      emitAdDidLoad(); }
        else if (host == "close"){      qDebug() << "emit close";           emit adDidClose(); }
        else if (host == "failload"){   qDebug() << "failload loadFailUrl"; loadFailUrl(); }
        else if (host == "custom"){     qDebug() << "mopub custom";         invokeUrl(url); }
        request->ignore();
    }
    // Ad was clicked open in browser
    else if ((mUrl != url) && (scheme == "http" || scheme == "https"))
    {
        addClickTrackingRedirect(url);
        qDebug() << "Ad clicked. Click URL: " << url;
        emit adClicked();
        if (!mInterceptslinks) {
            showBrowserForUrl(url);
            request->ignore();
        }
    }
    // Invoke all other url schemes.
    else if (mUrl != url)
    {
        invokeUrl(url);
        request->ignore();
    }
    emit htmlChanged();
}

void MoPubView::registerClick(){
    if (!mClickThroughUrl.isEmpty()) {
        QNetworkRequest request = QNetworkRequest();
        request.setUrl(mClickThroughUrl);
        //Latin1 encoding chosen with suggestion from RFC 5987 might not be the perfect choice.
        request.setRawHeader("User-Agent", mAdView->settings()->userAgent().toLatin1());
        QNetworkReply* reply = mNetworkAccessManager->get(request);
        bool res = connect(reply, SIGNAL(finished()), this, SLOT(onRegisterClickReply()));
        Q_ASSERT(res);
        Q_UNUSED(res);
    }
}

void MoPubView::onRegisterClickReply(){
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    Q_CHECK_PTR(reply);
    if ( QNetworkReply::NoError != reply->error()){
        qDebug() << "RegisterClick error: " << reply->errorString();
    }
    reply->deleteLater();
}

void MoPubView::addClickTrackingRedirect(QUrl url){
    if (!mClickThroughUrl.isEmpty()) {
       url.setUrl(mClickThroughUrl.toString() + "&r=" + url.toString());
    }
}

void MoPubView::invokeUrl(QUrl url)
{
    registerClick();
    InvokeRequest request = InvokeRequest();
    request.setUri(url);
    mInvokeManager->invoke(request);
}

void MoPubView::showBrowserForUrl(QUrl url)
{
    registerClick();
    InvokeRequest request = InvokeRequest();
    request.setUri(url);
    request.setAction("bb.action.OPEN");
    request.setTarget("sys.browser");
    mInvokeManager->invoke(request);
}

void MoPubView::fetchAd(){
    QNetworkRequest request = QNetworkRequest();
    request.setUrl(mUrl);
    request.setRawHeader("User-Agent", getUserAgent().toLatin1());

    QNetworkReply* reply = mNetworkAccessManager->get(request);
    bool res = connect(reply, SIGNAL(finished()), this, SLOT(onFetchAdReply()));
    Q_ASSERT(res);
    res = connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(onFetchAdError(QNetworkReply::NetworkError)));
    Q_ASSERT(res);
    Q_UNUSED(res);
}

void MoPubView::onFetchAdError(QNetworkReply::NetworkError code){
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    Q_CHECK_PTR(reply);
    qDebug() << "Network Error fetching ad code: " + QString().setNum(code);
    reply->deleteLater();
    loadFailUrl();
}

void MoPubView::onFetchAdReply(){
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    Q_CHECK_PTR(reply);
    // Client and Server HTTP errors should result in an exponential back off
    QVariant statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    if (!statusCode.isNull() && statusCode.toInt() >= 400){
        mFetchStatus = INVALID_SERVER_RESPONSE_BACKOFF;
        qDebug() << "MoPub server returned invalid response." << reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
        exponentialBackoff();
        return;
    }else if (!statusCode.isNull() && statusCode.toInt() != 200){
        mFetchStatus = INVALID_SERVER_RESPONSE_NOBACKOFF;
        qDebug() << "MoPub server returned invalid response." << reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
        return;
    }

    configureAdViewUsingHeadersFromHttpResponse(reply);

    if (reply->hasRawHeader("X-Adtype")) {
        QString adType(reply->rawHeader("X-Adtype"));

        // Ensure that the ad type header is valid and not "clear".
        if (adType.toLower() == "clear"){
            qDebug() <<  "MoPub server returned no ad.";
            mFetchStatus = CLEAR_AD_TYPE;
            loadFailUrl();
            exponentialBackoff();
            return;
        } else if (adType.toLower() == "custom"){
            // Handle custom native ad type.
            if (reply->hasRawHeader("X-Customselector")) {
                QString value(reply->rawHeader("X-Customselector"));
                qDebug() << "Trying to call method named " << value;
                mIsLoading = false;
                //TODO Handle custom native ad type.
                qDebug() << "Couldn't call custom method not implemented.";
                emitAdFailed();
            }
            return;
        } else if (adType.toLower() == "mraid"){
            // Handle mraid ad type
            qDebug() << "Loading mraid ad";
            QHash<QString, QString> paramsHash;
            paramsHash.insert("X-Adtype",adType);
            paramsHash.insert("X-Nativeparams",QString(reply->readAll()));
            mIsLoading = false;
            loadNativeSDK(paramsHash);
            emitAdFailed();
            return;
        } else if (adType.toLower() != "html"){
            // Handle native SDK ad type.
            qDebug() << "Loading native ad";
            QHash<QString, QString> paramsHash;
            paramsHash.insert("X-Adtype",adType);
            QString npHeader(reply->rawHeader("X-Nativeparams"));
            paramsHash.insert("X-Nativeparams","{}");
            if (!npHeader.isEmpty()) {
                paramsHash.insert("X-Nativeparams", npHeader);
            }
            QString ftHeader(reply->rawHeader("X-Fulladtype"));
            if (!ftHeader.isEmpty()) {
               paramsHash.insert("X-Fulladtype", ftHeader);
            }
            mIsLoading = false;
            loadNativeSDK(paramsHash);
            emitAdFailed();
            return;
        }
    }
    // Handle HTML ad.
    bool res = reply->open(QIODevice::ReadOnly);
    Q_ASSERT(res);
    Q_UNUSED(res);
    QString value(reply->readAll());
    reply->close();

    //Remove webview's incorrectly handling of meta viewport device-size element.
    QRegExp viewport("<meta name=\"viewport\".*>");
    viewport.setMinimal(true);
    value.remove(viewport);

    mAdView->setHtml(value,mUrl);
    mIsLoading = false;
    reply->deleteLater();
}

//TODO add any native SDK support currently there are none for BB10
void MoPubView::loadNativeSDK(const QHash<QString, QString>& paramsHash){
    qDebug() << "Loading native SDK is not implemented.";
}

void MoPubView::configureAdViewUsingHeadersFromHttpResponse(QNetworkReply* reply){
    Q_CHECK_PTR(reply);

    // Print the ad network type to the console.
    if (reply->hasRawHeader("X-Networktype")) {
        QString value(reply->rawHeader("X-Networktype"));
        qDebug() <<  "Fetching ad network type: " << value;
    }

    // Set the redirect URL prefix: navigating to any matching URLs will send us to the browser.
    if (reply->hasRawHeader("X-Launchpage")) {
        mRedirectUrl = QUrl(QString(reply->rawHeader("X-Launchpage")));
    }else {mRedirectUrl = QUrl();}

    // Set the URL that is prepended to links for click-tracking purposes.
    if (reply->hasRawHeader("X-Clickthrough")) {
        mClickThroughUrl = QString(reply->rawHeader("X-Clickthrough"));
    }else {mClickThroughUrl = QString();}

    // Set the fall-back URL to be used if the current request fails.
    if (reply->hasRawHeader("X-Failurl")) {
        mFailUrl = QUrl(QString(reply->rawHeader("X-Failurl")));
    }else {mFailUrl = QUrl();}

    // Set the URL to be used for impression tracking.
    if (reply->hasRawHeader("X-Imptracker")) {
        mImpressionUrl = QUrl(QString(reply->rawHeader("X-Imptracker")));
    }else {mImpressionUrl = QUrl();}

    // Set the webview's scrollability.
    bool enabled = false;
    if (reply->hasRawHeader("X-Scrollable")) {
        enabled = QString(reply->rawHeader("X-Scrollable")) == "1";
    }
    setWebViewScrollingEnabled(enabled);

    // Set the width and height.
    if (reply->hasRawHeader("X-Width") && reply->hasRawHeader("X-Height")) {
        mWidth = QString(reply->rawHeader("X-Width")).toInt();
        mHeight =QString(reply->rawHeader("X-Height")).toInt();
    } else {
        mWidth = 0;
        mHeight = 0;
    }

    // Set the auto-refresh time. A timer will be scheduled upon ad success or failure.
    if (reply->hasRawHeader("X-Refreshtime")){
        mRefreshTimeMilliseconds = QString(reply->rawHeader("X-Refreshtime")).toInt() * 1000;
        if (mRefreshTimeMilliseconds < MINIMUM_REFRESH_TIME_MILLISECONDS) {
            mRefreshTimeMilliseconds = MINIMUM_REFRESH_TIME_MILLISECONDS;
        }
    } else { mRefreshTimeMilliseconds = 0; }

    // Set the allowed orientations for this ad.
    if (reply->hasRawHeader("X-Orientation")){
        mAdOrientation = QString(reply->rawHeader("X-Orientation"));
    } else { mAdOrientation = QString(); }


    if (reply->hasRawHeader("X-Interceptlinks")){
        if (QString(reply->rawHeader("X-Interceptlinks")) == "1"){
            mInterceptslinks = true;
        } else {mInterceptslinks = false;}

    }
}

void MoPubView::setWebViewScrollingEnabled(bool enabled){
    bb::cascades::ScrollViewProperties* scrollViewProp = mScrollView->scrollViewProperties();
    if (enabled){
        scrollViewProp->setScrollMode(ScrollMode::Both);
    }else{
        scrollViewProp->setScrollMode(ScrollMode::None);
    }
}

void MoPubView::scheduleRefreshTimerIfEnabled(){
    if (!mAutoRefreshEnabled || mRefreshTimeMilliseconds <= 0) return;
    mAutoAdRefreshTimer->setSingleShot(true);
    mAutoAdRefreshTimer->start(mRefreshTimeMilliseconds);
    qDebug() << "Auto refreshing AdUnit " << mAdUnitId << "enabled for timeout after " << mRefreshTimeMilliseconds << "ms";
}

void MoPubView::cancelRefreshTimer(){
    if (mAutoAdRefreshTimer->isActive())
    {
        mAutoAdRefreshTimer->stop();
        qDebug() << "Auto refreshing AdUnit " << mAdUnitId << "disabled.";
    }
}

void MoPubView::loadFailUrl(){
    mIsLoading = false;
    if (!mFailUrl.isEmpty()) {
        qDebug() << "Loading failover url: " << mFailUrl;
        mUrl = mFailUrl;
        loadAd();
    } else {
        // No other URLs to try, so signal a failure.
        emitAdFailed();
    }
}

void MoPubView::emitAdDidLoad(){
    qDebug() << "Ad successfully loaded.";
    mIsLoading = false;
    scheduleRefreshTimerIfEnabled();
    emit adDidLoad();
}
void MoPubView::emitAdFailed(){
    qDebug() << "Ad failed to load.";
    mIsLoading = false;
    scheduleRefreshTimerIfEnabled();
    emit adFailed();
}

void MoPubView::trackImpression() {
    if (mImpressionUrl.isEmpty()) return;
    QNetworkRequest request = QNetworkRequest();
    request.setUrl(mImpressionUrl);
    request.setRawHeader("User-Agent", mAdView->settings()->userAgent().toLatin1());
    mNetworkAccessManager->get(request);
}

void MoPubView::exponentialBackoff(){
    int refreshMills =  mRefreshTimeMilliseconds * EXPONENTIAL_BACKOFF_FACTOR;
    if (refreshMills > MAXIMUM_REFRESH_TIME_MILLISECONDS){
        refreshMills = MAXIMUM_REFRESH_TIME_MILLISECONDS;
    }
    mRefreshTimeMilliseconds = refreshMills;
}

QString MoPubView::getUserAgent(){
   QString agent = mAdView->settings()->userAgent();
   //A fake user agent string is added when it is blank. Added version value from Cascades Gold Release sdk.
   //This string matches the format added to webkit http://trac.webkit.org/changeset/125779/trunk/Source/WebCore/inspector/front-end/SettingsScreen.js
   if (agent.isEmpty())
   {
       agent = QString("[\"BlackBerry \u2014 BB10\", \"Mozilla/5.0 (BB10; Touch) AppleWebKit/537.1+ (KHTML, like Gecko) Version/10.0.9.1673 Mobile Safari/537.1+\", \"768x1280x1\"]");
   }
   return agent;
}

QString MoPubView::createRequestId(){
    return "&reqid=" + QUuid::createUuid();
}

QString MoPubView::createRequestTime(){
    return "&reqt=" + QDateTime::currentMSecsSinceEpoch();
}

void MoPubView::impressionTracking(){
    QNetworkRequest request = QNetworkRequest();
    request.setUrl(QUrl(
                        createMoPubAPIUrl(IMPRESSION_HANDLER)
                        + getUdid()
                        + "&appid=" + mPackageInfo->installId()
                        + createRequestId()
                        + createRequestTime()
                        + "&random=" + rand()
                        ));

    request.setRawHeader("User-Agent", getUserAgent().toLatin1());
    mNetworkAccessManager->get(request);
}

void MoPubView::conversionTracking(){
    QNetworkRequest request = QNetworkRequest();
    request.setUrl(QUrl(
                        MOPUB_URL + CONVERSION_HANDLER
                        + "?id=" + mPackageInfo->installId()
                        + getUdid()
                        ));
    request.setRawHeader("User-Agent", getUserAgent().toLatin1());
    mNetworkAccessManager->get(request);
}
