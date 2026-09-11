#include "io/SceneFile.h"
#include "scene/SceneHistory.h"
#include <QTemporaryDir>
#include <cstdio>

int main() {
    int failures=0;
    auto check=[&](bool okay,const char* name) { if (!okay) { ++failures; std::fprintf(stderr,"FAIL: %s\n",name); } };
    n3d::Scene scene;
    n3d::ObjectData box; box.name="中文名称 \"测试\"\n"; box.positionMm={-123.25f,0,875.5f}; box.rotationDegrees={360015,-45,12};
    const auto large=n3d::ObjectId(9007199254740993ULL);
    scene.restore({large,box},0);
    auto cylinder=box; cylinder.shape=n3d::CylinderParameters{650.25f,900}; cylinder.name="圆柱";
    const auto cylinderId=scene.add(cylinder);
    const auto removed=scene.add(box); scene.remove(removed);
    const auto encoded=SceneFile::encode(scene);
    const auto decoded=SceneFile::decode(encoded);
    check(decoded.objects().size()==2 && decoded.objects()[0].id==large && decoded.find(large)->data==box
          && decoded.find(cylinderId)->data==cylinder && decoded.nextObjectId()==scene.nextObjectId(),"exact Unicode, float, large ID and allocator round trip");
    check(SceneFile::decode(SceneFile::encode(n3d::Scene{})).objects().empty(),"empty scene round trip");
    auto rejected=[&](const QByteArray& bytes,const char* label) {
        try { SceneFile::decode(bytes); check(false,label); } catch(const std::exception&) {}
    };
    rejected("not JSON","malformed JSON"); rejected("[]","array root");
    const auto baseline=QJsonDocument::fromJson(encoded).object();
    auto changed=[&](const char* field,QJsonValue value) { auto root=baseline; root[field]=value; return QJsonDocument(root).toJson(); };
    rejected(changed("version",2),"future version"); rejected(changed("version","1"),"version type");
    rejected(changed("units","m"),"wrong units"); rejected(changed("nextObjectId","1"),"allocator collision");
    rejected(changed("objects",QJsonValue()),"missing objects array"); rejected(changed("extra",1),"unknown root field");
    for (const auto invalid:QJsonArray{0,-1,true,"123",QJsonValue()}) {
        auto root=baseline; auto objects=root["objects"].toArray(); auto object=objects[0].toObject();
        auto shape=object["shape"].toObject(); shape["width"]=invalid; object["shape"]=shape; objects[0]=object; root["objects"]=objects;
        rejected(QJsonDocument(root).toJson(),"invalid dimension");
    }
    for (const auto pair:std::initializer_list<std::pair<QString,QJsonValue>>{{"id",double(large)},{"id","0"},{"positionMm",QJsonArray{0,1}},{"rotationDegrees",QJsonArray{0,0,"1"}},{"shape",QJsonObject{{"type","sphere"}}}}) {
        auto root=baseline; auto objects=root["objects"].toArray(); auto object=objects[0].toObject();
        object[pair.first]=pair.second; objects[0]=object; root["objects"]=objects; rejected(QJsonDocument(root).toJson(),"invalid object field");
    }
    auto root=baseline; auto objects=root["objects"].toArray(); objects.append(objects[0]); root["objects"]=objects;
    rejected(QJsonDocument(root).toJson(),"duplicate ID");
    rejected(QByteArray(SceneFile::maxBytes+1,' '),"oversize input");
    n3d::Scene thousand; for (int i=0;i<1000;++i) thousand.add(i%2?box:cylinder);
    check(SceneFile::decode(SceneFile::encode(thousand)).objects().size()==1000,"1000 object serialization");
    QTemporaryDir directory;
    check(directory.isValid(),"temporary file directory");
    const auto path=directory.filePath(QStringLiteral("中文工程.n3d"));
    SceneFile::write(path,scene);
    check(SceneFile::read(path).find(large)->data==box,"Unicode file path");
    auto bad=scene; auto badName=box; badName.name=std::string(4097,'x'); bad.update(large,badName);
    try { SceneFile::write(path,bad); check(false,"invalid output rejected"); } catch(const std::exception&) {}
    check(SceneFile::read(path).find(large)->data==box,"failed encoding leaves original file intact");
    try { SceneFile::write(directory.path(),scene); check(false,"directory output rejected"); } catch(const std::exception&) {}
    try { SceneFile::read(directory.filePath("missing.n3d")); check(false,"missing file rejected"); } catch(const std::exception&) {}
    n3d::SceneHistory history; n3d::Scene historyScene; const auto id=historyScene.add(box);
    check(history.isClean(),"new history clean");
    history.recordApplied({"create",id,{},box,0,0,id}); history.markClean();
    history.undo(historyScene); check(!history.isClean(),"undo before save is dirty");
    history.redo(historyScene); check(history.isClean(),"redo to save is clean");
    history.undo(historyScene); const auto branch=historyScene.add(cylinder);
    history.recordApplied({"create",branch,{},cylinder,0,0,branch});
    check(!history.isClean(),"same numeric cursor on new branch is not saved state");
    history.clear(); check(history.isClean(),"clear resets save point");
    if (!failures) std::puts("PASS: scene file codec, IO failures, history save points");
    return failures?1:0;
}
