/****************************************************************************
 *
 *   (c) 2009-2016 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "PositionManager.h"
#include "QGCApplication.h"
#include "QGCCorePlugin.h"
#include <QtMath>

QGCPositionManager::QGCPositionManager(QGCApplication* app, QGCToolbox* toolbox)
    : QGCTool           (app, toolbox)
    , _updateInterval   (0)
    , _gcsHeading       (NAN)
    , _currentSource    (NULL)
    , _defaultSource    (NULL)
    , _nmeaSource       (NULL)
    , _simulatedSource  (NULL)
{

}

QGCPositionManager::~QGCPositionManager()
{
    delete(_simulatedSource);
    delete(_nmeaSource);
}

void QGCPositionManager::setToolbox(QGCToolbox *toolbox)
{
   QGCTool::setToolbox(toolbox);
   //-- First see if plugin provides a position source
   _defaultSource = toolbox->corePlugin()->createPositionSource(this);
   if(!_defaultSource) {
       //-- Otherwise, create a default one
       _defaultSource = QGeoPositionInfoSource::createDefaultSource(this);
   }
   _simulatedSource = new SimulatedPosition();

   // Enable this to get a simulated target on desktop
   // if (_defaultSource == nullptr) {
   //     _defaultSource = _simulatedSource;
   // }

   setPositionSource(QGCPositionSource::InternalGPS);
}

void QGCPositionManager::setNmeaSourceDevice(QIODevice* device)
{
    // stop and release _nmeaSource
    if (_nmeaSource) {
        _nmeaSource->stopUpdates();
        disconnect(_nmeaSource);

        // if _currentSource is pointing there, point to null
        if (_currentSource == _nmeaSource){
            _currentSource = nullptr;
        }

        delete _nmeaSource;
        _nmeaSource = nullptr;

    }
    _nmeaSource = new QNmeaPositionInfoSource(QNmeaPositionInfoSource::RealTimeMode, this);
    _nmeaSource->setDevice(device);
    setPositionSource(QGCPositionManager::NmeaGPS);
}

void QGCPositionManager::_positionUpdated(const QGeoPositionInfo &update)
{
    QGeoCoordinate newGCSPosition = QGeoCoordinate();
    qreal newGCSHeading = update.attribute(QGeoPositionInfo::Direction);

    if (update.isValid()) {
        // Note that gcsPosition filters out possible crap values
        if (qAbs(update.coordinate().latitude()) > 0.001 && qAbs(update.coordinate().longitude()) > 0.001) {
            newGCSPosition =update.coordinate(); //w84to02(update.coordinate());
        }
    }
    if (newGCSPosition != _gcsPosition) {
        _gcsPosition = newGCSPosition;
        emit gcsPositionChanged(_gcsPosition);
    }
    if (newGCSHeading != _gcsHeading) {
        _gcsHeading = newGCSHeading;
        emit gcsHeadingChanged(_gcsHeading);
    }

    emit positionInfoUpdated(update);
}

int QGCPositionManager::updateInterval() const
{
    return _updateInterval;
}

void QGCPositionManager::setPositionSource(QGCPositionManager::QGCPositionSource source)
{
    if (_currentSource != nullptr) {
        _currentSource->stopUpdates();
        disconnect(_currentSource);
    }

    if (qgcApp()->runningUnitTests()) {
        // Units test on travis fail due to lack of position source
        return;
    }

    switch(source) {
    case QGCPositionManager::Log:
        break;
    case QGCPositionManager::Simulated:
        _currentSource = _simulatedSource;
        break;
    case QGCPositionManager::NmeaGPS:
        _currentSource = _nmeaSource;
        break;
    case QGCPositionManager::InternalGPS:
    default:        
        _currentSource = _defaultSource;
        break;
    }

    if (_currentSource != nullptr) {
        _updateInterval = _currentSource->minimumUpdateInterval();
        _currentSource->setPreferredPositioningMethods(QGeoPositionInfoSource::SatellitePositioningMethods);
        _currentSource->setUpdateInterval(_updateInterval);
        connect(_currentSource, &QGeoPositionInfoSource::positionUpdated,       this, &QGCPositionManager::_positionUpdated);
        connect(_currentSource, SIGNAL(error(QGeoPositionInfoSource::Error)),   this, SLOT(_error(QGeoPositionInfoSource::Error)));
        _currentSource->startUpdates();
    }
}

void QGCPositionManager::_error(QGeoPositionInfoSource::Error positioningError)
{
    qWarning() << "QGCPositionManager error" << positioningError;
}
bool QGCPositionManager::out_of_china(double lon,double lat)
{
    if(lon < 72.004 || lon > 137.8347)
   return true;
    else if(lat < 0.8293 || lat > 55.8271)
    return true;
           return false;
}

double QGCPositionManager::transformlat(double lon,double lat)
{
    double ret = -100.0 + 2.0 * lon + 3.0 * lat + 0.2 * lat * lat + 0.1 * lon * lat + 0.2 * qSqrt(qAbs(lon));
            ret += (20.0 * qSin(6.0 * lon * pi) + 20.0 * qSin(2.0 * lon * pi)) * 2.0 / 3.0;
            ret += (20.0 * qSin(lat * pi) + 40.0 * qSin(lat / 3.0 * pi)) * 2.0 / 3.0;
            ret += (160.0 * qSin(lat / 12.0 * pi) + 320 * qSin(lat * pi / 30.0)) * 2.0 / 3.0;
            return ret;
}

double QGCPositionManager::transformlng(double lon,double lat)
{

    double ret = 300.0 + lon + 2.0 * lat + 0.1 * lon * lon + 0.1 * lon * lat + 0.1 * qSqrt(qAbs(lon));
            ret += (20.0 * qSin(6.0 * lon * pi) + 20.0 * qSin(2.0 * lon * pi)) * 2.0 / 3.0;
            ret += (20.0 * qSin(lon * pi) + 40.0 * qSin(lon / 3.0 * pi)) * 2.0 / 3.0;
            ret += (150.0 * qSin(lon / 12.0 * pi) + 300.0 * qSin(lon / 30.0 * pi)) * 2.0 / 3.0;
            return ret;
}
QGeoCoordinate QGCPositionManager::w84to02(QGeoCoordinate position)
{
double wgs_lon=position.longitude();
double wgs_lat=position.latitude();
QGeoCoordinate temp = position;
if (out_of_china(wgs_lon, wgs_lat)) {
            return temp;
        }
        double dlat = transformlat(wgs_lon - 105.0, wgs_lat - 35.0);
        double dlng = transformlng(wgs_lon - 105.0, wgs_lat - 35.0);
        double radlat = wgs_lat / 180.0 * pi;
        double magic = qSin(radlat);
        magic = 1 - ee * magic * magic;
        double sqrtmagic = qSqrt(magic);
        dlat = (dlat * 180.0) / ((a * (1 - ee)) / (magic * sqrtmagic) * pi);
        dlng = (dlng * 180.0) / (a / sqrtmagic * qCos(radlat) * pi);
        temp.setAltitude(wgs_lat + dlat);
        temp.setLongitude(wgs_lon + dlng);
        return temp;
}
