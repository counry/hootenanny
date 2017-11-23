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
 * @copyright Copyright (C) 2015, 2016, 2017 DigitalGlobe (http://www.digitalglobe.com/)
 */
#include "ScriptMatchCreator.h"

// hoot
#include <hoot/core/util/Factory.h>
#include <hoot/core/conflate/MatchThreshold.h>
#include <hoot/core/conflate/MatchType.h>
#include <hoot/core/filters/ArbitraryCriterion.h>
#include <hoot/core/filters/ChainCriterion.h>
#include <hoot/core/filters/StatusCriterion.h>
#include <hoot/core/index/OsmMapIndex.h>
#include <hoot/core/schema/OsmSchema.h>
#include <hoot/core/util/ConfPath.h>
#include <hoot/core/util/ConfigOptions.h>
#include <hoot/core/visitors/ElementConstOsmMapVisitor.h>
#include <hoot/core/visitors/FilteredVisitor.h>
#include <hoot/core/visitors/WorstCircularErrorVisitor.h>
#include <hoot/core/visitors/IndexElementsVisitor.h>
#include <hoot/js/OsmMapJs.h>
#include <hoot/js/v8Engine.h>
#include <hoot/js/elements/ElementJs.h>
#include <hoot/js/util/SettingsJs.h>

// Qt
#include <QFileInfo>
#include <qnumeric.h>

// Boost
#include <boost/bind.hpp>

// Standard
#include <deque>
#include <math.h>

// TGS
#include <tgs/RStarTree/IntersectionIterator.h>
#include <tgs/RStarTree/MemoryPageStore.h>

#include "ScriptMatch.h"

using namespace geos::geom;
using namespace std;
using namespace Tgs;
using namespace v8;

namespace hoot
{

HOOT_FACTORY_REGISTER(MatchCreator, ScriptMatchCreator)

class ScriptMatchVisitor;

/**
 * Searches the specified map for any match potentials.
 */
class ScriptMatchVisitor : public ConstElementVisitor
{

public:

  ScriptMatchVisitor(const ConstOsmMapPtr& map, vector<const Match*>& result,
    ConstMatchThresholdPtr mt, boost::shared_ptr<PluginContext> script) :
    _map(map),
    _result(result),
    _mt(mt),
    _script(script),
    _customSearchRadius(-1.0)
  {
    _neighborCountMax = -1;
    _neighborCountSum = 0;
    _elementsEvaluated = 0;
    _elementsVisited = 0;
    _maxGroupSize = 0;

    Isolate* current = v8Engine::getIsolate();
    HandleScope handleScope(current);
    Local<Context> context(current->GetCurrentContext());
    Context::Scope context_scope(context);
    Persistent<Object> plugin(current, getPlugin(_script));
    _candidateDistanceSigma = getNumber(plugin, "candidateDistanceSigma", 0.0, 1.0);

    //this is meant to have been set externally in a js rules file
    _customSearchRadius = getNumber(plugin, "searchRadius", -1.0, 15.0);
    LOG_VART(_customSearchRadius);

    Handle<Value> value = plugin.Get(current)->Get(toV8("getSearchRadius"));
    if (value->IsUndefined())
    {
      // pass
    }
    else if (value->IsFunction() == false)
    {
      throw HootException("getSearchRadius is not a function.");
    }
    else
    {
      _getSearchRadius.Reset(current, Handle<Function>::Cast(value));
    }
  }

  ~ScriptMatchVisitor()
  {
  }

  void checkForMatch(const boost::shared_ptr<const Element>& e)
  {
    Isolate* current = v8Engine::getIsolate();
    HandleScope handleScope(current);
    Context::Scope context_scope(current->GetCurrentContext());
    Persistent<Object> plugin(current, getPlugin(_script));
    Local<Object> mapJs(ToLocal(&_mapJs));

    ConstOsmMapPtr map = getMap();

    // create an envlope around the e plus the search radius.
    auto_ptr<Envelope> env(e->getEnvelope(map));
    Meters searchRadius = getSearchRadius(e);
    env->expandBy(searchRadius);

    // find other nearby candidates
    set<ElementId> neighbors =
      IndexElementsVisitor::findNeighbors(*env, getIndex(), _indexToEid, getMap());
    ElementId from = e->getElementId();

    _elementsEvaluated++;

    for (set<ElementId>::const_iterator it = neighbors.begin(); it != neighbors.end(); ++it)
    {
      ConstElementPtr e2 = map->getElement(*it);
      if (isCorrectOrder(e, e2) && isMatchCandidate(e2))
      {
        // score each candidate and push it on the result vector
        ScriptMatch* m = new ScriptMatch(_script, plugin, map, mapJs, from, *it, _mt);
        // if we're confident this is a miss
        if (m->getType() == MatchType::Miss)
        {
          delete m;
        }
        else
        {
          _result.push_back(m);
        }
      }
    }

    _neighborCountSum += neighbors.size();
    _neighborCountMax = std::max(_neighborCountMax, (int)neighbors.size());
  }

  ConstOsmMapPtr getMap() const { return _map.lock(); }

  static double getNumber(Persistent<Object>& obj, QString key, double minValue, double defaultValue)
  {
    Isolate* current = v8Engine::getIsolate();
    HandleScope handleScope(current);
    Handle<Context> context(current->GetCurrentContext());
    Context::Scope context_scope(context);
    double result = defaultValue;
    Handle<String> cdtKey = String::NewFromUtf8(current, key.toUtf8());
    if (obj.Get(current)->Has(cdtKey))
    {
      Local<Value> v = obj.Get(current)->Get(cdtKey);
      if (v->IsNumber() == false || ::qIsNaN(v->NumberValue()))
      {
        throw IllegalArgumentException("Expected " + key + " to be a number.");
      }
      result = v->NumberValue();

      if (result < minValue)
      {
        throw IllegalArgumentException(QString("Expected %1 to be greater than %2.")
                                       .arg(key).arg(minValue));
      }
    }
    return result;
  }

  static Local<Object> getPlugin(boost::shared_ptr<PluginContext> script)
  {
    Isolate* current = v8Engine::getIsolate();
    EscapableHandleScope handleScope(current);
    Handle<Context> context(script->getContext(current));
    Context::Scope context_scope(context);
    Handle<Object> global = context->Global();

    if (global->Has(String::NewFromUtf8(current, "plugin")) == false)
    {
      throw IllegalArgumentException("Expected the script to have exports.");
    }

    Handle<Value> pluginValue = global->Get(String::NewFromUtf8(current, "plugin"));
    Handle<Object> plugin(Handle<Object>::Cast(pluginValue));
    if (plugin.IsEmpty() || plugin->IsObject() == false)
    {
      throw IllegalArgumentException("Expected plugin to be a valid object.");
    }

    return handleScope.Escape(plugin);
  }

  // See the "Calculating Search Radius" section in the user docs for more information.
  Meters getSearchRadius(const ConstElementPtr& e)
  {
    Meters result;
    if (_getSearchRadius.IsEmpty())
    {
      if (_customSearchRadius < 0)
      {
        //base the radius off of the element itself
        result = e->getCircularError() * _candidateDistanceSigma;
      }
      else
      {
        //base the radius off some predefined radius
        result = _customSearchRadius * _candidateDistanceSigma;
      }
    }
    else
    {
      if (_searchRadiusCache.contains(e->getElementId()))
      {
        result = _searchRadiusCache[e->getElementId()];
      }
      else
      {
        Isolate* current = v8Engine::getIsolate();
        HandleScope handleScope(current);
        Context::Scope context_scope(_script->getContext(current));

        Handle<Value> jsArgs[1];

        int argc = 0;
        jsArgs[argc++] = ElementJs::New(e);

        LOG_TRACE("Calling getSearchRadius...");
        Persistent<Object> plugin(current, getPlugin(_script));
        Local<Object> local_plugin(ToLocal(&plugin));
        Local<Function> searchRadius(ToLocal(&_getSearchRadius));
        Handle<Value> f = searchRadius->Call(local_plugin, argc, jsArgs);

        result = toCpp<Meters>(f) * _candidateDistanceSigma;

        _searchRadiusCache[e->getElementId()] = result;
      }
    }

    LOG_VART(result);
    return result;
  }

  /*
   * This is meant to run one time when the match creator is initialized.
   */
  void calculateSearchRadius()
  {
    Isolate* current = v8Engine::getIsolate();
    HandleScope handleScope(current);
    Context::Scope context_scope(_script->getContext(current));

    Persistent<Object> plugin(current, getPlugin(_script));
    Handle<String> initStr = String::NewFromUtf8(current, "calculateSearchRadius");
    //optional method, so don't throw an error
    if (plugin.Get(current)->Has(initStr) == false)
    {
      LOG_TRACE("calculateSearchRadius function not present.");
      return;
    }
    Handle<Value> value = plugin.Get(current)->Get(initStr);
    if (value->IsFunction() == false)
    {
      LOG_TRACE("calculateSearchRadius function not present.");
      return;
    }

    Handle<Function> func = Handle<Function>::Cast(value);
    Handle<Value> jsArgs[1];
    int argc = 0;
    HandleScope scope(current);
    assert(getMap().get());
    OsmMapPtr copiedMap(new OsmMap(getMap()));
    jsArgs[argc++] = OsmMapJs::create(copiedMap);

    func->Call(plugin.Get(current), argc, jsArgs);

    //this is meant to have been set externally in a js rules file
    _customSearchRadius = getNumber(plugin, "searchRadius", -1.0, 15.0);
    LOG_VART(_customSearchRadius);

    QFileInfo scriptFileInfo(_scriptPath);
    LOG_DEBUG("Search radius calculation complete for " << scriptFileInfo.fileName());
  }

  void cleanMapCache()
  {
    _mapJs.ClearWeak();
  }

  boost::shared_ptr<HilbertRTree>& getIndex()
  {
    if (!_index)
    {
      // No tuning was done, I just copied these settings from OsmMapIndex.
      // 10 children - 368
      boost::shared_ptr<MemoryPageStore> mps(new MemoryPageStore(728));
      _index.reset(new HilbertRTree(mps, 2));

      // Only index elements that satisfy the isMatchCandidate
      // previously we only indexed Unknown2, but that causes issues when wanting to conflate
      // from n datasets and support intradataset conflation. This approach over-indexes a bit and
      // will likely slow things down, but should give the same results.
      // An option in the future would be to support an "isIndexedFeature" or similar function
      // to speed the operation back up again.
      boost::function<bool (ConstElementPtr e)> f =
        boost::bind(&ScriptMatchVisitor::isMatchCandidate, this, _1);
      boost::shared_ptr<ArbitraryCriterion> pC(new ArbitraryCriterion(f));

      // Instantiate our visitor
      IndexElementsVisitor v(_index,
                             _indexToEid,
                             pC,
                             boost::bind(&ScriptMatchVisitor::getSearchRadius, this, _1),
                             getMap());

      // Do the visiting
      getMap()->visitRo(v);
      v.finalizeIndex();
    }

    return _index;
  }

  Local<Object> getOsmMapJs()
  {
    if (_mapJs.IsEmpty())
    {
      _mapJs.Reset(v8Engine::getIsolate(), OsmMapJs::create(getMap()));
    }
    return ToLocal(&_mapJs);
  }

  /**
   * Returns true if e1, e2 is in the correct ordering for matching. This does a few things:
   *
   *  - Avoid comparing e1 to e2 and e2 to e1
   *  - The Unknown1/Input1 is always e1. This is a requirement for some of the older code.
   *  - Gives a consistent ordering to allow backwards compatibility with system tests.
   */
  bool isCorrectOrder(const ConstElementPtr& e1, const ConstElementPtr& e2)
  {
    if (e1->getStatus().getEnum() == e2->getStatus().getEnum())
    {
      return e1->getElementId() < e2->getElementId();
    }
    else
    {
      return e1->getStatus().getEnum() < e2->getStatus().getEnum();
    }
  }

  bool isMatchCandidate(ConstElementPtr e)
  {
    if (_matchCandidateCache.contains(e->getElementId()))
    {
      return _matchCandidateCache[e->getElementId()];
    }

    Isolate* current = v8Engine::getIsolate();
    HandleScope handleScope(current);
    Context::Scope context_scope(_script->getContext(current));
    Persistent<Object> plugin(current, getPlugin(_script));
    Local<Object> local_plugin(ToLocal(&plugin));
    Handle<String> isMatchCandidateStr = String::NewFromUtf8(current, "isMatchCandidate");
    if (local_plugin->Has(isMatchCandidateStr) == false)
    {
      throw HootException("Error finding 'isMatchCandidate' function.");
    }
    Handle<Value> value = local_plugin->Get(isMatchCandidateStr);
    if (value->IsFunction() == false)
    {
      throw HootException("isMatchCandidate is not a function.");
    }
    Handle<Function> func = Handle<Function>::Cast(value);
    Handle<Value> jsArgs[2];

    Local<Object> osmMapJs(getOsmMapJs());
    int argc = 0;
    jsArgs[argc++] = osmMapJs;
    jsArgs[argc++] = ElementJs::New(e);

    Handle<Value> f = func->Call(local_plugin, argc, jsArgs);

    bool result = f->BooleanValue();
    _matchCandidateCache[e->getElementId()] = result;
    return result;
  }

  virtual void visit(const ConstElementPtr& e)
  {
    if (isMatchCandidate(e))
    {
      checkForMatch(e);
    }
    _elementsVisited++;
    if (_elementsVisited % 10 == 0 && Log::getInstance().getLevel() <= Log::Info)
    {
      cout << "Progress: " << _elementsVisited <<
              " _neighborCountSum: " << _neighborCountSum << "          \r";
      cout << flush;
    }
  }

  void setScriptPath(QString path) { _scriptPath = path; }
  QString getScriptPath() const { return _scriptPath; }

  Meters getCustomSearchRadius() const { return _customSearchRadius; }
  void setCustomSearchRadius(Meters searchRadius) { _customSearchRadius = searchRadius; }

private:

  // don't hold on to the map.
  boost::weak_ptr<const OsmMap> _map;
  Persistent<Object> _mapJs;
  vector<const Match*>& _result;
  set<ElementId> _empty;
  int _neighborCountMax;
  int _neighborCountSum;
  long _elementCount;
  int _elementsEvaluated;
  long _elementsVisited;
  size_t _maxGroupSize;
  ConstMatchThresholdPtr _mt;
  boost::shared_ptr<PluginContext> _script;
  Persistent<Function> _getSearchRadius;

  QHash<ElementId, bool> _matchCandidateCache;
  QHash<ElementId, double> _searchRadiusCache;

  // Used for finding neighbors
  boost::shared_ptr<HilbertRTree> _index;
  deque<ElementId> _indexToEid;

  double _candidateDistanceSigma;
  //used for automatic search radius calculation; it is expected that this is set from the
  //Javascript rules file used for the generic conflation
  double _customSearchRadius;
  QString _scriptPath;
};

ScriptMatchCreator::ScriptMatchCreator()
{
  _cachedScriptVisitor.reset();
}

ScriptMatchCreator::~ScriptMatchCreator()
{
}

Meters ScriptMatchCreator::calculateSearchRadius(const ConstOsmMapPtr& map,
  const ConstElementPtr& e)
{
  return _getCachedVisitor(map)->getSearchRadius(e);
}

void ScriptMatchCreator::setArguments(QStringList args)
{
  if (args.size() != 1)
  {
    throw HootException("The ScriptMatchCreator takes exactly one argument (script path).");
  }

  Isolate* current = v8Engine::getIsolate();
  HandleScope handleScope(current);
  _scriptPath = ConfPath::search(args[0], "rules");
  _script.reset(new PluginContext());
  Context::Scope context_scope(_script->getContext(current));
  _script->loadScript(_scriptPath, "plugin");
  //bit of a hack...see MatchCreator.h...need to refactor
  _description = QString::fromStdString(className()) + "," + args[0];
  _cachedScriptVisitor.reset();

  QFileInfo scriptFileInfo(_scriptPath);
  LOG_DEBUG("Set arguments for: " << className() << " - rules: " << scriptFileInfo.fileName());
}

Match* ScriptMatchCreator::createMatch(const ConstOsmMapPtr& map, ElementId eid1, ElementId eid2)
{
  if (isMatchCandidate(map->getElement(eid1), map) && isMatchCandidate(map->getElement(eid2), map))
  {
    Isolate* current = v8Engine::getIsolate();
    HandleScope handleScope(current);
    Context::Scope context_scope(_script->getContext(current));

    Handle<Object> mapJs = OsmMapJs::create(map);
    Persistent<Object> plugin(current, ScriptMatchVisitor::getPlugin(_script));

    return new ScriptMatch(_script, plugin, map, mapJs, eid1, eid2, getMatchThreshold());
  }
  return 0;
}

void ScriptMatchCreator::createMatches(const ConstOsmMapPtr& map, vector<const Match *> &matches,
                                       ConstMatchThresholdPtr threshold)
{
  if (!_script)
  {
    throw IllegalArgumentException("The script must be set on the ScriptMatchCreator.");
  }

  ScriptMatchVisitor v(map, matches, threshold, _script);
  v.setScriptPath(_scriptPath);
  v.calculateSearchRadius();
  _cachedCustomSearchRadii[_scriptPath] = v.getCustomSearchRadius();
  LOG_VART(_scriptPath);
  LOG_VART(_cachedCustomSearchRadii[_scriptPath]);
  QFileInfo scriptFileInfo(_scriptPath);
  LOG_INFO(
    "Creating matches with: " << className() << "; rules: " << scriptFileInfo.fileName() << "...");
  LOG_VARD(*threshold);
  map->visitRo(v);
}

vector<MatchCreator::Description> ScriptMatchCreator::getAllCreators() const
{
  vector<Description> result;

  // find all the files that end with .js in [ConfPath]/rules/
  QStringList scripts = ConfPath::find(QStringList() << "*.js", "rules");

  // go through each script
  for (int i = 0; i < scripts.size(); i++)
  {
    try
    {
      // if the script is a valid generic script w/ 'description' exposed, add it to the list.
      Description d = _getScriptDescription(scripts[i]);
      if (!d.description.isEmpty())
      {
        result.push_back(d);
        LOG_TRACE(d.description);
      }
      else
      {
        LOG_TRACE(QString("Skipping reporting script %1 because it has no "
          "description.").arg(scripts[i]));
      }
    }
    catch (const HootException& e)
    {
      LOG_WARN("Error loading script: " + scripts[i] + " exception: " + e.getWhat());
    }
  }

  return result;
}

boost::shared_ptr<ScriptMatchVisitor> ScriptMatchCreator::_getCachedVisitor(
  const ConstOsmMapPtr& map)
{
  if (!_cachedScriptVisitor.get() || _cachedScriptVisitor->getMap() != map)
  {
    LOG_VART(_cachedScriptVisitor.get());
    QString scriptPath = _scriptPath;
    if (_cachedScriptVisitor.get())
    {
      LOG_VART(_cachedScriptVisitor->getMap() == map);
      scriptPath = _cachedScriptVisitor->getScriptPath();
    }
    LOG_VART(scriptPath);

    QFileInfo scriptFileInfo(_scriptPath);
    LOG_TRACE("Resetting the match candidate checker " << scriptFileInfo.fileName() << "...");

    vector<const Match*> emptyMatches;
    _cachedScriptVisitor.reset(
      new ScriptMatchVisitor(map, emptyMatches, ConstMatchThresholdPtr(), _script));
    _cachedScriptVisitor->setScriptPath(scriptPath);
    //If the search radius has already been calculated for this matcher once, we don't want to do
    //it again due to the expense.
    LOG_VART(_cachedCustomSearchRadii.contains(scriptPath));
    if (!_cachedCustomSearchRadii.contains(scriptPath))
    {
      _cachedScriptVisitor->calculateSearchRadius();
    }
    else
    {
      LOG_VART(_cachedCustomSearchRadii[scriptPath]);
      _cachedScriptVisitor->setCustomSearchRadius(_cachedCustomSearchRadii[scriptPath]);
    }
  }

  return _cachedScriptVisitor;
}

MatchCreator::Description ScriptMatchCreator::_getScriptDescription(QString path) const
{
  MatchCreator::Description result;
  result.experimental = true;

  boost::shared_ptr<PluginContext> script(new PluginContext());
  Isolate* current = v8Engine::getIsolate();
  HandleScope handleScope(current);
  Context::Scope context_scope(script->getContext(current));
  script->loadScript(path, "plugin");

  Persistent<Object> plugin(current, ScriptMatchVisitor::getPlugin(script));
  Handle<String> descriptionStr = String::NewFromUtf8(current, "description");
  if (plugin.Get(current)->Has(descriptionStr))
  {
    Handle<Value> value = plugin.Get(current)->Get(descriptionStr);
    result.description = toCpp<QString>(value);
  }
  Handle<String> experimentalStr = String::NewFromUtf8(current, "experimental");
  if (plugin.Get(current)->Has(experimentalStr))
  {
    Handle<Value> value = plugin.Get(current)->Get(experimentalStr);
    result.experimental = toCpp<bool>(value);
  }
  Handle<String> featureTypeStr = String::NewFromUtf8(current, "baseFeatureType");
  if (plugin.Get(current)->Has(featureTypeStr))
  {
    Handle<Value> value = plugin.Get(current)->Get(featureTypeStr);
    result.baseFeatureType = MatchCreator::StringToBaseFeatureType(toCpp<QString>(value));
  }

  QFileInfo fi(path);
  result.className = (QString::fromStdString(className()) + "," + fi.fileName()).toStdString();

  return result;
}

bool ScriptMatchCreator::isMatchCandidate(ConstElementPtr element, const ConstOsmMapPtr& map)
{
  if (!_script)
  {
    throw IllegalArgumentException("The script must be set on the ScriptMatchCreator.");
  }

  return _getCachedVisitor(map)->isMatchCandidate(element);
}

boost::shared_ptr<MatchThreshold> ScriptMatchCreator::getMatchThreshold()
{
  if (!_matchThreshold.get())
  {
    if (!_script)
    {
      throw IllegalArgumentException("The script must be set on the ScriptMatchCreator.");
    }
    Isolate* current = v8Engine::getIsolate();
    HandleScope handleScope(current);
    Context::Scope context_scope(_script->getContext(current));
    Persistent<Object> plugin(current, ScriptMatchVisitor::getPlugin(_script));

    double matchThreshold = -1.0;
    double missThreshold = -1.0;
    double reviewThreshold = -1.0;
    try
    {
      matchThreshold = ScriptMatchVisitor::getNumber(plugin, "matchThreshold", 0.0, 1.0);
      missThreshold = ScriptMatchVisitor::getNumber(plugin, "missThreshold", 0.0, 1.0);
      reviewThreshold = ScriptMatchVisitor::getNumber(plugin, "reviewThreshold", 0.0, 1.0);
    }
    catch (const IllegalArgumentException& e)
    {
    }

    if (matchThreshold != -1.0 && missThreshold != -1.0 && reviewThreshold != -1.0)
    {
      return boost::shared_ptr<MatchThreshold>(
        new MatchThreshold(matchThreshold, missThreshold, reviewThreshold));
    }
    else
    {
      return boost::shared_ptr<MatchThreshold>(new MatchThreshold());
    }
  }
  return _matchThreshold;
}

}
