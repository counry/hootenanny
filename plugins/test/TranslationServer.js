var assert = require('assert'),
    http = require('http'),
    xml2js = require('xml2js'),
    fs = require('fs'),
    httpMocks = require('node-mocks-http'),
    osmtogeojson = require('osmtogeojson'),
    DOMParser = new require('xmldom').DOMParser
    parser = new DOMParser();

var server = require('../TranslationServer.js');

describe('TranslationServer', function () {

    describe('fcodes', function() {

        it('should return fcodes for MGCP Line', function(){
            assert.equal(server.getFCodes({
                method: 'GET',
                translation: 'MGCP',
                geometry: 'line'
            }).length, 59);
        });

        it('should return fcodes for TDSv61 Point', function(){
            assert.equal(server.getFCodes({
                method: 'GET',
                translation: 'TDSv61',
                geometry: 'Point'
            }).length, 193);
        });

        it('should return fcodes for GGDMv30 Area', function(){
            assert.equal(server.getFCodes({
                method: 'GET',
                translation: 'GGDMv30',
                geometry: 'Area'
            }).length, 280);
        });

        it('should return fcodes for TDSv40 Vertex', function(){
            assert.equal(server.getFCodes({
                method: 'GET',
                translation: 'TDSv40',
                geometry: 'vertex'
            }).length, 190);
        });

    });

    describe('searchSchema', function() {

        var defaults = {};
        // Updated to reflect returning all geometries when none specified
        var defaultsResult = {
                                "name": "AERATION_BASIN_S",
                                "fcode": "AB040",
                                "desc": "Aeration Basin",
                                "geom": "Area"
                            };

        var MgcpPointBui = {
                                translation: 'MGCP',
                                geomType: 'point',
                                searchStr: 'Bui'
                            };
        var MgcpResult = [{
                                name: "PAL015",
                                fcode: "AL015",
                                desc: "General Building",
                                geom: "Point",
                                idx: -1
                            },
                            {
                                name: "PAL020",
                                fcode: "AL020",
                                desc: "Built-Up Area",
                                geom: "Point",
                                idx: -1
                            }];

        var MgcpVertexCulvert = {
                                translation: 'MGCP',
                                geomType: 'vertex',
                                searchStr: 'culvert'
                            };
        var MgcpVertexResult = [{
                                name: "PAQ065",
                                fcode: "AQ065",
                                desc: "Culvert",
                                geom: "Point",
                                idx: -1
                            }];
        it('should search for default options', function(){
            assert.equal(JSON.stringify(server.searchSchema(defaults)[0]), JSON.stringify(defaultsResult));
        });

        it('should search for "Bui" point feature types in the MGCP schema', function(){
            assert.equal(JSON.stringify(server.searchSchema(MgcpPointBui).slice(0,2)), JSON.stringify(MgcpResult));
        });

        it('should search for "culvert" vertex feature types in the MGCP schema', function(){
            assert.equal(JSON.stringify(server.searchSchema(MgcpVertexCulvert).slice(0,1)), JSON.stringify(MgcpVertexResult));
        });
    });

    describe('handleInputs', function() {

        it('should handle translateTo GET', function() {
            // example url
            // http://localhost:8094/translateTo?idval=AL015&geom=Point&translation=MGCP&idelem=fcode
            var schema = server.handleInputs({
                idval: 'AL015',
                geom: 'Point',
                translation: 'MGCP',
                idelem: 'fcode',
                method: 'GET',
                path: '/translateTo'
            });
            assert.equal(schema.desc, 'General Building');
            assert.equal(schema.columns[0].name, 'ACC');
            assert.equal(schema.columns[0].enumerations[0].name, 'Accurate');
            assert.equal(schema.columns[0].enumerations[0].value, '1');
        });

        it('should handle no matches osmtotds GET for MGCP', function() {
            assert.throws(function error() {
                server.handleInputs({
                    idval: 'FB123',
                    geom: 'Area',
                    translation: 'TDSv61',
                    idelem: 'fcode',
                    method: 'GET',
                    path: '/osmtotds'
                })
            }, Error, 'TDSv61 for Area with fcode=FB123 not found');
        });

        it('should handle translateFrom GET for TDSv61', function() {
            //http://localhost:8094/translateFrom?fcode=AL013&translation=TDSv61
            var attrs = server.handleInputs({
                fcode: 'AL013',
                translation: 'TDSv61',
                method: 'GET',
                path: '/translateFrom'
            }).attrs;
            assert.equal(attrs.building, 'yes');
        });

        it('should handle tdstoosm GET for TDSv40', function() {
            //http://localhost:8094/tdstoosm?fcode=AL013&translation=TDSv61
            var attrs = server.handleInputs({
                fcode: 'AP030',
                translation: 'TDSv40',
                method: 'GET',
                path: '/tdstoosm'
            }).attrs;
            assert.equal(attrs.highway, 'road');
        });

        it('should handle translateFrom GET for MGCP', function() {
            //http://localhost:8094/translateFrom?fcode=AL013&translation=TDSv61
            var attrs = server.handleInputs({
                fcode: 'BH140',
                translation: 'MGCP',
                method: 'GET',
                path: '/translateFrom'
            }).attrs;
            assert.equal(attrs.waterway, 'river');
        });

        it('should handle tdstoosm GET for MGCP', function() {
            //http://localhost:8094/tdstoosm?fcode=AL013&translation=TDSv61
            var attrs = server.handleInputs({
                fcode: 'BH140',
                translation: 'GGDMv30',
                method: 'GET',
                path: '/tdstoosm'
            }).attrs;
            assert.equal(attrs.waterway, 'river');
        });
        it('should handle invalid F_CODE in tdstoosm GET for MGCP', function() {
            var attrs = server.handleInputs({
                fcode: 'ZZTOP',
                translation: 'MGCP',
                method: 'GET',
                path: '/tdstoosm'
            }).attrs;
            assert.equal(attrs.error, 'Feature Code ZZTOP is not valid for MGCP');
        });

        it('should handle translateTo TDSv61 POST', function() {
            var output = server.handleInputs({
                osm: '<osm version="0.6" upload="true" generator="JOSM"><node id="-1" lon="-105.21811763904256" lat="39.35643172777992" version="0"><tag k="building" v="yes"/><tag k="uuid" v="{bfd3f222-8e04-4ddc-b201-476099761302}"/></node></osm>',
                method: 'POST',
                translation: 'TDSv61',
                path: '/translateTo'
            });

            var xml = parser.parseFromString(output);
            var gj = osmtogeojson(xml);

            assert.equal(xml.getElementsByTagName("osm")[0].getAttribute("schema"), "TDSv61");

            var tags = gj.features[0].properties;
            assert.equal(tags["F_CODE"], "AL013");
            assert.equal(tags["UFI"], "bfd3f222-8e04-4ddc-b201-476099761302");
        });

        it('should handle osmtotds POST and preserve bounds tag and ids', function() {
            //http://localhost:8094/osmtotds
            var osm2trans = server.handleInputs({
                osm: '<osm version="0.6" upload="true" generator="JOSM"><bounds minlat="39.35643172777992" minlon="-105.21811763904256" maxlat="39.35643172777992" maxlon="-105.21811763904256" origin="MapEdit server" /><node id="777" lon="-105.21811763904256" lat="39.35643172777992" version="0"><tag k="building" v="yes"/><tag k="uuid" v="{bfd3f222-8e04-4ddc-b201-476099761302}"/></node></osm>',
                method: 'POST',
                translation: 'TDSv61',
                path: '/osmtotds'
            });
            xml2js.parseString(osm2trans, function(err, result) {
                if (err) console.error(err);
                assert.equal(result.osm.node[0].$.id, "777");
                assert.equal(parseFloat(result.osm.bounds[0].$.minlat).toFixed(6), 39.356432);
                assert.equal(parseFloat(result.osm.bounds[0].$.minlon).toFixed(6), -105.218118);
                assert.equal(parseFloat(result.osm.bounds[0].$.maxlat).toFixed(6), 39.356432);
                assert.equal(parseFloat(result.osm.bounds[0].$.maxlon).toFixed(6), -105.218118);
            });
        });
        it('should handle OSM to MGCP POST of building area feature', function() {
            var osm2trans = server.handleInputs({
                osm: '<osm version="0.6" upload="true" generator="hootenanny"><way id="-1" version="0"><nd ref="-1"/><nd ref="-4"/><nd ref="-7"/><nd ref="-10"/><nd ref="-1"/><tag k="building" v="yes"/><tag k="uuid" v="{d7cdbdfe-88c6-4d8a-979d-ad88cfc65ef1}"/></way></osm>',
                method: 'POST',
                translation: 'MGCP',
                path: '/osmtotds'
            });
            xml2js.parseString(osm2trans, function(err, result) {
                if (err) console.error(err);
                assert.equal(result.osm.$.schema, "MGCP");
                assert.equal(result.osm.way[0].tag[0].$.k, "Feature Code");
                assert.equal(result.osm.way[0].tag[0].$.v, "AL015:General Building");
                assert.equal(result.osm.way[0].tag[1].$.k, "MGCP Feature universally unique identifier");
                assert.equal(result.osm.way[0].tag[1].$.v, "d7cdbdfe-88c6-4d8a-979d-ad88cfc65ef1");
            });
        });

        it('should handle OSM to MGCP POST of power line feature', function() {
            var osm2trans = server.handleInputs({
                osm: '<osm version="0.6" upload="true" generator="hootenanny"><way id="-1" version="0"><nd ref="-1"/><nd ref="-4"/><nd ref="-7"/><nd ref="-10"/><nd ref="-1"/><tag k="power" v="line"/><tag k="uuid" v="{d7cdbdfe-88c6-4d8a-979d-ad88cfc65ef1}"/></way></osm>',
                method: 'POST',
                translation: 'MGCP',
                path: '/translateTo'
            });
            xml2js.parseString(osm2trans, function(err, result) {
                if (err) console.error(err);
                assert.equal(result.osm.$.schema, "MGCP");
                assert.equal(result.osm.way[0].tag[0].$.k, "UID");
                assert.equal(result.osm.way[0].tag[0].$.v, "d7cdbdfe-88c6-4d8a-979d-ad88cfc65ef1");
                assert.equal(result.osm.way[0].tag[1].$.k, "FCODE");
                assert.equal(result.osm.way[0].tag[1].$.v, "AT030");
            });
        });

        it('should handle OSM to TDSv40 POST of power line feature', function() {
            var osm2trans = server.handleInputs({
                osm: '<osm version="0.6" upload="true" generator="hootenanny"><way id="-1" version="0"><nd ref="-1"/><nd ref="-4"/><nd ref="-7"/><nd ref="-10"/><nd ref="-1"/><tag k="power" v="line"/><tag k="uuid" v="{d7cdbdfe-88c6-4d8a-979d-ad88cfc65ef1}"/></way></osm>',
                method: 'POST',
                translation: 'TDSv40',
                path: '/translateTo'
            });
            xml2js.parseString(osm2trans, function(err, result) {
                if (err) console.error(err);
                assert.equal(result.osm.$.schema, "TDSv40");
                assert.equal(result.osm.way[0].tag[0].$.k, "CAB");
                assert.equal(result.osm.way[0].tag[0].$.v, "2");
                assert.equal(result.osm.way[0].tag[1].$.k, "UFI");
                assert.equal(result.osm.way[0].tag[1].$.v, "d7cdbdfe-88c6-4d8a-979d-ad88cfc65ef1");
                assert.equal(result.osm.way[0].tag[2].$.k, "F_CODE");
                assert.equal(result.osm.way[0].tag[2].$.v, "AT005");
            });
        });

        it('should handle OSM to TDSv61 POST of power line feature', function() {
            var osm2trans = server.handleInputs({
                osm: '<osm version="0.6" upload="true" generator="hootenanny"><way id="-1" version="0"><nd ref="-1"/><nd ref="-4"/><nd ref="-7"/><nd ref="-10"/><nd ref="-1"/><tag k="power" v="line"/><tag k="uuid" v="{d7cdbdfe-88c6-4d8a-979d-ad88cfc65ef1}"/></way></osm>',
                method: 'POST',
                translation: 'TDSv61',
                path: '/translateTo'
            });
            xml2js.parseString(osm2trans, function(err, result) {
                if (err) console.error(err);
                assert.equal(result.osm.$.schema, "TDSv61");
                assert.equal(result.osm.way[0].tag[0].$.k, "CAB");
                assert.equal(result.osm.way[0].tag[0].$.v, "2");
                assert.equal(result.osm.way[0].tag[1].$.k, "UFI");
                assert.equal(result.osm.way[0].tag[1].$.v, "d7cdbdfe-88c6-4d8a-979d-ad88cfc65ef1");
                assert.equal(result.osm.way[0].tag[2].$.k, "F_CODE");
                assert.equal(result.osm.way[0].tag[2].$.v, "AT005");
            });
        });

        it('should handle OSM to GGDMv30 POST of power line feature', function() {
            var osm2trans = server.handleInputs({
                osm: '<osm version="0.6" upload="true" generator="hootenanny"><way id="-1" version="0"><nd ref="-1"/><nd ref="-4"/><nd ref="-7"/><nd ref="-10"/><nd ref="-1"/><tag k="power" v="line"/><tag k="uuid" v="{d7cdbdfe-88c6-4d8a-979d-ad88cfc65ef1}"/></way></osm>',
                method: 'POST',
                translation: 'GGDMv30',
                path: '/translateTo'
            });
            xml2js.parseString(osm2trans, function(err, result) {
                if (err) console.error(err);
                assert.equal(result.osm.$.schema, "GGDMv30");
                assert.equal(result.osm.way[0].tag[0].$.k, "CAB");
                assert.equal(result.osm.way[0].tag[0].$.v, "2");
                assert.equal(result.osm.way[0].tag[1].$.k, "UFI");
                assert.equal(result.osm.way[0].tag[1].$.v, "d7cdbdfe-88c6-4d8a-979d-ad88cfc65ef1");
                assert.equal(result.osm.way[0].tag[2].$.k, "F_CODE");
                assert.equal(result.osm.way[0].tag[2].$.v, "AT005");
            });
        });

        it('should handle OSM to MGCP POST of road line feature with width', function() {
            //http://localhost:8094/osmtotds
            var osm2trans = server.handleInputs({
                osm: '<osm version="0.6" upload="true" generator="hootenanny"><way id="-8" version="0"><nd ref="-21"/><nd ref="-24"/><nd ref="-27"/><tag k="highway" v="road"/><tag k="uuid" v="{8cd72087-a7a2-43a9-8dfb-7836f2ffea13}"/><tag k="width" v="20"/><tag k="lanes" v="2"/></way></osm>',
                method: 'POST',
                translation: 'MGCP',
                path: '/osmtotds'
            });
            xml2js.parseString(osm2trans, function(err, result) {
                if (err) console.error(err);
                assert.equal(result.osm.$.schema, "MGCP");
                assert.equal(result.osm.way[0].tag[0].$.k, "Feature Code");
                assert.equal(result.osm.way[0].tag[0].$.v, "AP030:Road");
                assert.equal(result.osm.way[0].tag[1].$.k, "MGCP Feature universally unique identifier");
                assert.equal(result.osm.way[0].tag[1].$.v, "8cd72087-a7a2-43a9-8dfb-7836f2ffea13");
                assert.equal(result.osm.way[0].tag[2].$.k, "Thoroughfare Class");
                assert.equal(result.osm.way[0].tag[2].$.v, "Unknown");
                assert.equal(result.osm.way[0].tag[3].$.k, "Route Minimum Travelled Way Width");
                assert.equal(result.osm.way[0].tag[3].$.v, "20");
                assert.equal(result.osm.way[0].tag[4].$.k, "Track or Lane Count");
                assert.equal(result.osm.way[0].tag[4].$.v, "2");
            });
        });

        it('should handle OSM to GGDMv30 English POST of road line feature with width', function() {
            //http://localhost:8094/osmtotds
            var osm2trans = server.handleInputs({
                osm: '<osm version="0.6" upload="true" generator="hootenanny"><way id="-8" version="0"><nd ref="-21"/><nd ref="-24"/><nd ref="-27"/><tag k="highway" v="road"/><tag k="uuid" v="{8cd72087-a7a2-43a9-8dfb-7836f2ffea13}"/><tag k="width" v="20"/><tag k="lanes" v="2"/></way></osm>',
                method: 'POST',
                translation: 'GGDMv30',
                path: '/osmtotds'
            });
            xml2js.parseString(osm2trans, function(err, result) {
                if (err) console.error(err);
                assert.equal(result.osm.$.schema, "GGDMv30");

                assert.equal(result.osm.way[0].tag[0].$.k, "Feature Code");
                assert.equal(result.osm.way[0].tag[0].$.v, "AP030:Road");
                assert.equal(result.osm.way[0].tag[1].$.k, "Roadway Type");
                assert.equal(result.osm.way[0].tag[1].$.v, "No Information");
                assert.equal(result.osm.way[0].tag[2].$.k, "Unique Entity Identifier");
                assert.equal(result.osm.way[0].tag[2].$.v, "8cd72087-a7a2-43a9-8dfb-7836f2ffea13");
                assert.equal(result.osm.way[0].tag[3].$.k, "Route Pavement Information : Route Minimum Travelled Way Width");
                assert.equal(result.osm.way[0].tag[3].$.v, "20");
                assert.equal(result.osm.way[0].tag[4].$.k, "Route Identification <route designation type>");
                assert.equal(result.osm.way[0].tag[4].$.v, "Local");
                assert.equal(result.osm.way[0].tag[5].$.k, "Track or Lane Count");
                assert.equal(result.osm.way[0].tag[5].$.v, "2");
            });
        });

        it('should handle OSM to GGDMv30 Raw POST of road line feature with width', function() {
            //http://localhost:8094/osmtotds
            var osm2trans = server.handleInputs({
                osm: '<osm version="0.6" upload="true" generator="hootenanny"><way id="-8" version="0"><nd ref="-21"/><nd ref="-24"/><nd ref="-27"/><tag k="highway" v="road"/><tag k="uuid" v="{8cd72087-a7a2-43a9-8dfb-7836f2ffea13}"/><tag k="width" v="20"/><tag k="lanes" v="2"/></way></osm>',
                method: 'POST',
                translation: 'GGDMv30',
                path: '/translateTo'
            });
            xml2js.parseString(osm2trans, function(err, result) {
                if (err) console.error(err);
                assert.equal(result.osm.$.schema, "GGDMv30");
                assert.equal(result.osm.way[0].tag[0].$.k, "LTN");
                assert.equal(result.osm.way[0].tag[0].$.v, "2");
                assert.equal(result.osm.way[0].tag[1].$.k, "RTY");
                assert.equal(result.osm.way[0].tag[1].$.v, "-999999");
                assert.equal(result.osm.way[0].tag[2].$.k, "UFI");
                assert.equal(result.osm.way[0].tag[2].$.v, "8cd72087-a7a2-43a9-8dfb-7836f2ffea13");
                assert.equal(result.osm.way[0].tag[3].$.k, "F_CODE");
                assert.equal(result.osm.way[0].tag[3].$.v, "AP030");
                assert.equal(result.osm.way[0].tag[4].$.k, "ZI016_WD1");
                assert.equal(result.osm.way[0].tag[4].$.v, "20");
                assert.equal(result.osm.way[0].tag[5].$.k, "RIN_ROI");
                assert.equal(result.osm.way[0].tag[5].$.v, "5");
            });
        });

        it('should handle OSM to TDSv40 POST of road line feature with width', function() {
            //http://localhost:8094/osmtotds
            var osm2trans = server.handleInputs({
                osm: '<osm version="0.6" upload="true" generator="hootenanny"><way id="-8" version="0"><nd ref="-21"/><nd ref="-24"/><nd ref="-27"/><tag k="highway" v="road"/><tag k="uuid" v="{8cd72087-a7a2-43a9-8dfb-7836f2ffea13}"/><tag k="width" v="20"/><tag k="lanes" v="2"/></way></osm>',
                method: 'POST',
                translation: 'TDSv40',
                path: '/osmtotds'
            });
            //console.log(osm2trans);
            xml2js.parseString(osm2trans, function(err, result) {
                if (err) console.error(err);
                assert.equal(result.osm.$.schema, "TDSv40");

                assert.equal(result.osm.way[0].tag[0].$.k, "Feature Code");
                assert.equal(result.osm.way[0].tag[0].$.v, "AP030:Road");
                assert.equal(result.osm.way[0].tag[1].$.k, "Width");
                assert.equal(result.osm.way[0].tag[1].$.v, "20");
                assert.equal(result.osm.way[0].tag[2].$.k, "Route Designation (route designation type)");
                assert.equal(result.osm.way[0].tag[2].$.v, "No Information");
                assert.equal(result.osm.way[0].tag[3].$.k, "Unique Entity Identifier");
                assert.equal(result.osm.way[0].tag[3].$.v, "8cd72087-a7a2-43a9-8dfb-7836f2ffea13");
                assert.equal(result.osm.way[0].tag[4].$.k, "Thoroughfare Type");
                assert.equal(result.osm.way[0].tag[4].$.v, "No Information");
                assert.equal(result.osm.way[0].tag[5].$.k, "Track or Lane Count");
                assert.equal(result.osm.way[0].tag[5].$.v, "2");
            });
        });

        it('should handle OSM to MGCP POST of facility area feature', function() {
            var osm2trans = server.handleInputs({
                osm: '<osm version="0.6" upload="true" generator="hootenanny"><way id="-1" version="0"><nd ref="-1"/><nd ref="-4"/><nd ref="-7"/><nd ref="-10"/><nd ref="-1"/><tag k="facility" v="yes"/><tag k="uuid" v="{fee4529b-5ecc-4e5c-b06d-1b26a8e830e6}"/><tag k="area" v="yes"/></way></osm>',
                method: 'POST',
                translation: 'MGCP',
                path: '/translateTo'
            });
            xml2js.parseString(osm2trans, function(err, result) {
                if (err) console.error(err);
                assert.equal(result.osm.way[0].tag[0].$.k, "UID");
                assert.equal(result.osm.way[0].tag[0].$.v, "fee4529b-5ecc-4e5c-b06d-1b26a8e830e6");
                assert.equal(result.osm.way[0].tag[1].$.k, "FCODE");
                assert.equal(result.osm.way[0].tag[1].$.v, "AL010");
            });
        });

        it('should handle OSM to TDSv61 Raw POST of a complete osm file and preserve bounds tag and element ids', function() {
            var data = fs.readFileSync('../test-files/ToyTestA.osm', 'utf8');//, function(err, data) {
            var osm2trans = server.handleInputs({
                osm: data,
                method: 'POST',
                translation: 'TDSv61',
                path: '/translateTo'
            });
            xml2js.parseString(osm2trans, function(err, result) {
                if (err) console.error(err);
                assert.equal(result.osm.$.schema, "TDSv61");

                assert.equal(result.osm.bounds[0].$.minlat, "38.85324242720166");
                assert.equal(result.osm.bounds[0].$.minlon, "-104.9024316099691");
                assert.equal(result.osm.bounds[0].$.maxlat, "38.85496143739888");
                assert.equal(result.osm.bounds[0].$.maxlon, "-104.8961823052624");

                assert.equal(result.osm.way[0].$.id, "-1669801");
                assert.equal(result.osm.way[0].nd[0].$.ref, "-1669731");
                assert.equal(result.osm.way[0].nd[1].$.ref, "-1669791");
                assert.equal(result.osm.way[0].nd[2].$.ref, "-1669793");
            });
        });

        it('should be lossy to go from osm -> mgcp -> osm', function() {

            var data = '<osm version="0.6" upload="true" generator="JOSM"><node id="-4" lon="-105.24014094121263" lat="39.28928610944744" version="0"><tag k="poi" v="yes"/><tag k="amenity" v="cafe"/><tag k="uuid" v="{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}"/></node></osm>';

            var mgcp_xml = server.handleInputs({
                osm: data,
                method: 'POST',
                translation: 'MGCP',
                path: '/translateTo'
            });

            var xml = parser.parseFromString(mgcp_xml);
            var gj = osmtogeojson(xml);

            assert.equal(xml.getElementsByTagName("osm")[0].getAttribute("schema"), "MGCP");

            var tags = gj.features[0].properties;
            assert.equal(tags["FCODE"], "AL015");
            assert.equal(tags["FFN"], "572");
            assert.equal(tags["HWT"], "998");
            assert.equal(tags["UID"], "4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4");

            var osm_xml = server.handleInputs({
                osm: mgcp_xml,
                method: 'POST',
                translation: 'MGCP',
                path: '/translateFrom'
            });

            xml = parser.parseFromString(osm_xml);
            gj = osmtogeojson(xml);

            assert.equal(xml.getElementsByTagName("osm")[0].getAttribute("schema"), "OSM");

            var tags = gj.features[0].properties;
            assert.equal(tags["building"], "yes");
            assert.equal(tags["amenity"], "restaurant");
            assert.equal(tags["uuid"], "{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}");
        });

        it('should translate from osm -> mgcp', function() {

            var data = '<osm version="0.6" upload="true" generator="JOSM"><node id="-4" lon="-105.24014094121263" lat="39.28928610944744" version="0"><tag k="poi" v="yes"/><tag k="place" v="town"/><tag k="name" v="Manitou Springs"/><tag k="uuid" v="{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}"/></node></osm>';

            var mgcp_xml = server.handleInputs({
                osm: data,
                method: 'POST',
                translation: 'MGCP',
                path: '/translateTo'
            });

            var xml = parser.parseFromString(mgcp_xml);
            var gj = osmtogeojson(xml);

            assert.equal(xml.getElementsByTagName("osm")[0].getAttribute("schema"), "MGCP");

            var tags = gj.features[0].properties;
            assert.equal(tags["FCODE"], "AL020");
            assert.equal(tags["NAM"], "Manitou Springs");
            assert.equal(tags["FUC"], "999");
            assert.equal(tags["UID"], "4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4");
        });

        it('should translate OTH from tdsv61 -> osm -> tdsv61', function() {

            var data = '<osm version="0.6" upload="true" generator="hootenanny"><node id="-19" lon="9.304397440128325" lat="41.65083522130027" version="0"><tag k="FCSUBTYPE" v="100080"/><tag k="ZI001_SDP" v="DigitalGlobe"/><tag k="UFI" v="0d8b2563-81cf-44d4-8ef7-52c0e862651f"/><tag k="F_CODE" v="AL010"/><tag k="ZI006_MEM" v="&lt;OSM&gt;{&quot;source:imagery:datetime&quot;:&quot;2017-11-11 10:45:15&quot;,&quot;source:imagery:sensor&quot;:&quot;WV02&quot;,&quot;source:imagery:id&quot;:&quot;756b80e1f695fb591caca8e7ce0f9ef5&quot;}&lt;/OSM&gt;"/><tag k="ZSAX_RS0" v="U"/><tag k="OTH" v="(FFN:foo)"/></node></osm>';

            var osm_xml = server.handleInputs({
                osm: data,
                method: 'POST',
                translation: 'TDSv61',
                path: '/translateFrom'
            });
            //console.log(osm_xml);

            var xml = parser.parseFromString(osm_xml);
            var gj = osmtogeojson(xml);

            assert.equal(xml.getElementsByTagName("osm")[0].getAttribute("schema"), "OSM");

            var tags = gj.features[0].properties;
            assert.equal(tags["facility"], "yes");
            assert.equal(tags["note:oth"], "(FFN:foo)");
            assert.equal(tags["security:classification"], "UNCLASSIFIED");
            assert.equal(tags["uuid"], "{0d8b2563-81cf-44d4-8ef7-52c0e862651f}");
            assert.equal(tags["source"], "DigitalGlobe");
            assert.equal(tags["source:imagery:id"], "756b80e1f695fb591caca8e7ce0f9ef5");
            assert.equal(tags["source:imagery:datetime"], "2017-11-11 10:45:15");
            assert.equal(tags["source:imagery:sensor"], "WV02");

            var tds_xml = server.handleInputs({
                osm: osm_xml,
                method: 'POST',
                translation: 'TDSv61',
                path: '/translateTo'
            });
            //console.log(tds_xml);
            xml = parser.parseFromString(tds_xml);
            gj = osmtogeojson(xml);

            assert.equal(xml.getElementsByTagName("osm")[0].getAttribute("schema"), "TDSv61");

            var tags = gj.features[0].properties;
            assert.equal(tags["F_CODE"], "AL010");
            assert.equal(tags["ZI001_SDP"], "DigitalGlobe");
            assert.equal(tags["ZSAX_RS0"], "U");
            assert.equal(tags["OTH"], "(FFN:foo)");
            assert.equal(tags["UFI"], "0d8b2563-81cf-44d4-8ef7-52c0e862651f");

        });


        it('should handle tdstoosm POST of power line feature', function() {
            var trans2osm = server.handleInputs({
                osm: '<osm version="0.6" upload="true" generator="hootenanny"><way id="-6" version="0"><nd ref="-13"/><nd ref="-14"/><nd ref="-15"/><nd ref="-16"/><tag k="UID" v="fee4529b-5ecc-4e5c-b06d-1b26a8e830e6"/><tag k="FCODE" v="AT030"/></way></osm>',
                method: 'POST',
                translation: 'MGCP',
                path: '/translateFrom'
            });
            var output = xml2js.parseString(trans2osm, function(err, result) {
                if (err) console.error(err);
                assert.equal(result.osm.way[0].tag[0].$.k, "uuid");
                assert.equal(result.osm.way[0].tag[0].$.v, "{fee4529b-5ecc-4e5c-b06d-1b26a8e830e6}");
                assert.equal(result.osm.way[0].tag[1].$.k, "power");
                assert.equal(result.osm.way[0].tag[1].$.v, "line");
            });
        });

        it('should handle tdstoosm POST of power line feature', function() {
            var trans2osm = server.handleInputs({
                osm: '<osm version="0.6" upload="true" generator="hootenanny"><way id="-6" version="0"><nd ref="-13"/><nd ref="-14"/><nd ref="-15"/><nd ref="-16"/><tag k="UFI" v="fee4529b-5ecc-4e5c-b06d-1b26a8e830e6"/><tag k="F_CODE" v="AT005"/><tag k="CAB" v="2"/></way></osm>',
                method: 'POST',
                translation: 'TDSv61',
                path: '/translateFrom'
            });
            var output = xml2js.parseString(trans2osm, function(err, result) {
                if (err) console.error(err);
                assert.equal(result.osm.way[0].tag[0].$.k, "uuid");
                assert.equal(result.osm.way[0].tag[0].$.v, "{fee4529b-5ecc-4e5c-b06d-1b26a8e830e6}");
                assert.equal(result.osm.way[0].tag[1].$.k, "power");
                assert.equal(result.osm.way[0].tag[1].$.v, "line");
            });
        });

        it('should handle tdstoosm POST of facility area feature', function() {
            var trans2osm = server.handleInputs({
                osm: '<osm version="0.6" upload="true" generator="hootenanny"><way id="-6" version="0"><nd ref="-13"/><nd ref="-14"/><nd ref="-15"/><nd ref="-16"/><nd ref="-13"/><tag k="UID" v="fee4529b-5ecc-4e5c-b06d-1b26a8e830e6"/><tag k="FCODE" v="AL010"/><tag k="SDP" v="D"/></way></osm>',
                method: 'POST',
                translation: 'MGCP',
                path: '/translateFrom'
            });
            var output = xml2js.parseString(trans2osm, function(err, result) {
                if (err) console.error(err);
                assert.equal(result.osm.way[0].tag[0].$.k, "source");
                assert.equal(result.osm.way[0].tag[0].$.v, "D");
                assert.equal(result.osm.way[0].tag[1].$.k, "uuid");
                assert.equal(result.osm.way[0].tag[1].$.v, "{fee4529b-5ecc-4e5c-b06d-1b26a8e830e6}");
                assert.equal(result.osm.way[0].tag[2].$.k, "facility");
                assert.equal(result.osm.way[0].tag[2].$.v, "yes");
                assert.equal(result.osm.way[0].tag[3].$.k, "area");
                assert.equal(result.osm.way[0].tag[3].$.v, "yes");
            });
        });

        it('should return error message for invalid F_CODE/geom combination in osmtotds POST', function() {
            var output = server.handleInputs({
                osm: '<osm version="0.6" upload="true" generator="hootenanny"><node id="-24" lon="9.305143094234467" lat="41.65140640721789" version="0"><tag k="leisure" v="park"/><tag k="source" v="DigitalGlobe"/><tag k="source:imagery:sensor" v="WV02"/><tag k="source:imagery:id" v="756b80e1f695fb591caca8e7ce0f9ef5"/><tag k="source:imagery:datetime" v="2017-11-11 10:45:15"/><tag k="security:classification" v="UNCLASSIFIED"/></node></osm>',
                //osm: '<osm version="0.6" upload="true" generator="hootenanny"><node id="72" lon="-104.878690508945" lat="38.8618557942463" version="1"><tag k="poi" v="yes"/><tag k="hoot:status" v="1"/><tag k="name" v="Garden of the Gods"/><tag k="leisure" v="park"/><tag k="error:circular" v="1000"/><tag k="hoot" v="AllDataTypesACucumber"/></node></osm>',
                method: 'POST',
                translation: 'MGCP',
                path: '/translateTo'
            });
            //console.log(output);
            var xml = parser.parseFromString(output);
            var gj = osmtogeojson(xml);

            assert.equal(xml.getElementsByTagName("osm")[0].getAttribute("schema"), "MGCP");

            var tags = gj.features[0].properties;
            assert.equal(tags["error"], 'Point geometry is not valid for AK120 in MGCP TRD4');
        });

        it('should handle bad translation schema value in osmtotds POST', function() {
            assert.throws(function error() {
                var osm2trans = server.handleInputs({
                    osm: '<osm version="0.6" upload="true" generator="JOSM"><node id="-1" lon="-105.21811763904256" lat="39.35643172777992" version="0"><tag k="building" v="yes"/><tag k="uuid" v="{bfd3f222-8e04-4ddc-b201-476099761302}"/></node></osm>',
                    method: 'POST',
                    translation: 'TDv61',
                    path: '/osmtotds'
                });
            }, Error, 'Unsupported translation schema');
        });

        it('should handle tdstoosm POST', function() {
            //http://localhost:8094/tdstoosm
            var trans2osm = server.handleInputs({
                osm: '<osm version="0.6" upload="true" generator="JOSM"><node id="-9" lon="-104.907037158172" lat="38.8571566428667" version="0"><tag k="Horizontal Accuracy Category" v="Accurate"/><tag k="Built-up Area Density Category" ve="Unknown"/><tag k="Commercial Copyright Notice" v="UNK"/><tag k="Feature Code" v="AL020:Built-Up Area"/><tag k="Functional Use" v="Other"/><tag k="Condition of Facility" v="Unknown"/><tag k="Name" v="Manitou Springs"/><tag k="Named Feature Identifier" v="UNK"/><tag k="Name Identifier" v="UNK"/><tag k="Relative Importance" v="Unknown"/><tag k="Source Description" v="N_A"/><tag k="Source Date and Time" v="UNK"/><tag k="Source Type" v="Unknown"/><tag k="Associated Text" v="&lt;OSM&gt;{&quot;poi&quot;:&quot;yes&quot;}&lt;/OSM&gt;"/><tag k="MGCP Feature universally unique identifier" v="c6df0618-ce96-483c-8d6a-afa33541646c"/></node></osm>',
                method: 'POST',
                translation: 'MGCP',
                path: '/tdstoosm'
            });
            var output = xml2js.parseString(trans2osm, function(err, result) {
                if (err) console.error(err);
                assert.equal(result.osm.node[0].tag[0].$.k, "use");
                assert.equal(result.osm.node[0].tag[0].$.v, "other");
                assert.equal(result.osm.node[0].tag[1].$.k, "place");
                assert.equal(result.osm.node[0].tag[1].$.v, "yes");
                assert.equal(result.osm.node[0].tag[2].$.k, "poi");
                assert.equal(result.osm.node[0].tag[2].$.v, "yes");
                assert.equal(result.osm.node[0].tag[3].$.k, "uuid");
                assert.equal(result.osm.node[0].tag[3].$.v, "{c6df0618-ce96-483c-8d6a-afa33541646c}");
                assert.equal(result.osm.node[0].tag[4].$.k, "name");
                assert.equal(result.osm.node[0].tag[4].$.v, "Manitou Springs");
                assert.equal(result.osm.node[0].tag[5].$.k, "landuse");
                assert.equal(result.osm.node[0].tag[5].$.v, "built_up_area");
                assert.equal(result.osm.node[0].tag[6].$.k, "source:accuracy:horizontal:category");
                assert.equal(result.osm.node[0].tag[6].$.v, "accurate");
            });
        });

        it('should untangle MGCP tags', function() {
            //http://localhost:8094/osmtotds
            var trans2osm = server.handleInputs({
                osm: '<osm version="0.6" upload="true" generator="hootenanny"><way id="-1" version="0"><nd ref="-1"/><nd ref="-4"/><nd ref="-7"/><nd ref="-10"/><nd ref="-1"/><tag k="FCODE" v="AL013"/><tag k="levels" v="3"/>/></way></osm>',
                method: 'POST',
                translation: 'MGCP',
                path: '/translateFrom'
            });
            xml2js.parseString(trans2osm, function(err, result) {
                if (err) console.error(err);
                assert.equal(result.osm.way[0].tag[0].$.k, "levels");
                assert.equal(result.osm.way[0].tag[0].$.v, "3");
                assert.equal(result.osm.way[0].tag[1].$.k, "building");
                assert.equal(result.osm.way[0].tag[1].$.v, "yes");
            });
        });

        it('should untangle TDSv61 tags', function() {
            //http://localhost:8094/osmtotds
            var trans2osm = server.handleInputs({
                osm: '<osm version="0.6" upload="true" generator="hootenanny"><way id="-1" version="0"><nd ref="-1"/><nd ref="-4"/><nd ref="-7"/><nd ref="-10"/><nd ref="-1"/><tag k="AL013" v="building"/><tag k="levels" v="3"/>/></way></osm>',
                method: 'POST',
                translation: 'TDSv61',
                path: '/translateFrom'
            });
            xml2js.parseString(trans2osm, function(err, result) {
                if (err) console.error(err);
                assert.equal(result.osm.way[0].tag[0].$.k, "levels");
                assert.equal(result.osm.way[0].tag[0].$.v, "3");
                assert.equal(result.osm.way[0].tag[1].$.k, "building");
                assert.equal(result.osm.way[0].tag[1].$.v, "yes");
            });
        });

        it('should untangle TDSv40 tags', function() {
            //http://localhost:8094/osmtotds
            var trans2osm = server.handleInputs({
                osm: '<osm version="0.6" upload="true" generator="hootenanny"><way id="-1" version="0"><nd ref="-1"/><nd ref="-4"/><nd ref="-7"/><nd ref="-10"/><nd ref="-1"/><tag k="AL013" v="building"/><tag k="levels" v="3"/>/></way></osm>',
                method: 'POST',
                translation: 'TDSv40',
                path: '/translateFrom'
            });
            xml2js.parseString(trans2osm, function(err, result) {
                if (err) console.error(err);
                assert.equal(result.osm.way[0].tag[0].$.k, "levels");
                assert.equal(result.osm.way[0].tag[0].$.v, "3");
                assert.equal(result.osm.way[0].tag[1].$.k, "building");
                assert.equal(result.osm.way[0].tag[1].$.v, "yes");
            });
        });
        it('should handle /taginfo/key/values GET with NO enums', function() {
            //http://localhost:8094/taginfo/key/values?fcode=AP030&filter=ways&key=SGCC&page=1&query=Clo&rp=25&sortname=count_ways&sortorder=desc&translation=TDSv61
            //http://localhost:8094/taginfo/key/values?fcode=AA040&filter=nodes&key=ZSAX_RX3&page=1&query=undefined&rp=25&sortname=count_nodes&sortorder=desc&translation=TDSv61
            var enums = server.handleInputs({
                fcode: 'AA040',
                filter: 'ways',
                key: 'ZSAX_RX3',
                page: '1',
                query: 'undefined',
                rp: '25',
                sortname: 'count_nodes',
                sortorder: 'desc',
                translation: 'TDSv61',
                method: 'GET',
                path: '/taginfo/key/values'
            });
            assert.equal(enums.data.length, 0);
        });

        it('should handle /taginfo/key/values GET with enums', function() {
            //http://localhost:8094/taginfo/key/values?fcode=AA040&filter=nodes&key=FUN&page=1&query=Damaged&rp=25&sortname=count_nodes&sortorder=desc&translation=MGCP
            var enums = server.handleInputs({
                fcode: 'AA040',
                filter: 'nodes',
                key: 'FUN',
                page: '1',
                query: 'Damaged',
                rp: '25',
                sortname: 'count_nodes',
                sortorder: 'desc',
                translation: 'MGCP',
                method: 'GET',
                path: '/taginfo/key/values'
            });
            assert.equal(enums.data.length, 7);
        });

        it('should handle /taginfo/keys/all GET with enums', function() {
            //http://localhost:8094/taginfo/keys/all?fcode=AA040&page=1&query=&rawgeom=Point&rp=10&sortname=count_nodes&sortorder=desc&translation=MGCP

            var enums = server.handleInputs({
                fcode: 'AA040',
                rawgeom: 'Point',
                key: 'FUN',
                page: '1',
                rp: '10',
                sortname: 'count_nodes',
                sortorder: 'desc',
                translation: 'MGCP',
                method: 'GET',
                path: '/taginfo/keys/all'
            });
            assert.equal(enums.data.length, 15);
        });

        it('should handle /taginfo/keys/all GET with enums', function() {

            var enums = server.handleInputs({
                fcode: 'EC030',
                rawgeom: 'Area',
                key: 'FUN',
                page: '1',
                rp: '10',
                sortname: 'count_ways',
                sortorder: 'desc',
                translation: 'MGCP',
                method: 'GET',
                path: '/taginfo/keys/all'
            });
            assert.equal(enums.data.length, 14);
        });

        it('should handle /capabilities GET', function() {

            var capas = server.handleInputs({
                method: 'GET',
                path: '/capabilities'
            });
            assert.equal(capas.TDSv61.isavailable, true);
            assert.equal(capas.TDSv40.isavailable, true);
            assert.equal(capas.MGCP.isavailable, true);
            assert.equal(capas.GGDMv30.isavailable, true);
        });

        it('should handle /schema GET', function() {
            //http://localhost:8094/schema?geometry=point&translation=MGCP&searchstr=Buil&maxlevdst=20&limit=12
            var schm = server.handleInputs({
                geometry: 'line',
                translation: 'TDSv40',
                searchstr: 'river',
                maxlevdst: 20,
                limit: 12,
                method: 'GET',
                path: '/schema'
            });
            assert.equal(schm[0].name, 'RIVER_C');
            assert.equal(schm[0].fcode, 'BH140');
            assert.equal(schm[0].desc, 'River');
        });

        //Checking the use of limit param
        it('should handle /schema GET', function() {
            var schm = server.handleInputs({
                geometry: 'line',
                translation: 'TDSv40',
                searchstr: 'river',
                maxlevdst: 10,
                limit: 1,
                method: 'GET',
                path: '/schema'
            });
            assert.equal(schm.length, 1);
        });

        it('should handle /schema GET', function() {
            var schm = server.handleInputs({
                geometry: 'line',
                translation: 'TDSv40',
                searchstr: 'ri',
                maxlevdst: 50,
                limit: 33,
                method: 'GET',
                path: '/schema'
            });
            assert.equal(schm.length, 33);
        });

        it('should handle /schema GET', function() {
            var schm = server.handleInputs({
                geometry: 'line',
                translation: 'TDSv40',
                searchstr: 'ri',
                maxlevdst: 50,
                limit: 100,
                method: 'GET',
                path: '/schema'
            });
            assert(schm.length <= 100, 'Schema search results greater than requested');
        });

        //Checking the use of limit param with no search string
        it('should handle /schema GET', function() {
            var schm = server.handleInputs({
                geometry: 'line',
                translation: 'TDSv40',
                searchstr: '',
                maxlevdst: 10,
                limit: 1,
                method: 'GET',
                path: '/schema'
            });
            assert.equal(schm.length, 1);
        });

        it('should handle /schema GET', function() {
            var schm = server.handleInputs({
                geometry: 'point',
                translation: 'TDSv61',
                searchstr: '',
                limit: 33,
                method: 'GET',
                path: '/schema'
            });
            assert.equal(schm.length, 33);
        });

        it('should handle /schema GET', function() {
            var schm = server.handleInputs({
                geometry: 'line',
                translation: 'MGCP',
                searchstr: '',
                maxlevdst: 0, //This shouldn't be used when searchstr is ''
                limit: 100,
                method: 'GET',
                path: '/schema'
            });
            assert(schm.length <= 100, 'Schema search results greater than requested');
            assert(schm.some(function(d) {
                return d.desc === 'Railway';
            }));
        });

        it('should handle /schema GET', function() {
            var schm = server.handleInputs({
                geometry: 'line',
                translation: 'GGDMv30',
                searchstr: '',
                maxlevdst: 0, //This shouldn't be used when searchstr is ''
                limit: 100,
                method: 'GET',
                path: '/schema'
            });
            assert(schm.length <= 100, 'Schema search results greater than requested');
            assert(schm.some(function(d) {
                return d.desc === 'Railway';
            }));
        });

        it('throws error if url not found', function() {
            assert.throws(function error() {
                server.handleInputs({
                    idval: 'AL015',
                    geom: 'Point',
                    translation: 'TDSv40',
                    idelem: 'fcode',
                    method: 'GET',
                    path: '/foo'
                })
            }, Error, 'Not found');
        });

        it('throws error if unsupported method', function() {
            assert.throws(function error() {
                server.handleInputs({
                    method: 'POST',
                    path: '/schema'
                })
            }, Error, 'Unsupported method');
        });

        it('throws error if unsupported method', function() {
            assert.throws(function error() {
                server.handleInputs({
                    method: 'POST',
                    path: '/taginfo/key/values'
                })
            }, Error, 'Unsupported method');
        });

        it('throws error if unsupported method', function() {
            assert.throws(function error() {
                server.handleInputs({
                    method: 'POST',
                    path: '/taginfo/keys/all'
                })
            }, Error, 'Unsupported method');
        });

        it('throws error if unsupported method', function() {
            assert.throws(function error() {
                server.handleInputs({
                    method: 'POST',
                    path: '/capabilities'
                })
            }, Error, 'Unsupported method');
        });
     });

    describe('capabilities', function () {
      it('should return 200', function (done) {
        var request  = httpMocks.createRequest({
            method: 'GET',
            url: '/capabilities'
        });
        var response = httpMocks.createResponse();
        server.TranslationServer(request, response);
        assert.equal(response.statusCode, '200');
        done();
      });
    });

    describe('schema', function () {
      it('should return 200', function (done) {
        var request  = httpMocks.createRequest({
            method: 'GET',
            url: '/schema',
            params: {
                geometry: 'point',
                translation: 'MGCP',
                searchstr: 'Buil',
                maxlevdst: 20,
                limit: 12
            }
        });
        var response = httpMocks.createResponse();
        server.TranslationServer(request, response);
        assert.equal(response.statusCode, '200');
        done();
      });
    });

    describe('osmtotds', function () {
      it('should return 200', function (done) {
        var request  = httpMocks.createRequest({
            method: 'GET',
            url: '/osmtotds',
            params: {
                idval: 'AP030',
                translation: 'MGCP',
                geom: 'Line',
                idelem: 'fcode'
            }
        });
        var response = httpMocks.createResponse();
        server.TranslationServer(request, response);
        assert.equal(response.statusCode, '200');
        done();
      });
    });

    describe('translateFrom', function () {
      it('should return 200', function (done) {
        var request  = httpMocks.createRequest({
            method: 'GET',
            url: '/translateFrom',
            params: {
                fcode: 'AL013',
                translation: 'TDSv61'
            }
        });
        var response = httpMocks.createResponse();
        server.TranslationServer(request, response);
        assert.equal(response.statusCode, '200');
        done();
      });
    });

    describe('translateTo', function () {
      it('should return 200', function (done) {
        var request  = httpMocks.createRequest({
            method: 'GET',
            url: '/translateFrom',
            params: {
                idelem: 'fcode',
                idval: 'AL013',
                geom: 'Area',
                translation: 'MGCP'
            }
        });
        var response = httpMocks.createResponse();
        server.TranslationServer(request, response);
        assert.equal(response.statusCode, '200');
        done();
      });
    });

    describe('taginfo/key/values', function () {
      it('should return 200', function (done) {
        var request  = httpMocks.createRequest({
            method: 'GET',
            url: '/taginfo/key/values',
            params: {
                fcode: 'AP030',
                translation: 'MGCP',
                filter: 'ways',
                key: 'LTN',
                page: '1',
                query: 'Clo',
                rp: '25',
                sortname: 'count_ways',
                sortorder: 'desc'
            }
        });
        var response = httpMocks.createResponse();
        server.TranslationServer(request, response);
        assert.equal(response.statusCode, '200');
        done();
      });
    });

    describe('taginfo/keys/all', function () {
      it('should return 200', function (done) {
        var request  = httpMocks.createRequest({
            method: 'GET',
            url: '/taginfo/keys/all',
            params: {
                fcode: 'AP030',
                rawgeom: 'Line',
                translation: 'MGCP',
                page: 1,
                rp: 10,
                sortname: 'count_ways',
                sortorder: 'desc'
            }
        });
        var response = httpMocks.createResponse();
        server.TranslationServer(request, response);
        assert.equal(response.statusCode, '200');
        done();
      });
    });

    describe('not found', function () {
      it('should return 404', function (done) {
        var request  = httpMocks.createRequest({
            method: 'GET',
            url: '/foo',
            params: {
                fcode: 'AL013',
                translation: 'TDSv61'
            }
        });
        var response = httpMocks.createResponse();
        server.TranslationServer(request, response);
        assert.equal(response.statusCode, '404');
        done();
      });
    });
});

