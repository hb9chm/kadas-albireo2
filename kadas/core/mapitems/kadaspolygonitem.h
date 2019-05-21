/***************************************************************************
    kadaspolygonitem.h
    ------------------
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

#ifndef KADASPOLYGONITEM_H
#define KADASPOLYGONITEM_H

#include <kadas/core/mapitems/kadasgeometryitem.h>

class QgsMultiPolygon;

class KADAS_CORE_EXPORT KadasPolygonItem : public KadasGeometryItem
{
public:
  KadasPolygonItem(const QgsCoordinateReferenceSystem& crs, bool geodesic = false, QObject* parent = nullptr);

  KadasStateStack::State* cloneState() const override{ return new State(*state()); }

  bool startPart(const QgsPointXY& firstPoint, const QgsMapSettings& mapSettings) override;
  bool moveCurrentPoint(const QgsPointXY& p, const QgsMapSettings& mapSettings) override;
  bool setNextPoint(const QgsPointXY& p, const QgsMapSettings& mapSettings) override;
  void endPart() override;
  QList<double> recomputeAttributes(const QgsPointXY& pos) const override;
  QgsPointXY positionFromAttributes(const QList<double>& values) const override;
  bool startPart(const QList<double>& attributeValues) override;
  void changeAttributeValues(const QList<double>& values) override;
  bool acceptAttributeValues() override;

  const QgsMultiPolygon* geometry() const;

private:
  struct State : KadasStateStack::State {
    QList<QList<QgsPointXY>> points;
  };
  enum Attributes {AttrX, AttrY, NAttrs};

  bool mGeodesic = false;

  QgsMultiPolygon* geometry();
  State* state() const{ return static_cast<State*>(mState); }
  State* createEmptyState() const override { return new State(); }
  void measureGeometry() override;
  void recomputeDerived() override;
};

#endif // KADASLINEITEM_H
