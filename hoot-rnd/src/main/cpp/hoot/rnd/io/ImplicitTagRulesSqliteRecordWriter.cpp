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
#include "ImplicitTagRulesSqliteRecordWriter.h"

// hoot
#include <hoot/core/util/HootException.h>
#include <hoot/core/util/DbUtils.h>
#include <hoot/core/util/Log.h>
#include <hoot/core/util/Factory.h>
#include <hoot/core/util/StringUtils.h>
#include <hoot/core/util/ConfigOptions.h>

// Qt
#include <QSqlError>
#include <QFile>
#include <QStringBuilder>

namespace hoot
{

HOOT_FACTORY_REGISTER(ImplicitTagRuleWordPartWriter, ImplicitTagRulesSqliteRecordWriter)

ImplicitTagRulesSqliteRecordWriter::ImplicitTagRulesSqliteRecordWriter() :
_statusUpdateInterval(ConfigOptions().getTaskStatusUpdateInterval())
{
}

ImplicitTagRulesSqliteRecordWriter::~ImplicitTagRulesSqliteRecordWriter()
{
  close();
}

bool ImplicitTagRulesSqliteRecordWriter::isSupported(const QString outputUrl)
{
  return outputUrl.endsWith(".sqlite", Qt::CaseInsensitive);
}

void ImplicitTagRulesSqliteRecordWriter::open(const QString outputUrl)
{
  QFile outputFile(outputUrl);
  if (outputFile.exists() && !outputFile.remove())
  {
    throw HootException(QObject::tr("Error removing existing %1 for writing.").arg(outputUrl));
  }
  outputFile.open(QIODevice::WriteOnly);

  if (!QSqlDatabase::contains(outputUrl))
  {
    _db = QSqlDatabase::addDatabase("QSQLITE", outputUrl);
    _db.setDatabaseName(outputUrl);
    if (!_db.open())
    {
      throw HootException("Error opening DB. " + outputUrl);
    }
  }
  else
  {
    _db = QSqlDatabase::database(outputUrl);
  }

  if (!_db.isOpen())
  {
    throw HootException("Error DB is not open. " + outputUrl);
  }
  LOG_DEBUG("Opened: " << outputUrl << ".");

  _createTables();
  _prepareQueries();
}

void ImplicitTagRulesSqliteRecordWriter::_createTables()
{
  DbUtils::execNoPrepare(
    _db, "CREATE TABLE words (id INTEGER PRIMARY KEY, word TEXT NOT NULL UNIQUE)");
  DbUtils::execNoPrepare(
    _db, "CREATE TABLE tags (id INTEGER PRIMARY KEY, kvp TEXT NOT NULL UNIQUE)");
  DbUtils::execNoPrepare(
    _db,
    QString("CREATE TABLE rules (id INTEGER PRIMARY KEY, word_id INTEGER NOT NULL, ") +
    QString("tag_id INTEGER NOT NULL, tag_count INTEGER NOT NULL, FOREIGN KEY(word_id) ") +
    QString("REFERENCES words(id), FOREIGN KEY(tag_id) REFERENCES tags(id))"));
}

void ImplicitTagRulesSqliteRecordWriter::_prepareQueries()
{
  _insertWordQuery = QSqlQuery(_db);
  if (!_insertWordQuery.prepare("INSERT INTO words (word) VALUES(:word)"))
  {
    throw HootException(
      QString("Error preparing _insertWordQuery: %1").arg(_insertWordQuery.lastError().text()));
  }

  _insertTagQuery = QSqlQuery(_db);
  if (!_insertTagQuery.prepare("INSERT INTO tags (kvp) VALUES(:kvp)"))
  {
    throw HootException(
      QString("Error preparing _insertTagQuery: %1").arg(_insertTagQuery.lastError().text()));
  }

  _insertRuleQuery = QSqlQuery(_db);
  if (!_insertRuleQuery.prepare(
        QString("INSERT INTO rules (word_id, tag_id, tag_count) ") +
        QString("VALUES(:wordId, :tagId, :tagCount)")))
  {
    throw HootException(
      QString("Error preparing _insertRuleQuery: %1").arg(_insertRuleQuery.lastError().text()));
  }

  _getLastWordIdQuery = QSqlQuery(_db);
  if (!_getLastWordIdQuery.prepare("SELECT last_insert_rowid() FROM words"))
  {
    throw HootException(
      QString("Error preparing _getLastWordIdQuery: %1").arg(_getLastWordIdQuery.lastError().text()));
  }

  _getLastTagIdQuery = QSqlQuery(_db);
  if (!_getLastTagIdQuery.prepare("SELECT last_insert_rowid() FROM tags"))
  {
    throw HootException(
      QString("Error preparing _getLastTagIdQuery: %1").arg(_getLastTagIdQuery.lastError().text()));
  }
}

void ImplicitTagRulesSqliteRecordWriter::write(const QString inputUrl)
{
  LOG_INFO("Writing implicit tag rules to: " << _db.databaseName() << "...");

  long duplicatedWordCount = 0;

  DbUtils::execNoPrepare(_db, "BEGIN");

  //The input is assumed sorted by word, then by kvp.
  QFile inputFile(inputUrl);
  if (!inputFile.open(QIODevice::ReadOnly))
  {
    throw HootException(QObject::tr("Error opening %1 for reading.").arg(inputUrl));
  }

  //TODO: batch this write?
  long ruleWordPartCtr = 0;
  while (!inputFile.atEnd())
  {
    const QString line = QString::fromUtf8(inputFile.readLine().constData());
    const QStringList lineParts = line.split("\t");
    const long tagOccurrenceCount = lineParts[0].trimmed().toLong();
    LOG_VART(tagOccurrenceCount);
    const QString word = lineParts[1].trimmed();
    LOG_VART(word);
    const QString kvp = lineParts[2].trimmed();
    LOG_VART(kvp);

    long wordId = _wordsToWordIds.value(word, -1);
    if (wordId == -1)
    {
//      if (_words.contains(word))
//      {
//        LOG_ERROR("Found duplicate word: " << word);
//        duplicatedWordCount++;
//      }
//      else
//      {
        //_words.insert(word);
        wordId = _insertWord(word);
        if (wordId == -1)
        {
          LOG_WARN("Unable to insert word: " << word);
        }
        else
        {
          _wordsToWordIds[word] = wordId;
          LOG_TRACE("Created new word with ID: " << wordId);
        }
      //}
    }
    else
    {
      LOG_TRACE("Found existing word with ID: " << wordId);
    }

    long tagId = _tagsToTagIds.value(kvp, -1);
    if (tagId == -1)
    {
      tagId = _insertTag(kvp);
      _tagsToTagIds[kvp] = tagId;
      LOG_TRACE("Created new tag with ID: " << tagId);
    }
    else
    {
      LOG_TRACE("Found existing tag with ID: " << tagId);
    }

    if (wordId != -1)
    {
      _insertRuleWordPart(wordId, tagId, tagOccurrenceCount);
      ruleWordPartCtr++;
    }
    else
    {
      LOG_WARN(
        "Skipping inserting rule word part for word: " << word << " and tag: " << kvp << "...");
    }

    if (ruleWordPartCtr % _statusUpdateInterval == 0)
    {
      PROGRESS_INFO(
        "Sqlite implicit tag rules writer has parsed " <<
        StringUtils::formatLargeNumber(ruleWordPartCtr) << " input file lines.");
    }

    LOG_VART("************************************")
  }
  inputFile.close();

  LOG_INFO(
    "Wrote " << StringUtils::formatLargeNumber(_wordsToWordIds.size()) << " words, " <<
    StringUtils::formatLargeNumber(_tagsToTagIds.size()) << " tags, and " <<
    StringUtils::formatLargeNumber(ruleWordPartCtr) <<
    " word/tag associations to implicit tag rules database: " << _db.databaseName());
  if (duplicatedWordCount > 0)
  {
    LOG_WARN("Found " << duplicatedWordCount << " duplicated words.");
  }

  LOG_DEBUG("Clearing word/tags cache...");
  _wordsToWordIds.clear();
  _tagsToTagIds.clear();
  //_words.clear();

  LOG_INFO("Creating database indexes...");
  LOG_DEBUG("Creating tags index...");
  DbUtils::execNoPrepare(_db, "CREATE UNIQUE INDEX tag_idx ON tags (kvp)");
  //oddly, making the id indexes unique changes test results
  LOG_DEBUG("Creating word id index...");
  DbUtils::execNoPrepare(_db, "CREATE INDEX word_id_idx ON rules (word_id)");
  LOG_DEBUG("Creating tag id index...");
  DbUtils::execNoPrepare(_db, "CREATE INDEX tag_id_idx ON rules (tag_id)");
  LOG_DEBUG("Creating tag count index...");
  DbUtils::execNoPrepare(_db, "CREATE INDEX tag_count_idx ON rules (tag_count)");
  LOG_DEBUG("Creating words index...");
  DbUtils::execNoPrepare(_db, "CREATE UNIQUE INDEX word_idx ON words (word)");

  DbUtils::execNoPrepare(_db, "COMMIT");
}

long ImplicitTagRulesSqliteRecordWriter::_insertWord(const QString word)
{
  LOG_TRACE("Inserting word: " << word << "...");

  _insertWordQuery.bindValue(":word", word);
  if (!_insertWordQuery.exec())
  {
    QString err =
      QString("Error inserting word: %1; query: %2 (%3)")
        .arg(word)
        .arg(_insertWordQuery.executedQuery())
        .arg(_insertWordQuery.lastError().text());
    //Strange...getting a duplicate word here Абранка for waterway=river.  Passing
    //on the dupe for now until it apparent what's going on.
    //throw HootException(err);
    LOG_WARN(err);
    return -1;
  }

  if (!_getLastWordIdQuery.exec())
  {
    QString err =
      QString("Error executing query: %1 (%2)").arg(_getLastWordIdQuery.executedQuery()).
        arg(_getLastWordIdQuery.lastError().text());
    throw HootException(err);
  }

  bool ok = false;
  long id = -1;
  if (_getLastWordIdQuery.next())
  {
    id = _getLastWordIdQuery.value(0).toLongLong(&ok);
  }
  if (!ok || id == -1)
  {
    throw HootException(
      "Error retrieving new ID " + _getLastWordIdQuery.lastError().text() + " Query: " +
      _getLastWordIdQuery.executedQuery());
  }

  LOG_TRACE("Word: " << word << " inserted with ID: " << id);
  return id;
}

long ImplicitTagRulesSqliteRecordWriter::_insertTag(const QString kvp)
{
  LOG_TRACE("Inserting tag: " << kvp << "...");

  _insertTagQuery.bindValue(":kvp", kvp);
  if (!_insertTagQuery.exec())
  {
    QString err = QString("Error executing query: %1 (%2)").arg(_insertTagQuery.executedQuery()).
        arg(_insertTagQuery.lastError().text());
    throw HootException(err);
  }

  if (!_getLastTagIdQuery.exec())
  {
    QString err =
      QString("Error executing query: %1 (%2)").arg(_getLastTagIdQuery.executedQuery()).
        arg(_getLastTagIdQuery.lastError().text());
    throw HootException(err);
  }

  bool ok = false;
  long id = -1;
  if (_getLastTagIdQuery.next())
  {
    id = _getLastTagIdQuery.value(0).toLongLong(&ok);
  }
  if (!ok || id == -1)
  {
    throw HootException(
      "Error retrieving new ID " + _getLastTagIdQuery.lastError().text() + " Query: " +
      _getLastTagIdQuery.executedQuery());
  }

  LOG_TRACE("Tag: " << kvp << " inserted with ID: " << id);
  return id;
}

void ImplicitTagRulesSqliteRecordWriter::_insertRuleWordPart(const long wordId, const long tagId,
                                                             const long tagOccurrenceCount)
{
  LOG_TRACE(
    "Inserting rule record for: word ID: " << wordId << " tag ID: " <<
    tagId << " and tag count: " << tagOccurrenceCount << "...");

  _insertRuleQuery.bindValue(":wordId", qlonglong(wordId));
  _insertRuleQuery.bindValue(":tagId", qlonglong(tagId));
  _insertRuleQuery.bindValue(":tagCount", qlonglong(tagOccurrenceCount));
  if (!_insertRuleQuery.exec())
  {
    QString err = QString("Error executing query: %1 (%2)").arg(_insertRuleQuery.executedQuery()).
        arg(_insertRuleQuery.lastError().text());
    throw HootException(err);
  }
}

void ImplicitTagRulesSqliteRecordWriter::close()
{
  if (_db.open())
  {
    _db.close();
  }
}

}