#ifndef EDGELOCATION_H
#define EDGELOCATION_H

// hoot
#include "NetworkEdge.h"

// Qt
#include <QHash>

// Tgs
#include <tgs/HashMap.h>

namespace hoot
{

/**
 * Represents a location on an edge. This assumes an edge can be represented as a line and addressed
 * as a portion [0, 1] along that line.
 *
 * This class parallels WayLocation.
 */
class EdgeLocation
{
public:
  // the suggested sloppy segment fraction distance from a vertex that is still considered on a
  // vertex.
  static const double SLOPPY_EPSILON;

  EdgeLocation(ConstNetworkEdgePtr e, double portion) : _e(e), _portion(portion) {}

  const ConstNetworkEdgePtr& getEdge() const { return _e; }

  double getPortion() const { return _portion; }

  /**
   * Returns true if the location is either at the start or end of an edge.
   */
  bool isExtreme(double epsilon = 0.0) const { return isFirst(epsilon) || isLast(epsilon); }

  bool isFirst(double epsilon = 0.0) const { return _portion <= epsilon; }

  bool isLast(double epsilon = 0.0) const { return _portion >= 1 - epsilon; }

  bool isValid() const { return _portion >= 0.0 && _portion <= 1.0; }

  /**
   * Returns the vertex at this location if isExtreme(epsilon) == true. Otherwise an exception is
   * thrown.
   */
  ConstNetworkVertexPtr getVertex(double epsilon = 0.0) const;

  QString toString() const;

private:

  ConstNetworkEdgePtr _e;
  double _portion;

};

typedef shared_ptr<EdgeLocation> EdgeLocationPtr;
typedef shared_ptr<const EdgeLocation> ConstEdgeLocationPtr;

inline bool operator==(const ConstEdgeLocationPtr& a, const ConstEdgeLocationPtr& b)
{
  return a->getEdge() == b->getEdge() && a->getPortion() == b->getPortion();
}

inline bool operator<(const ConstEdgeLocationPtr& a, const ConstEdgeLocationPtr& b)
{
  assert(a->getEdge() == b->getEdge());
  return a->getPortion() < b->getPortion();
}

inline bool operator>(const ConstEdgeLocationPtr& a, const ConstEdgeLocationPtr& b)
{
  assert(a->getEdge() == b->getEdge());
  return a->getPortion() > b->getPortion();
}

inline bool operator<=(const ConstEdgeLocationPtr& a, const ConstEdgeLocationPtr& b)
{
  assert(a->getEdge() == b->getEdge());
  return a->getPortion() <= b->getPortion();
}

inline bool operator>=(const ConstEdgeLocationPtr& a, const ConstEdgeLocationPtr& b)
{
  assert(a->getEdge() == b->getEdge());
  return a->getPortion() >= b->getPortion();
}

inline uint qHash(const ConstEdgeLocationPtr& el)
{
//  LOG_VARW(el);
//  LOG_VARW(qHash(el->getEdge()));
//  LOG_VARW(::qHash(__gnu_cxx::fastHashDouble()(el->getPortion())));
  uint result = ::qHash(Tgs::cantorPairing(qHash(el->getEdge()),
    ::qHash((quint64)__gnu_cxx::fastHashDouble()(el->getPortion()))));
//  LOG_VARW(result);
  return result;
}

}

#endif // EDGELOCATION_H
