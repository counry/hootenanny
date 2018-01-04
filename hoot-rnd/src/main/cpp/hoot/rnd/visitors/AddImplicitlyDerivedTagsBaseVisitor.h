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
 * @copyright Copyright (C) 2017 DigitalGlobe (http://www.digitalglobe.com/)
 */
#ifndef ADDIMPLICITLYDERIVEDTAGSBASEVISITOR_H
#define ADDIMPLICITLYDERIVEDTAGSBASEVISITOR_H

// hoot
#include <hoot/core/elements/ElementVisitor.h>
#include <hoot/rnd/io/ImplicitTagRulesSqliteReader.h>
#include <hoot/rnd/schema/PoiImplicitTagCustomRules.h>
#include <hoot/core/util/Configurable.h>

namespace hoot
{

/**
 * Adds tags implicitly derived from POI names to POIs
 */
class AddImplicitlyDerivedTagsBaseVisitor : public ElementVisitor, public Configurable
{
public:

  AddImplicitlyDerivedTagsBaseVisitor();
  AddImplicitlyDerivedTagsBaseVisitor(const QString databasePath);
  ~AddImplicitlyDerivedTagsBaseVisitor();

  /**
   * Adds implicitly derived tags to an element
   *
   * @param e element to add derived tags to
   */
  virtual void visit(const ElementPtr& e);

  virtual void setConfiguration(const Settings& conf);

  void setCustomRuleFile(const QString file) { _customRules.setCustomRuleFile(file); }
  void setTagIgnoreFile(const QString file) { _customRules.setTagIgnoreFile(file); }
  void setWordIgnoreFile(const QString file) { _customRules.setWordIgnoreFile(file); }
  void setTranslateAllNamesToEnglish(bool translate) { _translateAllNamesToEnglish = translate; }
  void setMatchEndOfNameSingleTokenFirst(bool match) { _matchEndOfNameSingleTokenFirst = match; }
  void setAllowTaggingSpecificPois(bool allow) { _allowTaggingSpecificPois = allow; }
  void setMinWordLength(int length) { _minWordLength = length; }

protected:

  virtual bool _visitElement(const ElementPtr& e) = 0;

  bool _allowTaggingSpecificPois;
  bool _elementIsASpecificPoi;

private:

  boost::shared_ptr<ImplicitTagRulesSqliteReader> _ruleReader;

  long _numNodesModified;
  long _numTagsAdded;
  long _numNodesInvolvedInMultipleRules;
  long _numNodesParsed;
  long _statusUpdateInterval;
  int _minWordLength;
  long _smallestNumberOfTagsAdded;
  long _largestNumberOfTagsAdded;
  bool _translateAllNamesToEnglish;
  bool _matchEndOfNameSingleTokenFirst;

  PoiImplicitTagCustomRules _customRules;

  QSet<QString> _getNameTokens(const QStringList names);

};

}

#endif // ADDIMPLICITLYDERIVEDTAGSBASEVISITOR_H
