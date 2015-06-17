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
 * @copyright Copyright (C) 2013, 2014 DigitalGlobe (http://www.digitalglobe.com/)
 */
#ifndef PERTY_H
#define PERTY_H

// geos
#include <geos/geom/Envelope.h>

// hoot
#include <hoot/core/Units.h>
#include <hoot/core/ops/OsmMapOperation.h>
#include <hoot/core/util/Configurable.h>

// OpenCV
#include <opencv/cv.h>

// Qt
#include <QString>
#include <QStringList>

namespace hoot
{
using namespace cv;
using namespace geos::geom;

class PermuteGridCalculator;

/**
 * Performs a perty style permutation on the data. The specifics of which operations are performed
 * can be specified via the "perty.ops" configuration setting.
 *
 * The geometry permutations are done in accordance with [1].
 *
 * 1. Evaluating conflation methods using uncertainty modeling - Peter Doucette, et al. 2013
 *    https://insightcloud.digitalglobe.com/redmine/attachments/download/1667/2013%20Evaluating%20conflation%20methods%20using%20uncertainty%20modeling.pdf
 *    http://proceedings.spiedigitallibrary.org/proceeding.aspx?articleid=1691369
 */
class PertyOp : public OsmMapOperation, public Configurable
{
public:

  static string className() { return "hoot::PertyOp"; }

  PertyOp();

  virtual ~PertyOp() {}

  virtual void setConfiguration(const Settings& conf);

  /**
   * Permute the map and then apply all "perty.ops" to the map as well.
   */
  virtual void apply(shared_ptr<OsmMap>& map);

  /**
   * Generates a map of all the grid offset vectors and permutes the given map.
   */
  shared_ptr<OsmMap> generateDebugMap(shared_ptr<OsmMap>& map);

  void permute(const shared_ptr<OsmMap>& map);

  void setCsmParameters(double beta, double D) { _beta = beta; _D = D; }

  void setGridSpacing(Meters gridSpacing) { _gridSpacing = gridSpacing; }

  /**
   * Sets a list of operations that should be run after the permute method is called.
   */
  void setNamedOps(QStringList namedOps) { _namedOps = namedOps; }

  /**
   * Set the permutation algorithm to one of:
   * - "DirectSequentialSimulation" (default)
   * - "FullCovariance" (older and slower)
   */
  void setPermuteAlgorithm(QString algo);

  /**
   * Sets the random error. This is the sigma value for Rx and Ry. The same sigma value is used
   * for all values in each matrix. See [1] for more information.
   * @note There were problems with taking the sqrtm when sigma is zero. Now I make sigma a small
   * value (1e-6) when sigma is zero.
   */
  void setRandomError(Meters sigmaX, Meters sigmaY);

  /**
   * Seeds the permutation process. By default a seed is generated based on time. The seed should
   * be non-negative or -1 to generate a seed based on time.
   */
  void setSeed(int seed) { _seed = seed; }

  /**
   * Sets the systematic error. This is the sigma value for Sx and Sy. The same sigma value is used
   * for all values in each matrix. See [1] for more information.
   */
  void setSystematicError(Meters sigmaX, Meters sigmaY) { _sigmaSx = sigmaX; _sigmaSy = sigmaY; }

  /**
    @see OsmMapOperation
    */
  QString toString();

private:

  /// values used in the Community Sensor Model.
  double _beta;
  Meters _D;

  Meters _gridSpacing;
  int _seed;
  Meters _sigmaRx, _sigmaRy;
  Meters _sigmaSx, _sigmaSy;
  QString _permuteAlgorithm;
  shared_ptr<PermuteGridCalculator> _gridCalculator;
  QStringList _namedOps;

  Settings& _settings;

  void _configure();

  Mat _calculatePermuteGrid(geos::geom::Envelope env, int& rows, int& cols);
};

}

#endif // PERTY_H
