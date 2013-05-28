#ifndef MOPUBVIEW_HPP_
#define MOPUBVIEW_HPP_
#define TESTING

#include <QObject>
#include <QUrl>
#include <QString>
#include <QtLocationSubset/QGeoPositionInfoSource>

#include <bb/cascades/CustomControl>

namespace bb {
    namespace cascades {
        class Container;
        class ScrollView;
        class WebView;
        class WebNavigationRequest;
    }
    namespace system {
        class InvokeManager;
    }
    namespace device{
        class HardwareInfo;
        class DeviceInfo;
    }
    class PackageInfo;
}

class MoPubView: public bb::cascades::CustomControl {
	Q_OBJECT
	Q_PROPERTY(QString adUnitId READ adUnitId WRITE setAdUnitId)
	Q_PROPERTY(QUrl clickThroughUrl READ clickThroughUrl WRITE setClickThroughUrl)
	Q_PROPERTY(QString adOrientation READ adOrientation)
	Q_PROPERTY(int refreshTimeMilliseconds READ refreshTimeMilliseconds WRITE setRefreshTimeMilliseconds )
	Q_PROPERTY(int width READ width WRITE setWidth )
	Q_PROPERTY(int height READ height WRITE setHeight )
	Q_PROPERTY(QUrl redirectUrl READ redirectUrl)
	Q_PROPERTY(bool autoRefreshEnabled READ autoRefreshEnabled WRITE setAutoRefreshEnabled )
	Q_PROPERTY(bool interceptslinks READ interceptslinks)
	Q_PROPERTY(QString adHtml READ adHtml NOTIFY htmlChanged )

public:
	static const QString SDK_VERSION;
	static const QString API_VERSION;
	static const QString MOPUB_URL;
	static const QString AD_HANDLER;
	static const QString IMPRESSION_HANDLER;
	static const QString CONVERSION_HANDLER;
	static const int MINIMUM_REFRESH_TIME_MILLISECONDS;
	static const int MAXIMUM_REFRESH_TIME_MILLISECONDS;
	static const double EXPONENTIAL_BACKOFF_FACTOR;

	MoPubView();
	virtual ~MoPubView();

	//Q_PROPERTIES getter setters
	QString adUnitId() const { return mAdUnitId; }
	void setAdUnitId(const QString value) { mAdUnitId = value; }

	QUrl clickThroughUrl() const { return mClickThroughUrl; }
	void setClickThroughUrl(const QUrl value) { mClickThroughUrl = value; }

	QString adOrientation() const { return mAdOrientation; }

	int refreshTimeMilliseconds() const { return mRefreshTimeMilliseconds; }
	void setRefreshTimeMilliseconds(int value) { mRefreshTimeMilliseconds = value; }

    int width() const { return mWidth; }
    void setWidth(int value);

    int height() const { return mHeight; }
    void setHeight(int value);

    QUrl redirectUrl() const { return mRedirectUrl; }

    bool autoRefreshEnabled() const { return mAutoRefreshEnabled; }
    void setAutoRefreshEnabled(bool value) {
        mAutoRefreshEnabled = value;
        if (!mAutoRefreshEnabled) cancelRefreshTimer();
        else scheduleRefreshTimerIfEnabled();
    }

    bool interceptslinks() const { return mInterceptslinks; }
    QString adHtml();

public Q_SLOTS:
    Q_INVOKABLE void loadAd();
	Q_INVOKABLE void loadFailUrl();
	Q_INVOKABLE void conversionTracking();

Q_SIGNALS:
	void adDidLoad();
	void adDidClose();
	void adClicked();
	void adWillLoad(QUrl adUrl);
	void adFailed();
	void htmlChanged();

protected:
	void trackImpression();
    void impressionTracking();

private Q_SLOTS:
	void onNavigationRequested(bb::cascades::WebNavigationRequest *request);
    void onRegisterClickReply();
    void onFetchAdReply();
    void onFetchAdError(QNetworkReply::NetworkError);
    void scheduleRefreshTimerIfEnabled();
    void cancelRefreshTimer();

private:
    void addClickTrackingRedirect(QUrl url);
    void invokeUrl(QUrl url);
    void showBrowserForUrl(QUrl url);
    void registerClick();
    QUrl generateAdUrl();
    QString createMoPubAPIUrl(QString handlerPart);

    void fetchAd();
    void configureAdViewUsingHeadersFromHttpResponse(QNetworkReply* reply);
    void setWebViewScrollingEnabled(bool enabled);
    void loadNativeSDK(const QHash<QString, QString>& paramsHash);
    void exponentialBackoff();

    void emitAdDidLoad();
    void emitAdFailed();
    QString getTimeZone();
    QString getUserAgent();
    QString getUdid();
    QString createRequestId();
    QString createRequestTime();
    QString getOrientation();

private:
    bb::cascades::Container* mControlContainer;
	bb::cascades::ScrollView* mScrollView;
	bb::cascades::WebView* mAdView;
	bb::system::InvokeManager* mInvokeManager;
	QNetworkAccessManager* mNetworkAccessManager;
	QtMobilitySubset::QGeoPositionInfoSource* mPositionSource;
	bb::device::HardwareInfo* mHardwareInfo;
	bb::device::DeviceInfo* mDeviceInfo;
	QTimer *mAutoAdRefreshTimer;
	bb::PackageInfo* mPackageInfo;
	QUrl mUrl;
    QUrl mImpressionUrl;
    QUrl mFailUrl;
    bool mIsLoading;


    enum FetchStatus {
        NOT_SET,
        FETCH_CANCELLED,
        INVALID_SERVER_RESPONSE_BACKOFF,
        INVALID_SERVER_RESPONSE_NOBACKOFF,
        CLEAR_AD_TYPE
    };
    FetchStatus mFetchStatus;

	//Q_PROPERTIES
	QString mAdUnitId;
	QUrl mClickThroughUrl;
	QString mAdOrientation;
	int mRefreshTimeMilliseconds;
	int mWidth;
	int mHeight;
	QUrl mRedirectUrl;
    bool mAutoRefreshEnabled;
    bool mInterceptslinks;
};

#endif /* MOPUBVIEW_HPP_ */
