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
#include "PoiImplicitTagRulesDeriver.h"

// hoot
#include <hoot/core/elements/Tags.h>
#include <hoot/core/util/StringUtils.h>
#include <hoot/core/util/ConfigOptions.h>
#include <hoot/core/util/HootException.h>

// Qt
#include <QStringBuilder>

namespace hoot
{

PoiImplicitTagRulesDeriver::PoiImplicitTagRulesDeriver() :
_statusUpdateInterval(ConfigOptions().getTaskStatusUpdateInterval()),
_minTagOccurrencesPerWord(1),
_minWordLength(1)
{
}

void PoiImplicitTagRulesDeriver::setConfiguration(const Settings& conf)
{
  const ConfigOptions confOptions(conf);
  setCustomRuleFile(confOptions.getPoiImplicitTagRulesCustomRuleFile());
  setMinTagOccurrencesPerWord(confOptions.getPoiImplicitTagRulesMinimumTagOccurrencesPerWord());
  setMinWordLength(confOptions.getPoiImplicitTagRulesMinimumWordLength());
  setRuleIgnoreFile(confOptions.getPoiImplicitTagRulesRuleIgnoreFile());
  setTagIgnoreFile(confOptions.getPoiImplicitTagRulesTagIgnoreFile());
  setTagFile(confOptions.getPoiImplicitTagRulesTagFile());
  setWordIgnoreFile(confOptions.getPoiImplicitTagRulesWordIgnoreFile());
}

void PoiImplicitTagRulesDeriver::_readAllowLists()
{
  LOG_DEBUG("Reading allow lists...");

  if (!_tagFile.trimmed().isEmpty())
  {
    QFile tagsAllowFile(_tagFile);
    if (!tagsAllowFile.open(QIODevice::ReadOnly))
    {
      throw HootException(
        QObject::tr("Error opening %1 for writing.").arg(tagsAllowFile.fileName()));
    }
    _tagsAllowList.clear();
    while (!tagsAllowFile.atEnd())
    {
      const QString line = QString::fromUtf8(tagsAllowFile.readLine().constData()).trimmed();
      if (!line.isEmpty() && !line.startsWith("#"))
      {
        _tagsAllowList.append(line);
      }
    }
    tagsAllowFile.close();
  }
  LOG_VART(_tagsAllowList);

  if (!_customRuleFile.trimmed().isEmpty())
  {
    QFile customRulesFile(_customRuleFile);
    if (!customRulesFile.open(QIODevice::ReadOnly))
    {
      throw HootException(
        QObject::tr("Error opening %1 for writing.").arg(customRulesFile.fileName()));
    }
    _customRulesList.clear();
    while (!customRulesFile.atEnd())
    {
      const QString line = QString::fromUtf8(customRulesFile.readLine().constData()).trimmed();
      if (!line.isEmpty() && !line.startsWith("#"))
      {
        const QStringList lineParts = line.trimmed().split("\t");
        _customRulesList[lineParts[0].trimmed()] = lineParts[1].trimmed();
      }
    }
    customRulesFile.close();
  }
  LOG_VART(_customRulesList);
}

void PoiImplicitTagRulesDeriver::_readIgnoreLists()
{
  LOG_DEBUG("Reading ignore lists...");

  if (!_tagIgnoreFile.trimmed().isEmpty())
  {
    QFile tagIgnoreFile(_tagIgnoreFile);
    if (!tagIgnoreFile.open(QIODevice::ReadOnly))
    {
      throw HootException(
        QObject::tr("Error opening %1 for writing.").arg(tagIgnoreFile.fileName()));
    }
    _tagIgnoreList.clear();
    while (!tagIgnoreFile.atEnd())
    {
      const QString line = QString::fromUtf8(tagIgnoreFile.readLine().constData()).trimmed();
      if (!line.isEmpty() && !line.startsWith("#"))
      {
        _tagIgnoreList.append(line);
      }
    }
    tagIgnoreFile.close();
  }
  LOG_VART(_tagIgnoreList);

  if (!_wordIgnoreFile.trimmed().isEmpty())
  {
    QFile wordIgnoreFile(_wordIgnoreFile);
    if (!wordIgnoreFile.open(QIODevice::ReadOnly))
    {
      throw HootException(
        QObject::tr("Error opening %1 for writing.").arg(wordIgnoreFile.fileName()));
    }
    _wordIgnoreList.clear();
    while (!wordIgnoreFile.atEnd())
    {
      const QString line = QString::fromUtf8(wordIgnoreFile.readLine().constData()).trimmed();
      if (!line.isEmpty() && !line.startsWith("#"))
       {
        _wordIgnoreList.append(line);
      }
    }
    wordIgnoreFile.close();
  }
  LOG_VART(_wordIgnoreList);

  if (!_ruleIgnoreFile.trimmed().isEmpty())
  {
    QFile rulesIgnoreFile(_ruleIgnoreFile);
    if (!rulesIgnoreFile.open(QIODevice::ReadOnly))
    {
      throw HootException(
        QObject::tr("Error opening %1 for writing.").arg(rulesIgnoreFile.fileName()));
    }
    _rulesIgnoreList.clear();
    while (!rulesIgnoreFile.atEnd())
    {
      const QString line = QString::fromUtf8(rulesIgnoreFile.readLine().constData()).trimmed();
      if (!line.isEmpty() && !line.startsWith("#"))
      {
        const QStringList lineParts = line.trimmed().split("\t");
        _rulesIgnoreList[lineParts[0].trimmed()] = lineParts[1].trimmed();
      }
    }
    rulesIgnoreFile.close();
  }
  LOG_VART(_rulesIgnoreList);
}

QString PoiImplicitTagRulesDeriver::_getSqliteOutput(const QStringList outputs)
{
  for (int i = 0; i < outputs.size(); i++)
  {
    if (outputs.at(i).endsWith(".sqlite"))
    {
      return outputs.at(i);
    }
  }
  return "";
}

void PoiImplicitTagRulesDeriver::deriveRules(const QString input, const QStringList outputs)
{
  if (input.isEmpty())
  {
    throw HootException("No input was specified.");
  }
  if (!input.endsWith(".implicitTagRules"))
  {
    throw IllegalArgumentException(
      QString("A *.implicitTagRules file must be the input to implicit tag rules derivation.  ") +
      QString("Input specified: ") + input);
  }

  if (outputs.isEmpty())
  {
    throw HootException("No outputs were specified.");
  }
  const QString sqliteOutput = _getSqliteOutput(outputs);
  if (sqliteOutput.isEmpty())
  {
    throw HootException("Outputs must contain at least one Sqlite database.");
  }

  LOG_INFO(
    "Deriving POI implicit tag rules for input: " << input << ".  Writing to outputs: " <<
    outputs << "...");
  LOG_VAR(_minTagOccurrencesPerWord);
  LOG_VAR(_minWordLength);
  LOG_VAR(_wordIgnoreFile);
  LOG_VAR(_tagIgnoreFile);
  LOG_VAR(_tagFile);
  LOG_VAR(_customRuleFile);
  LOG_VAR(_ruleIgnoreFile);

  _readIgnoreLists();
  _readAllowLists();

  LOG_VAR(_wordIgnoreList.size());
  LOG_VAR(_wordIgnoreList);
  LOG_VAR(_tagIgnoreList.size());
  LOG_VAR(_tagIgnoreList);
  LOG_VAR(_tagsAllowList.size());
  LOG_VAR(_tagsAllowList);
  LOG_VAR(_customRulesList.size());
  LOG_VAR(_customRulesList);
  LOG_VAR(_rulesIgnoreList.size());
  LOG_VAR(_rulesIgnoreList);

  if (_minTagOccurrencesPerWord == 1 && _minWordLength == 1 && _wordIgnoreList.size() == 0 &&
      _tagIgnoreList.size() == 0 && _tagsAllowList.size() == 0 && _customRulesList.size() == 0 &&
      _rulesIgnoreList.size() == 0)
  {
    LOG_INFO("Skipping filtering as no filtering criteria were specified...");
    if (_minTagOccurrencesPerWord >= 2)
    {
      _removeKvpsBelowOccurrenceThreshold(input, _minTagOccurrencesPerWord);
      _writeRules(outputs, _thresholdedCountFile->fileName());
    }
    else
    {
      LOG_INFO("Skipping count thresholding since threshold = 1...");
      _writeRules(outputs, input);
    }
  }
  else
  {
    if (_minTagOccurrencesPerWord >= 2)
    {
      _removeKvpsBelowOccurrenceThreshold(input, _minTagOccurrencesPerWord);
      _applyFiltering(_thresholdedCountFile->fileName());
    }
    else
    {
      LOG_INFO("Skipping count thresholding since threshold = 1...");
      _applyFiltering(input);
    }

  //  LOG_INFO(
  //    "Extracted "  << StringUtils::formatLargeNumber(_wordKeysToCounts.size()) <<
  //    " word/tag associations.");
  //  _wordKeysToCounts.clear();

    _writeRules(outputs, _filteredCountFile->fileName());
  }
}

void PoiImplicitTagRulesDeriver::_writeRules(const QStringList outputs,
                                             const QString inputFile)
{
  for (int i = 0; i < outputs.size(); i++)
  {
    const QString output = outputs.at(i);
    LOG_VART(output);
    boost::shared_ptr<ImplicitTagRuleWordPartWriter> ruleWordPartWriter =
      ImplicitTagRuleWordPartWriterFactory::getInstance().createWriter(output);
    ruleWordPartWriter->open(output);
    ruleWordPartWriter->write(inputFile);
    ruleWordPartWriter->close();
  }
}

void PoiImplicitTagRulesDeriver::_removeKvpsBelowOccurrenceThreshold(const QString input,
  const int minOccurrencesThreshold)
{
  LOG_INFO("Removing tags below minimum occurrence threshold of: " +
           QString::number(minOccurrencesThreshold) << "...");

  _thresholdedCountFile.reset(
    new QTemporaryFile(
      ConfigOptions().getApidbBulkInserterTempFileDir() +
      "/poi-implicit-tag-rules-deriver-temp-XXXXXX"));
  _thresholdedCountFile->setAutoRemove(!ConfigOptions().getPoiImplicitTagRulesKeepTempFiles());
  if (!_thresholdedCountFile->open())
  {
    throw HootException(
      QObject::tr("Error opening %1 for writing.").arg(_thresholdedCountFile->fileName()));
  }
  LOG_DEBUG("Opened thresholded temp file: " << _thresholdedCountFile->fileName());
  if (ConfigOptions().getPoiImplicitTagRulesKeepTempFiles())
  {
    LOG_WARN("Keeping temp file: " << _thresholdedCountFile->fileName());
  }

  //This removes lines with occurrence counts below the specified threshold - not sure why 1 needs
  //to be subtracted from the min occurrences here, though...
  const QString cmd =
    "cat " + input + " | awk -v limit=" + QString::number(minOccurrencesThreshold - 1) +
    " '$1 > limit{print}' > " + _thresholdedCountFile->fileName();
  LOG_DEBUG(cmd);
  if (std::system(cmd.toStdString().c_str()) != 0)
  {
    throw HootException("Unable to sort input file.");
  }
  _thresholdedCountFile->close();
}

void PoiImplicitTagRulesDeriver::_applyFiltering(const QString input)
{
  LOG_INFO("Applying word/tag/rule filtering to output...");

  _filteredCountFile.reset(
    new QTemporaryFile(
      ConfigOptions().getApidbBulkInserterTempFileDir() +
      "/poi-implicit-tag-rules-deriver-temp-XXXXXX"));
  _filteredCountFile->setAutoRemove(!ConfigOptions().getPoiImplicitTagRulesKeepTempFiles());
  if (!_filteredCountFile->open())
  {
    throw HootException(
      QObject::tr("Error opening %1 for writing.").arg(_filteredCountFile->fileName()));
  }
  LOG_DEBUG("Opened filtered temp file: " << _filteredCountFile->fileName());
  if (ConfigOptions().getPoiImplicitTagRulesKeepTempFiles())
  {
    LOG_WARN("Keeping temp file: " << _filteredCountFile->fileName());
  }
  QFile inputFile(input);
  if (!inputFile.open(QIODevice::ReadOnly))
  {
    throw HootException(QObject::tr("Error opening %1 for reading.").arg(input));
  }
  LOG_DEBUG("Opened input file: " << input);

  long linesParsedCount = 0;
  long linesWrittenCount = 0;
  long wordsTooSmallCount = 0;
  long ignoredWordsCount = 0;
  long ignoredTagsCount = 0;
  long ignoredRuleCount = 0;

  while (!inputFile.atEnd())
  {
    const QString line = QString::fromUtf8(inputFile.readLine().constData()).trimmed();
    LOG_VART(line);
    const QStringList lineParts = line.split("\t");
    LOG_VART(lineParts);
    QString word = lineParts[1].trimmed();
    LOG_VART(word);

    const bool wordTooSmall = word.length() < _minWordLength;
    if (!wordTooSmall && !_wordIgnoreList.contains(word, Qt::CaseInsensitive))
    {
      const QString kvp = lineParts[2].trimmed();
      LOG_VART(kvp);
      const QString tagKey = kvp.split("=")[0];
      LOG_VART(tagKey);

      const QString keyWildCard = tagKey % "=*";
      LOG_VART(keyWildCard);

      const bool ignoreTag =
        !_tagIgnoreList.isEmpty() &&
        (_tagIgnoreList.contains(kvp) || _tagIgnoreList.contains(keyWildCard));
      LOG_VART(ignoreTag);
      const bool allowListExcludesTag =
        !_tagsAllowList.isEmpty() && !_tagsAllowList.contains(kvp) &&
        !_tagsAllowList.contains(keyWildCard);
      LOG_VART(allowListExcludesTag);

      if (!ignoreTag && !allowListExcludesTag)
      {
        const QString ignoreRuleTag = _rulesIgnoreList.value(word, "");
        const QString customRuleTag = _customRulesList.value(word, "");
        if (ignoreRuleTag != kvp && customRuleTag != kvp)
        {
          const long count = lineParts[0].trimmed().toLong();
          LOG_VART(count);
          const QString line = QString::number(count) % "\t" % word % "\t" % kvp % "\n";
          LOG_VART(line);
          _filteredCountFile->write(line.toUtf8());
          linesWrittenCount++;
        }
        else
        {
          LOG_TRACE(
            "Skipping word/tag combo on the rule ignore list.  Word: " << word << ", tag: " <<
            kvp << ".");
          ignoredRuleCount++;
        }
      }
      else
      {
        if (ignoreTag)
        {
          LOG_TRACE("Skipping tag on the ignore list: " << kvp << ".");
        }
        else
        {
          LOG_TRACE("Skipping tag not on the include list: " << kvp << ".");
        }
        ignoredTagsCount++;
      }
    }
    else
    {
      if (wordTooSmall)
      {
        LOG_TRACE(
          "Skipping word: " << word <<
          ", the length of which is less than the minimum allowed word length of: " <<
          _minWordLength);
        wordsTooSmallCount++;
      }
      else
      {
        LOG_TRACE("Skipping word on the ignore list: " << word << ".");
        ignoredWordsCount++;
      }
    }

    linesParsedCount++;
    if (linesParsedCount % (_statusUpdateInterval * 100) == 0)
    {
      PROGRESS_INFO(
        "Filtered " << StringUtils::formatLargeNumber(linesParsedCount) <<
        " count file lines from input.");
    }
  }
  inputFile.close();

  LOG_INFO(
    "Skipped " << StringUtils::formatLargeNumber(wordsTooSmallCount) <<
    " words that were too small.");
  LOG_INFO("Ignored " << StringUtils::formatLargeNumber(ignoredWordsCount) << " words.");
  LOG_INFO("Ignored " << StringUtils::formatLargeNumber(ignoredTagsCount) << " tags.");
  LOG_INFO("Ignored " << StringUtils::formatLargeNumber(ignoredRuleCount) << " rules.");

  LOG_DEBUG("Writing custom rules...");
  long ruleCount = 0;
  for (QMap<QString, QString>::const_iterator customRulesItr = _customRulesList.begin();
       customRulesItr != _customRulesList.end(); ++customRulesItr)
  {
    const QString line =
      QString::number(999999) % "\t" % customRulesItr.key() % "\t" % customRulesItr.value() % "\n";
    LOG_VART(line);
    _filteredCountFile->write(line.toUtf8());
    ruleCount++;
  }
  LOG_INFO("Wrote " << ruleCount << " custom rules.");

  LOG_INFO("Wrote " << linesWrittenCount << " lines to filtered file.");

  _filteredCountFile->close();
}

}
