#include "include/ui/utils/DataViewHtmlGenerator.h"

#include "include/global/CountryHelper.hpp"
#include "include/global/Configs.hpp"

void DataViewHtmlGenerator::setDownloadReport(const DownloadProgressReport &report, bool show) {
    QMutexLocker lk(&mu_);
    download_.visible = show;
    download_.report = report;
}

void DataViewHtmlGenerator::seedSpeedTest(int totalProfiles) {
    QMutexLocker lk(&mu_);
    testProgress.store(0);
    Configs::dataManager->settingsRepo->speed_test_mode == Configs::TestConfig::COUNTRY ? speedtest_.kind = SpeedtestPanelState::Kind::Country : speedtest_.kind = SpeedtestPanelState::Kind::Speed;
    speedtest_.totalProfiles = totalProfiles;
    speedtest_.visible = true;
}

void DataViewHtmlGenerator::setSpeedtestProgress(const QString &profileName, const libcore::SpeedTestResult &result) {
    QMutexLocker lk(&mu_);
    speedtest_.profileName = profileName;
    speedtest_.dlSpeed = QString::fromStdString(result.dl_speed.value());
    speedtest_.ulSpeed = QString::fromStdString(result.ul_speed.value());
    speedtest_.serverCountryFlag = CountryCodeToFlag(CountryNameToCode(QString::fromStdString(result.server_country.value())));
    speedtest_.serverCountry = QString::fromStdString(result.server_country.value());
    speedtest_.serverName = QString::fromStdString(result.server_name.value());
}

void DataViewHtmlGenerator::seedLatencyTest(LatencyTestPanelState::Kind kind, int totalProfiles) {
    QMutexLocker lk(&mu_);
    testProgress.store(0);
    latencyTest_.visible = true;
    latencyTest_.kind = kind;
    latencyTest_.totalProfiles = totalProfiles;
}

void DataViewHtmlGenerator::setAutoSelectorStatus(const QString &summary, const QString &detail) {
    QMutexLocker lk(&mu_);
    autoSelector_.summary = summary;
    autoSelector_.detail = detail;
    autoSelector_.visible = !summary.isEmpty();
}

void DataViewHtmlGenerator::clearTestSections() {
    QMutexLocker lk(&mu_);
    latencyTest_ = {};
    speedtest_ = {};
    testProgress.store(0);
}

void DataViewHtmlGenerator::addTestProgress(int count) {
    testProgress.fetch_add(count);
}

QString DataViewHtmlGenerator::buildHtml() {
    QMutexLocker lk(&mu_);
    QString html;
    html.reserve(512);
    if (download_.visible) {
        html += downloadSectionHtml();
    }
    if (speedtest_.visible) {
        html += speedtestSectionHtml();
    }
    if (latencyTest_.visible) {
        html += latencyTestSectionHtml();
    }
    // Deliberately last and conditional: the selector panel is ambient status,
    // so it yields the view entirely whenever a job wants to report progress.
    if (html.isEmpty() && autoSelector_.visible) {
        html += autoSelectorSectionHtml();
    }
    return html;
}

QString DataViewHtmlGenerator::autoSelectorSectionHtml() {
    QString res = QStringLiteral("<p style='text-align:center;margin:0;'>%1</p>").arg(autoSelector_.summary.toHtmlEscaped());
    if (!autoSelector_.detail.isEmpty()) {
        res += QStringLiteral("<p style='text-align:center;margin:0;opacity:0.75;'>%1</p>")
                   .arg(autoSelector_.detail.toHtmlEscaped());
    }
    return res;
}

QString DataViewHtmlGenerator::getProgressBar(long long current, long long total) {
    int filled = 0;
    if (total > 0) {
        filled = static_cast<int>(qBound(0LL, 10 * current / total, 10LL));
    }
    return QString(filled, QLatin1Char('#')) % QString(10 - filled, QLatin1Char('-'));
}

QString DataViewHtmlGenerator::downloadSectionHtml() {
    const auto progressText = getProgressBar(download_.report.downloadedSize, download_.report.totalSize);
    const QString stat = ReadableSize(download_.report.downloadedSize) % QLatin1Char('/')
                         % ReadableSize(download_.report.totalSize);
    return QStringLiteral("<p style='text-align:center;margin:0;'>Downloading %1: %2 %3</p>")
        .arg(download_.report.fileName, stat, progressText);
}

QString DataViewHtmlGenerator::speedtestSectionHtml() {
    if (speedtest_.kind == SpeedtestPanelState::Kind::Speed) {
        auto firstLine = QStringLiteral("Running Speedtest: %1").arg(speedtest_.profileName);
        if (speedtest_.totalProfiles > 1) {
            firstLine += QStringLiteral(" (%1 / %2)").arg(testProgress.load()).arg(speedtest_.totalProfiles);
        }
        if (speedtest_.serverName.isEmpty()) {
            return QStringLiteral("<p style='text-align:center;margin:0;'>%1</p>").arg(firstLine);
        }
        return QStringLiteral(
           "<p style='text-align:center;margin:0;'>%1</p>"
           "<div style='text-align: center;'>"
           "<span style='color: #3299FF;'>Dl↓ %2</span>  "
           "<span style='color: #86C43F;'>Ul↑ %3</span>"
           "</div>"
           "<p style='text-align:center;margin:0;'>Server: %4%5, %6</p>")
            .arg(firstLine, speedtest_.dlSpeed, speedtest_.ulSpeed, speedtest_.serverCountryFlag, speedtest_.serverCountry,
                speedtest_.serverName);
    }

    QString res;
    auto content = QStringLiteral("Running Country Test");
    if (speedtest_.totalProfiles > 1) {
        const int done = testProgress.load();
        const QString progress = getProgressBar(done, speedtest_.totalProfiles)
                                 % QLatin1Char(' ')
                                 % QString::number(100 * done / speedtest_.totalProfiles)
                                 % QLatin1Char('%');
        res += QStringLiteral("<p style='text-align:center;margin:0;'>%1</p>").arg(progress);
        content += QStringLiteral(" (%1 / %2)").arg(done).arg(speedtest_.totalProfiles);
    }
    res += QStringLiteral("<p style='text-align:center;margin:0;'>%1</p>").arg(content);
    return res;
}

QString DataViewHtmlGenerator::latencyTestSectionHtml() {
    QString res;
    auto content = latencyTest_.kind == LatencyTestPanelState::Kind::Url
                       ? QStringLiteral("Running URL test")
                       : QStringLiteral("Running IP test");
    if (latencyTest_.totalProfiles > 1) {
        const int done = testProgress.load();
        const QString progress = getProgressBar(done, latencyTest_.totalProfiles)
                                 % QLatin1Char(' ')
                                 % QString::number(100 * done / latencyTest_.totalProfiles)
                                 % QLatin1Char('%');
        res += QStringLiteral("<p style='text-align:center;margin:0;'>%1</p>").arg(progress);
        content += QStringLiteral(" (%1 / %2)").arg(done).arg(latencyTest_.totalProfiles);
    }
    res += QStringLiteral("<p style='text-align:center;margin:0;'>%1</p>").arg(content);
    return res;
}
