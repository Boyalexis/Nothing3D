#pragma once
#include "scene/Scene.h"
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

// File adapter: Qt JSON/IO types stop here; Scene remains ordinary C++ data.
namespace SceneFile {
inline constexpr qint64 maxBytes=32*1024*1024;
inline constexpr qsizetype maxObjects=10000;
inline void require(bool valid,const char* message) {
    if (!valid) throw std::runtime_error(message);
}
inline void keys(const QJsonObject& object,std::initializer_list<const char*> names) {
    require(object.size()==qsizetype(names.size()),"字段缺失或含有不支持的字段");
    for (const auto* name:names) require(object.contains(QLatin1String(name)),"缺少必需字段");
}
inline n3d::ObjectId id(const QJsonValue& value) {
    require(value.isString(),"对象编号必须使用十进制字符串");
    bool okay=false;
    const auto result=value.toString().toULongLong(&okay);
    require(okay && result>0 && QString::number(result)==value.toString(),"对象编号无效");
    return result;
}
inline float number(const QJsonValue& value,double low,double high) {
    require(value.isDouble(),"尺寸和变换必须是数值");
    const double result=value.toDouble();
    require(std::isfinite(result) && result>=low && result<=high,"尺寸或变换超出允许范围");
    return float(result);
}
inline QJsonArray vector(n3d::Vector3 value) { return {double(value.x),double(value.y),double(value.z)}; }
inline n3d::Vector3 vector(const QJsonValue& value,double bound) {
    require(value.isArray() && value.toArray().size()==3,"位置和旋转必须包含三个分量");
    const auto a=value.toArray();
    return {number(a[0],-bound,bound),number(a[1],-bound,bound),number(a[2],-bound,bound)};
}
inline QByteArray encode(const n3d::Scene& scene) {
    require(scene.objects().size()<=size_t(maxObjects),"当前文件格式最多支持 10000 个对象");
    QJsonArray objects;
    for (const auto& object:scene.objects()) {
        const auto& data=object.data;
        const auto name=QString::fromStdString(data.name);
        require(name.toUtf8().toStdString()==data.name && data.name.size()<=4096,"对象名称必须为有效 UTF-8 且不超过 4096 字节");
        QJsonObject shape;
        if (const auto* box=std::get_if<n3d::BoxParameters>(&data.shape))
            shape={{"type","box"},{"width",box->width},{"height",box->height},{"depth",box->depth}};
        else {
            const auto& cylinder=std::get<n3d::CylinderParameters>(data.shape);
            shape={{"type","cylinder"},{"diameter",cylinder.diameter},{"height",cylinder.height}};
        }
        objects.append(QJsonObject{{"id",QString::number(object.id)},{"name",name},{"shape",shape},
                                  {"positionMm",vector(data.positionMm)},{"rotationDegrees",vector(data.rotationDegrees)}});
    }
    const auto bytes=QJsonDocument(QJsonObject{{"format","Nothing3D"},{"version",1},{"units","mm"},
        {"nextObjectId",QString::number(scene.nextObjectId())},{"objects",objects}}).toJson(QJsonDocument::Indented);
    require(bytes.size()<=maxBytes,"工程文件超过 32 MiB 限制");
    return bytes;
}
inline n3d::Scene decode(const QByteArray& bytes) {
    require(bytes.size()<=maxBytes,"工程文件超过 32 MiB 限制");
    QJsonParseError error;
    const auto document=QJsonDocument::fromJson(bytes,&error);
    require(error.error==QJsonParseError::NoError && document.isObject(),"不是有效的 JSON 工程文件");
    const auto root=document.object(); keys(root,{"format","version","units","nextObjectId","objects"});
    require(root["format"]==QJsonValue("Nothing3D"),"不是 Nothing3D 工程");
    require(root["version"].isDouble() && root["version"].toDouble()==1,"不支持此工程格式版本");
    require(root["units"]==QJsonValue("mm"),"仅支持毫米单位");
    require(root["objects"].isArray(),"对象列表必须是数组");
    const auto objects=root["objects"].toArray();
    require(objects.size()<=maxObjects,"当前文件格式最多支持 10000 个对象");
    n3d::Scene scene;
    for (const auto& entry:objects) {
        require(entry.isObject(),"对象条目无效");
        const auto object=entry.toObject(); keys(object,{"id","name","shape","positionMm","rotationDegrees"});
        n3d::ObjectData data;
        require(object["name"].isString() && object["name"].toString().toUtf8().size()<=4096,"对象名称无效或过长");
        data.name=object["name"].toString().toStdString();
        require(object["shape"].isObject(),"基本体参数无效");
        const auto shape=object["shape"].toObject();
        if (shape["type"]==QJsonValue("box")) {
            keys(shape,{"type","width","height","depth"});
            data.shape=n3d::BoxParameters{number(shape["width"],0,1e7),number(shape["height"],0,1e7),number(shape["depth"],0,1e7)};
        } else if (shape["type"]==QJsonValue("cylinder")) {
            keys(shape,{"type","diameter","height"});
            data.shape=n3d::CylinderParameters{number(shape["diameter"],0,1e7),number(shape["height"],0,1e7)};
        } else throw std::runtime_error("不支持此基本体类型");
        data.positionMm=vector(object["positionMm"],1e9);
        data.rotationDegrees=vector(object["rotationDegrees"],std::numeric_limits<float>::max());
        try { scene.restore({id(object["id"]),data},scene.objects().size()); }
        catch (const std::invalid_argument&) { throw std::runtime_error("对象编号重复、编号无效或基本体参数无效"); }
    }
    try { scene.reserveObjectIdsUntil(id(root["nextObjectId"])); }
    catch (const std::invalid_argument&) { throw std::runtime_error("后续对象编号与已有编号冲突"); }
    return scene;
}
inline n3d::Scene read(const QString& path) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly),"无法读取工程文件");
    require(file.size()<=maxBytes,"工程文件超过 32 MiB 限制");
    const auto bytes=file.read(maxBytes+1);
    require(file.error()==QFileDevice::NoError,"读取工程文件失败");
    return decode(bytes);
}
inline void write(const QString& path,const n3d::Scene& scene) {
    const auto bytes=encode(scene); // Validate before creating any output.
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    require(file.open(QIODevice::WriteOnly),"无法写入此位置，请检查目录或文件权限");
    require(file.write(bytes)==bytes.size(),"工程写入失败，原文件未替换");
    require(file.commit(),"工程提交失败，原文件未替换");
}
}
