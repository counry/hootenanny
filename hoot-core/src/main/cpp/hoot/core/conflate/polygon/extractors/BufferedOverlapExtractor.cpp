/*
 * This file is part of Hootenanny.
 *
 * Hootenanny is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --------------------------------------------------------------------
 *
 * The following copyright notices are generated automatically. If you
 * have a new notice to add, please use the format:
 * " * @copyright Copyright ..."
 * This will properly maintain the copyright information. DigitalGlobe
 * copyrights will be updated automatically.
 *
 * @copyright Copyright (C) 2005 VividSolutions (http://www.vividsolutions.com/)
 * @copyright Copyright (C) 2013, 2014 DigitalGlobe (http://www.digitalglobe.com/)
 */
#include "BufferedOverlapExtractor.h"

// geos
#include <geos/geom/Geometry.h>
#include <geos/util/TopologyException.h>

// hoot
#include <hoot/core/Factory.h>
#include <hoot/core/util/ElementConverter.h>
#include <hoot/core/util/GeometryUtils.h>

namespace hoot
{

HOOT_FACTORY_REGISTER(FeatureExtractor, BufferedOverlapExtractor)

BufferedOverlapExtractor::BufferedOverlapExtractor(double bufferPortion)
{
  _bufferPortion = bufferPortion;
}

double BufferedOverlapExtractor::extract(const OsmMap& map, const ConstElementPtr& target,
  const ConstElementPtr& candidate) const
{
  ElementConverter ec(map.shared_from_this());
  shared_ptr<Geometry> g1 = ec.convertToGeometry(target);
  shared_ptr<Geometry> g2 = ec.convertToGeometry(candidate);

  if (g1->isEmpty() || g2->isEmpty())
  {
    return nullValue();
  }

  double a1, a2;
  try
  {
    a1 = g1->getArea();
    a2 = g2->getArea();
  }
  catch (geos::util::TopologyException& e)
  {
    g1.reset(GeometryUtils::validateGeometry(g1.get()));
    g2.reset(GeometryUtils::validateGeometry(g2.get()));
    a1 = g1->getArea();
    a2 = g2->getArea();
  }

  double buffer = sqrt(max(a1, a2)) * _bufferPortion;

  auto_ptr<Geometry> overlap;
  try
  {
    g1.reset(g1->buffer(buffer));
    g2.reset(g2->buffer(buffer));
    overlap.reset(g1->intersection(g2.get()));
  }
  catch (geos::util::TopologyException& e)
  {
    g1.reset(GeometryUtils::validateGeometry(g1.get()));
    g2.reset(GeometryUtils::validateGeometry(g2.get()));
    g1.reset(g1->buffer(buffer));
    g2.reset(g2->buffer(buffer));
    overlap.reset(g2->intersection(g1.get()));
  }

  double bufferedA1 = g1->getArea();
  double bufferedA2 = g2->getArea();
  double overlapArea = overlap->getArea();

  return std::min(1.0, (2 * overlapArea) / (bufferedA1 + bufferedA2));
}

string BufferedOverlapExtractor::getName() const
{
  return QString("BufferedOverlapExtractor %1").arg(_bufferPortion).toStdString();
}

}
