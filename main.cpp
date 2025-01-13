#include <iostream>
#include <string>
#include <gdal_priv.h>
#include <crow.h>
#include "cpl_port.h"
#include "Cpl.h"
#include "cpl_error.h"
#include <gdal.h>
#include <ogrsf_frmts.h>
#include <geos/geom/GeometryFactory.h>
#include <geos/io/WKTReader.h>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

static void ReportHiearchicalLayers(CPLJSONObject &oRoot,
                                    const GDALGroup *group,
                                    const std::string &indent, bool bGeomType) {
    const auto aosVectorLayerNames = group->GetVectorLayerNames();
    CPLJSONArray oLayerNames;
    oRoot.Add("layerNames", oLayerNames);
    for (const auto &osVectorLayerName: aosVectorLayerNames) {
        OGRLayer *poLayer = group->OpenVectorLayer(osVectorLayerName);
        if (poLayer) {
            CPLJSONObject oLayer;
            oLayer.Add("name", poLayer->GetName());
            oLayer.Add("type", OGRToOGCGeomType(poLayer->GetGeomType()));
            oLayer.Add("alias", poLayer->GetMetadataItem("ALIAS_NAME"));
            oLayer.Add("count", poLayer->GetFeatureCount());
            oLayerNames.Add(oLayer);
        }
    }

    const std::string subIndent(indent + "  ");
    auto aosSubGroupNames = group->GetGroupNames();
    CPLJSONArray oGroupArray;
    oRoot.Add("groups", oGroupArray);
    for (const auto &osSubGroupName: aosSubGroupNames) {
        auto poSubGroup = group->OpenGroup(osSubGroupName);
        if (poSubGroup) {
            CPLJSONObject oGroup;

            oGroupArray.Add(oGroup);
            oGroup.Set("name", osSubGroupName);
            ReportHiearchicalLayers(oGroup, poSubGroup.get(),
                                    subIndent, bGeomType);
        }
    }
}


std::string GetLayerInfo(const std::string &gdbPath) {
    GDALAllRegister();
    const char *projLibPath = "F:/github/vcpkg_dev/packages/proj_x64-windows/share/proj";
    const char *gdalData = "F:/github/vcpkg_dev/packages/gdal_x64-windows/share/gdal";
    CPLSetConfigOption("PROJ_LIB", projLibPath);
    CPLSetConfigOption("GDAL_DATA", gdalData);
    // 检查驱动是否可用
    if (GDALGetDriverByName("OpenFileGDB") == nullptr) {
        return "OpenFileGDB driver not available.";
    }

    auto factory = geos::geom::GeometryFactory::create();
    geos::io::WKTReader wktReader(factory.get());

    char **papszAllowedDrivers = nullptr;
    papszAllowedDrivers = CSLAddString(papszAllowedDrivers, "OpenFileGDB");

    GDALDataset *poDS = GDALDataset::Open(gdbPath.c_str(), GDAL_OF_VECTOR, papszAllowedDrivers, nullptr, nullptr);
    if (poDS == nullptr) {
        return std::string("Failed to open GDB: ") + CPLGetLastErrorMsg();
    }
    //图层目录
    CPLJSONObject oRoot;
    CPLJSONObject oGroup;
    oRoot.Add("rootGroup", oGroup);
    auto poRootGroup = poDS->GetRootGroup();
    if (poRootGroup &&
        (!poRootGroup->GetGroupNames().empty() ||
         !poRootGroup->GetVectorLayerNames().empty())) {
        CPLJSONObject oGroup;
        oRoot.Add("rootGroup", oGroup);
        ReportHiearchicalLayers(oGroup,
                                poRootGroup.get(), std::string(),
                                true);
    }

    std::cout << "Closing dataset..." << std::endl;
    GDALClose(poDS);
    std::cout << "Dataset closed successfully." << std::endl;

    return oRoot.ToString();

    /*
    for (int i = 0; i < poDS->GetLayerCount(); ++i) {
        OGRLayer *poLayer = poDS->GetLayer(i);
        if (poLayer == nullptr) {
            GDALClose(poDS);
            return "Layer 'FZJZSSM' not found.";
        }

        char **metadata = poLayer->GetMetadata("");
        if (metadata != nullptr) {
            for (int i = 0; metadata[i] != nullptr; ++i) {
                std::cout << "Metadata: " << metadata[i] << std::endl;
            }
        }
     
        OGRFeature *feature;
        while ((feature = poLayer->GetNextFeature()) != nullptr) {
            OGRGeometry *ogrGeom = feature->GetGeometryRef();
            // 转换 OGRGeometry 为 WKT
            char *wkt = nullptr;
            ogrGeom->exportToWkt(&wkt);
            auto geom = wktReader.read(wkt);
            double length = geom->getLength();
            //std::cout << length << std::endl;
            CPLFree(wkt);
            OGRFeature::DestroyFeature(feature);
        }
    }
    */
}

int main() {
    // Start Crow HTTP server
    crow::SimpleApp app;

    CROW_ROUTE(app, "/get_layer_info").methods(crow::HTTPMethod::POST)([](const crow::request &req) {
        // Parse the input JSON
        auto body = crow::json::load(req.body);
        if (!body || !body.has("gdb_path")) {
            return crow::response(400, "Invalid request. Please provide 'gdb_path' in the JSON body.");
        }

        // Get the GDB path from the request
        std::string gdbPath = body["gdb_path"].s();

        // Get layer information
        std::string layerInfo = GetLayerInfo(gdbPath);
        rapidjson::Document document;
        document.Parse(layerInfo.c_str());
        if (document.HasParseError()) {
            return crow::response(400, "Invalid JSON");
        }
        rapidjson::StringBuffer buffer;
        rapidjson::Writer writer(buffer);
        document.Accept(writer);
        crow::response res;
        res.set_header("Content-Type", "application/json");
        res.write(buffer.GetString());
        return res;
    });

    std::cout << "Server is running on http://127.0.0.1:18080\n";
    app.port(18080).multithreaded().run();

    return 0;
}
