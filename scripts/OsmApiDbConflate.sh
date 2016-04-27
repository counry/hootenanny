#!/bin/bash
set -e

# This test:
#   - writes two datasets within the same AOI, one to an OSM API database and one to a Hoot API database 
#   - conflates a cropping from the same AOI of the two datasets together in hoot and writes the result to a Hoot API database
#   - writes out a sql changeset file that contains the difference between the original OSM API database dataset and the conflated output in the Hoot API database
#   - executes the changeset file SQL against the OSM API database
#   - reads out the entire contents of the OSM API database and verifies them

REF_DATASET=$1
SEC_DATASET=$2
AOI=$3
TEST_NAME=$4

echo "reference dataset: " $REF_DATASET
echo "secondary dataset: " $SEC_DATASET
echo "AOI: " $AOI
echo "TEST_NAME: " $TEST_NAME

source conf/DatabaseConfig.sh

rm -rf test-output/cmd/glacial/$TEST_NAME
mkdir -p test-output/cmd/glacial/$TEST_NAME

echo ""
echo "STEP 1: Cleaning out the osm api db and initializing it for use..."
source scripts/SetupOsmApiDB.sh force
export DB_URL="osmapidb://$DB_USER:$DB_PASSWORD@$DB_HOST:$DB_PORT/$DB_NAME_OSMAPI"
export AUTH="-h $DB_HOST -p $DB_PORT -U $DB_USER"
export PGPASSWORD=$DB_PASSWORD
psql --quiet $AUTH -d $DB_NAME_OSMAPI -f test-files/servicesdb/users.sql

export HOOT_DB_URL="hootapidb://$DB_USER:$DB_PASSWORD@$DB_HOST:$DB_PORT/$DB_NAME"
export HOOT_OPTS="-D hootapi.db.writer.create.user=true -D hootapi.db.writer.email=HootApiDbWriterTest@hoottestcpp.org -D hootapi.db.writer.overwrite.map=true -D hootapi.db.reader.email=HootApiDbWriterTest@hoottestcpp.org"

echo ""
echo "STEP 2: Writing the reference dataset to the osm api db..."
cp $REF_DATASET test-output/cmd/glacial/$TEST_NAME/2-ref-raw.osm
# By default, all of these element ID's start at 1.
hoot convert --error $HOOT_OPTS test-output/cmd/glacial/$TEST_NAME/2-ref-raw.osm test-output/cmd/glacial/$TEST_NAME/2-ref-ToBeAppliedToOsmApiDb.sql
psql --quiet $AUTH -d $DB_NAME_OSMAPI -f test-output/cmd/glacial/$TEST_NAME/2-ref-ToBeAppliedToOsmApiDb.sql

echo ""
echo "STEP 3: Reading the reference dataset out of the osm api db and writing it into a file (debugging purposes only)..."
hoot convert --error $HOOT_OPTS $DB_URL test-output/cmd/glacial/$TEST_NAME/3-ref-PulledFromOsmApiDb.osm

echo ""
echo "STEP 4: Querying out a cropped aoi for the reference dataset from the osm api db and writing it into a file (debugging purposes only)..."
hoot convert --error $HOOT_OPTS -D convert.bounding.box=$AOI $DB_URL test-output/cmd/glacial/$TEST_NAME/4-ref-cropped-PulledFromOsmApiDb.osm

echo ""
echo "STEP 5: Writing the secondary dataset to the hoot api db..."
cp $SEC_DATASET test-output/cmd/glacial/$TEST_NAME/5-secondary-raw.osm
# Ensure that data written to the Hoot API database doesn't have ID's that clash with the OSM API database by making the hoot
# api db writer use the ID sequencing scheme of the OSM API database.
hoot convert --error $HOOT_OPTS -D convert.ops=hoot::MapCropper -D crop.bounds=$AOI -D hootapi.db.writer.osmapidb.id.sequence.url=$DB_URL test-output/cmd/glacial/$TEST_NAME/5-secondary-raw.osm "$HOOT_DB_URL/5-secondary-$TEST_NAME"

echo ""
echo "STEP 6: Reading the secondary dataset out of the hoot api db and writing it into a file (debugging purposes only)..."
hoot convert --error $HOOT_OPTS "$HOOT_DB_URL/5-secondary-$TEST_NAME" test-output/cmd/glacial/$TEST_NAME/6-secondary-cropped-PulledFromHootApiDb.osm

echo ""
echo "STEP 7: Conflating the two datasets..."
hoot conflate --error $HOOT_OPTS -D convert.bounding.box=$AOI -D conflate.use.data.source.ids.input.1=true -D conflate.use.data.source.ids.input.2=true -D hootapi.db.writer.remap.ids=false -D hootapi.db.writer.osmapidb.id.sequence.url=$DB_URL $DB_URL "$HOOT_DB_URL/5-secondary-$TEST_NAME" "$HOOT_DB_URL/7-conflated-$TEST_NAME"

echo ""
echo "STEP 8: Reading the conflated dataset out of the hoot api db and writing it into a file (debugging purposes only)..."
hoot convert --error $HOOT_OPTS "$HOOT_DB_URL/7-conflated-$TEST_NAME" test-output/cmd/glacial/$TEST_NAME/8-conflated-PulledFromHootApiDb.osm

echo ""
echo "STEP 9: Writing a SQL changeset file that is the difference between the cropped reference input dataset and the conflated output..."
hoot derive-changeset --error $HOOT_OPTS -D changeset.user.id=1 -D osm.changeset.file.writer.generate.new.ids=false -D convert.bounding.box=$AOI $DB_URL "$HOOT_DB_URL/7-conflated-$TEST_NAME" test-output/cmd/glacial/$TEST_NAME/9-changeset-ToBeAppliedToOsmApiDb.osc.sql $DB_URL

echo ""
echo "STEP 10: Executing the changeset SQL on the osm api db..."
hoot apply-changeset --error $HOOT_OPTS test-output/cmd/glacial/$TEST_NAME/9-changeset-ToBeAppliedToOsmApiDb.osc.sql $DB_URL

echo ""
echo "STEP 11: Reading the contents of the osm api db for the specified aoi, writing it into a file, and verifying it (debugging purposes only)..."
hoot convert --error $HOOT_OPTS -D convert.bounding.box=$AOI $DB_URL test-output/cmd/glacial/$TEST_NAME/11-cropped-output-PulledFromOsmApiDb.osm

echo ""
echo "STEP 12: Reading the entire contents of the osm api db, writing it into a file, and verifying it..."
hoot convert --error $HOOT_OPTS $DB_URL test-output/cmd/glacial/$TEST_NAME/12-output-PulledFromOsmApiDb.osm
hoot is-match test-files/cmd/glacial/$TEST_NAME/output.osm test-output/cmd/glacial/$TEST_NAME/12-output-PulledFromOsmApiDb.osm
