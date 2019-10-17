/***************************************************************************
    kadaspictureitem.cpp
    --------------------
    copyright            : (C) 2019 by Sandro Mani
    email                : smani at sourcepole dot ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QImageReader>
#include <QMenu>

#include <exiv2/exiv2.hpp>

#include <qgis/qgsgeometryengine.h>
#include <qgis/qgslinestring.h>
#include <qgis/qgsmapsettings.h>
#include <qgis/qgspolygon.h>
#include <qgis/qgsproject.h>

#include <kadas/gui/mapitems/kadaspictureitem.h>


KadasPictureItem::KadasPictureItem( const QgsCoordinateReferenceSystem &crs, QObject *parent )
  : KadasMapItem( crs, parent )
{
  clear();
}

void KadasPictureItem::setup( const QString &path, const KadasItemPos &fallbackPos, bool ignoreExiv, double offsetX, double offsetY )
{
  mFilePath = path;
  QImageReader reader( path );

  // Scale such that largest dimension is max 64px
  QSize size = reader.size();
  if ( size.width() > size.height() )
  {
    size.setHeight( ( 64. * size.height() ) / size.width() );
    size.setWidth( 64 );
  }
  else
  {
    size.setWidth( ( 64. * size.width() ) / size.height() );
    size.setHeight( 64 );
  }

  state()->size = size;

  state()->pos = fallbackPos;
  QgsPointXY wgsPos;
  if ( !ignoreExiv && readGeoPos( path, wgsPos ) )
  {
    QgsPointXY itemPos = QgsCoordinateTransform( QgsCoordinateReferenceSystem( "EPSG:4326" ), mCrs, QgsProject::instance()->transformContext() ).transform( wgsPos );
    state()->pos = KadasItemPos( itemPos.x(), itemPos.y() );
  }

  mOffsetX = offsetX;
  mOffsetY = offsetY;

  reader.setBackgroundColor( Qt::white );
  reader.setScaledSize( size );
  mImage = reader.read().convertToFormat( QImage::Format_ARGB32 );

  update();
}

void KadasPictureItem::setFilePath( const QString &filePath )
{
  mFilePath = filePath;
  update();
}

void KadasPictureItem::setOffsetX( double offsetX )
{
  mOffsetX = offsetX;
  update();
}

void KadasPictureItem::setOffsetY( double offsetY )
{
  mOffsetY = offsetY;
  update();
}

void KadasPictureItem::setFrameVisible( bool frame )
{
  mFrame = frame;
  if ( !frame )
  {
    mOffsetX = 0;
    mOffsetY = 0;
  }
  update();
}

void KadasPictureItem::setPosition( const KadasItemPos &pos )
{
  state()->pos = pos;
  state()->drawStatus = State::DrawStatus::Finished;
  update();
}

KadasItemRect KadasPictureItem::boundingBox() const
{
  return KadasItemRect( constState()->pos, constState()->pos );
}

KadasMapItem::Margin KadasPictureItem::margin() const
{
  double framePadding = mFrame ? sFramePadding : 0;
  return Margin
  {
    qCeil( qMax( 0., 0.5 * constState()->size.width() - mOffsetX + framePadding ) ),
    qCeil( qMax( 0., 0.5 * constState()->size.height() + mOffsetY + framePadding ) ),
    qCeil( qMax( 0., 0.5 * constState()->size.width() + mOffsetX + framePadding ) ),
    qCeil( qMax( 0., 0.5 * constState()->size.height() - mOffsetY + framePadding ) )
  };
}

QList<KadasMapPos> KadasPictureItem::cornerPoints( const QgsMapSettings &settings ) const
{
  KadasMapPos mapPos = toMapPos( constState()->pos, settings );
  double halfW = 0.5 * constState()->size.width();
  double halfH = 0.5 * constState()->size.height();
  double mup = settings.mapUnitsPerPixel();

  KadasMapPos p1( mapPos.x() + ( mOffsetX - halfW ) * mup, mapPos.y() + ( mOffsetY - halfH ) * mup );
  KadasMapPos p2( mapPos.x() + ( mOffsetX + halfW ) * mup, mapPos.y() + ( mOffsetY - halfH ) * mup );
  KadasMapPos p3( mapPos.x() + ( mOffsetX + halfW ) * mup, mapPos.y() + ( mOffsetY + halfH ) * mup );
  KadasMapPos p4( mapPos.x() + ( mOffsetX - halfW ) * mup, mapPos.y() + ( mOffsetY + halfH ) * mup );

  return QList<KadasMapPos>() << p1 << p2 << p3 << p4;
}

QList<KadasMapItem::Node> KadasPictureItem::nodes( const QgsMapSettings &settings ) const
{
  double mup = settings.mapUnitsPerPixel() * QgsUnitTypes::fromUnitToUnitFactor( settings.destinationCrs().mapUnits(), mCrs.mapUnits() );
  QList<KadasMapPos> points = cornerPoints( settings );
  QList<Node> nodes;
  nodes.append( {points[0]} );
  nodes.append( {points[1]} );
  nodes.append( {points[2]} );
  nodes.append( {points[3]} );
  nodes.append( {toMapPos( constState()->pos, settings ), anchorNodeRenderer} );
  return nodes;
}

bool KadasPictureItem::intersects( const KadasMapRect &rect, const QgsMapSettings &settings ) const
{
  if ( constState()->size.isEmpty() )
  {
    return false;
  }

  QList<KadasMapPos> points = cornerPoints( settings );
  QgsPolygon imageRect;
  imageRect.setExteriorRing( new QgsLineString( QgsPointSequence() << QgsPoint( points[0] ) << QgsPoint( points[1] ) << QgsPoint( points[2] ) << QgsPoint( points[3] ) << QgsPoint( points[0] ) ) );

  QgsPolygon filterRect;
  QgsLineString *exterior = new QgsLineString();
  exterior->setPoints( QgsPointSequence()
                       << QgsPoint( rect.xMinimum(), rect.yMinimum() )
                       << QgsPoint( rect.xMaximum(), rect.yMinimum() )
                       << QgsPoint( rect.xMaximum(), rect.yMaximum() )
                       << QgsPoint( rect.xMinimum(), rect.yMaximum() )
                       << QgsPoint( rect.xMinimum(), rect.yMinimum() ) );
  filterRect.setExteriorRing( exterior );

  QgsGeometryEngine *geomEngine = QgsGeometry::createGeometryEngine( &imageRect );
  bool intersects = geomEngine->intersects( &filterRect );
  delete geomEngine;
  return intersects;
}

void KadasPictureItem::render( QgsRenderContext &context ) const
{
  QgsPoint pos( constState()->pos );
  pos.transform( context.coordinateTransform() );
  pos.transform( context.mapToPixel().transform() );
  context.painter()->translate( pos.x(), pos.y() );

  double w = constState()->size.width();
  double h = constState()->size.height();

  // Draw frame
  if ( mFrame )
  {
    double framew = w + 2 * sFramePadding;
    double frameh = h + 2 * sFramePadding;
    context.painter()->setPen( QPen( Qt::black, 1 ) );
    context.painter()->setBrush( Qt::white );
    QPolygonF poly;
    poly.append( QPointF( mOffsetX - 0.5 * framew, -mOffsetY + 0.5 * frameh ) );
    poly.append( QPointF( mOffsetX - 0.5 * framew, -mOffsetY - 0.5 * frameh ) );
    poly.append( QPointF( mOffsetX + 0.5 * framew, -mOffsetY - 0.5 * frameh ) );
    poly.append( QPointF( mOffsetX + 0.5 * framew, -mOffsetY + 0.5 * frameh ) );
    poly.append( QPointF( mOffsetX - 0.5 * framew, -mOffsetY + 0.5 * frameh ) );
    // Draw frame triangle
    if ( qAbs( mOffsetX ) > qAbs( 0.5 * framew ) || qAbs( mOffsetY ) > qAbs( 0.5 * frameh ) )
    {
      QgsPointXY framePos;
      QgsVector baseDir;
      // Determine triangle quadrant
      int quadrant = qRound( qAtan2( -mOffsetY, mOffsetX ) / M_PI * 180 / 90 );
      if ( quadrant < 0 ) { quadrant += 4; }
      if ( quadrant == 0 || quadrant == 2 )   // Triangle to the left (quadrant = 0) or right (quadrant = 2)
      {
        baseDir = QgsVector( 0, quadrant == 0 ? -1 : 1 );
        framePos.setX( quadrant == 0 ? mOffsetX - 0.5 * framew : mOffsetX + 0.5 * framew );
        framePos.setY( qMin( qMax( -mOffsetY - 0.5 * frameh + sArrowWidth, 0. ), -mOffsetY + 0.5 * frameh - sArrowWidth ) );
      }
      else     // Triangle above (quadrant = 1) or below (quadrant = 3)
      {
        framePos.setX( qMin( qMax( mOffsetX - 0.5 * framew + sArrowWidth, 0. ), mOffsetX + 0.5 * framew - sArrowWidth ) );
        framePos.setY( quadrant == 1 ? -mOffsetY - 0.5 * frameh : -mOffsetY + 0.5 * frameh );
        baseDir = QgsVector( quadrant == 1 ? 1 : -1, 0 );
      }
      int inspos = quadrant + 1;
      poly.insert( inspos++, QPointF( framePos.x() - sArrowWidth * baseDir.x(), framePos.y() - sArrowWidth * baseDir.y() ) );
      poly.insert( inspos++, QPointF( 0, 0 ) );
      poly.insert( inspos++, QPointF( framePos.x() + sArrowWidth * baseDir.x(), framePos.y() + sArrowWidth * baseDir.y() ) );
    }
    QPainterPath path;
    path.addPolygon( poly );
    context.painter()->drawPath( path );
  }

  context.painter()->drawImage( mOffsetX - 0.5 * w - 0.5, -mOffsetY - 0.5 * h - 0.5, mImage );
}

bool KadasPictureItem::startPart( const KadasMapPos &firstPoint, const QgsMapSettings &mapSettings )
{
  state()->drawStatus = State::Drawing;
  state()->pos = toItemPos( firstPoint, mapSettings );
  update();
  return false;
}

bool KadasPictureItem::startPart( const AttribValues &values, const QgsMapSettings &mapSettings )
{
  return startPart( KadasMapPos( values[AttrX], values[AttrY] ), mapSettings );
}

void KadasPictureItem::setCurrentPoint( const KadasMapPos &p, const QgsMapSettings &mapSettings )
{
  // Do nothing
}

void KadasPictureItem::setCurrentAttributes( const AttribValues &values, const QgsMapSettings &mapSettings )
{
  // Do nothing
}

bool KadasPictureItem::continuePart( const QgsMapSettings &mapSettings )
{
  // No further action allowed
  return false;
}

void KadasPictureItem::endPart()
{
  state()->drawStatus = State::Finished;
}

KadasMapItem::AttribDefs KadasPictureItem::drawAttribs() const
{
  AttribDefs attributes;
  attributes.insert( AttrX, NumericAttribute{"x", NumericAttribute::XCooAttr} );
  attributes.insert( AttrY, NumericAttribute{"y", NumericAttribute::YCooAttr} );
  return attributes;
}

KadasMapItem::AttribValues KadasPictureItem::drawAttribsFromPosition( const KadasMapPos &pos, const QgsMapSettings &mapSettings ) const
{
  AttribValues values;
  values.insert( AttrX, pos.x() );
  values.insert( AttrY, pos.y() );
  return values;
}

KadasMapPos KadasPictureItem::positionFromDrawAttribs( const AttribValues &values, const QgsMapSettings &mapSettings ) const
{
  return KadasMapPos( values[AttrX], values[AttrY] );
}

KadasMapItem::EditContext KadasPictureItem::getEditContext( const KadasMapPos &pos, const QgsMapSettings &mapSettings ) const
{
  double mup = mapSettings.mapUnitsPerPixel();
  if ( intersects( KadasMapRect( pos.x() - mup, pos.y() - mup, pos.x() + mup, pos.y() + mup ), mapSettings ) )
  {
    KadasMapPos mapPos = toMapPos( constState()->pos, mapSettings );
    KadasMapPos framePos( mapPos.x() + mOffsetX * mup, mapPos.y() + mOffsetY * mup );
    return EditContext( QgsVertexId( 0, 0, 0 ), framePos );
  }
  return EditContext();
}

void KadasPictureItem::edit( const EditContext &context, const KadasMapPos &newPoint, const QgsMapSettings &mapSettings )
{
  if ( context.vidx.isValid() )
  {
    QgsCoordinateTransform crst( crs(), mapSettings.destinationCrs(), QgsProject::instance() );
    QgsPointXY screenPos = mapSettings.mapToPixel().transform( newPoint );
    QgsPointXY screenAnchor = mapSettings.mapToPixel().transform( toMapPos( constState()->pos, mapSettings ) );
    mOffsetX = screenPos.x() - screenAnchor.x();
    mOffsetY = screenAnchor.y() - screenPos.y();
    update();
  }
}

void KadasPictureItem::edit( const EditContext &context, const AttribValues &values, const QgsMapSettings &mapSettings )
{
  // No editable attributes
}

void KadasPictureItem::populateContextMenu( QMenu *menu, const EditContext &context )
{
  QAction *frameAction = menu->addAction( tr( "Frame visible" ), [this]( bool active ) { setFrameVisible( active ); } );
  frameAction->setCheckable( true );
  frameAction->setChecked( mFrame );
}

KadasMapItem::AttribValues KadasPictureItem::editAttribsFromPosition( const EditContext &context, const KadasMapPos &pos, const QgsMapSettings &mapSettings ) const
{
  return drawAttribsFromPosition( pos, mapSettings );
}

KadasMapPos KadasPictureItem::positionFromEditAttribs( const EditContext &context, const AttribValues &values, const QgsMapSettings &mapSettings ) const
{
  return positionFromDrawAttribs( values, mapSettings );
}

bool KadasPictureItem::readGeoPos( const QString &filePath, QgsPointXY &wgsPos )
{
  // Read EXIF position
  Exiv2::Image::AutoPtr image;
  try
  {
    image = Exiv2::ImageFactory::open( filePath.toLocal8Bit().data() );
  }
  catch ( const Exiv2::Error & )
  {
    return false;
  }

  if ( image.get() == 0 )
  {
    return false;
  }

  image->readMetadata();
  Exiv2::ExifData &exifData = image->exifData();
  if ( exifData.empty() )
  {
    return false;
  }

  Exiv2::ExifData::iterator itLatRef = exifData.findKey( Exiv2::ExifKey( "Exif.GPSInfo.GPSLatitudeRef" ) );
  Exiv2::ExifData::iterator itLatVal = exifData.findKey( Exiv2::ExifKey( "Exif.GPSInfo.GPSLatitude" ) );
  Exiv2::ExifData::iterator itLonRef = exifData.findKey( Exiv2::ExifKey( "Exif.GPSInfo.GPSLongitudeRef" ) );
  Exiv2::ExifData::iterator itLonVal = exifData.findKey( Exiv2::ExifKey( "Exif.GPSInfo.GPSLongitude" ) );

  if ( itLatRef == exifData.end() || itLatVal == exifData.end() ||
       itLonRef == exifData.end() || itLonVal == exifData.end() )
  {
    return false;
  }
  QString latRef = QString::fromStdString( itLatRef->value().toString() );
  QStringList latVals = QString::fromStdString( itLatVal->value().toString() ).split( QRegExp( "\\s+" ) );
  QString lonRef = QString::fromStdString( itLonRef->value().toString() );
  QStringList lonVals = QString::fromStdString( itLonVal->value().toString() ).split( QRegExp( "\\s+" ) );

  double lat = 0, lon = 0;
  double div = 1;
  for ( const QString &rational : latVals )
  {
    QStringList pair = rational.split( "/" );
    if ( pair.size() != 2 )
    {
      break;
    }
    lat += ( pair[0].toDouble() / pair[1].toDouble() ) / div;
    div *= 60;
  }

  div = 1;
  for ( const QString &rational : lonVals )
  {
    QStringList pair = rational.split( "/" );
    if ( pair.size() != 2 )
    {
      break;
    }
    lon += ( pair[0].toDouble() / pair[1].toDouble() ) / div;
    div *= 60;
  }

  if ( latRef.compare( "S", Qt::CaseInsensitive ) == 0 )
  {
    lat *= -1;
  }
  if ( lonRef.compare( "W", Qt::CaseInsensitive ) == 0 )
  {
    lon *= -1;
  }
  wgsPos = QgsPointXY( lon, lat );
  return true;
}
