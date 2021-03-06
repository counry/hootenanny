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
 * @copyright Copyright (C) 2015, 2017 DigitalGlobe (http://www.digitalglobe.com/)
 */
#ifndef BUILDINGOUTLINEUPDATEOP_H
#define BUILDINGOUTLINEUPDATEOP_H

// Hoot
#include <hoot/core/ops/OsmMapOperation.h>
#include <hoot/core/io/Serializable.h>
#include <hoot/core/elements/Relation.h>

// Standard
#include <set>

namespace hoot
{
class OsmMap;

/**
 * Goes through all building relations and updates the outline of the building by taking the union
 * of all the building parts.
 */
class BuildingOutlineUpdateOp : public OsmMapOperation, public Serializable
{
public:

  static std::string className() { return "hoot::BuildingOutlineUpdateOp"; }

  static unsigned int logWarnCount;

  BuildingOutlineUpdateOp();

  virtual void apply(boost::shared_ptr<OsmMap>& map);

  virtual std::string getClassName() const { return className(); }

  virtual void readObject(QDataStream& /*is*/) {}

  virtual void writeObject(QDataStream& /*os*/) const {}

private:

  boost::shared_ptr<OsmMap> _map;

  void _createOutline(const RelationPtr& building);

  void _extractUsedNodes(const RelationPtr& r, std::set<long>& nodes);

  /**
   * Match nodes in change to nodes in reference. If there is an exact node match then change
   * "changed" by replacing the nodes with the equivalent nodes in reference.
   */
  void _mergeNodes(const boost::shared_ptr<Element>& changed,
    const RelationPtr& reference);

  void _unionOutline(const RelationPtr& building, boost::shared_ptr<geos::geom::Geometry> outline,
                     ElementPtr buildingMember);

};

}

#endif // BUILDINGOUTLINEUPDATEOP_H
